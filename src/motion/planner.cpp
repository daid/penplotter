/*
  planner.c - buffers movement commands and manages the acceleration profile plan
 Part of Grbl

 Copyright (c) 2009-2011 Simen Svale Skogsrud

 Grbl is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Grbl is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
 */

/* The ring buffer implementation gleaned from the wiring_serial library by David A. Mellis. */

/*
 Reasoning behind the mathematics in this module (in the key of 'Mathematica'):

 s == speed, a == acceleration, t == time, d == distance

 Basic definitions:

 Speed[s_, a_, t_] := s + (a*t)
 Travel[s_, a_, t_] := Integrate[Speed[s, a, t], t]

 Distance to reach a specific speed with a constant acceleration:

 Solve[{Speed[s, a, t] == m, Travel[s, a, t] == d}, d, t]
 d -> (m^2 - s^2)/(2 a) --> estimate_acceleration_distance()

 Speed after a given distance of travel with constant acceleration:

 Solve[{Speed[s, a, t] == m, Travel[s, a, t] == d}, m, t]
 m -> Sqrt[2 a d + s^2]

 DestinationSpeed[s_, a_, d_] := Sqrt[2 a d + s^2]

 When to start braking (di) to reach a specified destination speed (s2) after accelerating
 from initial speed s1 without ever stopping at a plateau:

 Solve[{DestinationSpeed[s1, a, di] == DestinationSpeed[s2, a, d - di]}, di]
 di -> (2 a d - s1^2 + s2^2)/(4 a) --> intersection_distance()

 IntersectionDistance[s1_, s2_, a_, d_] := (2 a d - s1^2 + s2^2)/(4 a)
 */

#include "planner.h"
#include "plannerConfig.h"
#include "arch/stepperMotor.h"

#include <math.h>
#include <string.h>
#include <algorithm>

//For some reason the avrlibc square function crashes, so we supply our own.
#define square(n) ((n)*(n))

//===========================================================================
//=============================public variables ============================
//===========================================================================

float max_feedrate[OUTPUT_AXIS_COUNT] = DEFAULT_MAX_FEEDRATE; // set the max speeds
unsigned long max_acceleration_units_per_sq_second[OUTPUT_AXIS_COUNT] = DEFAULT_MAX_ACCELERATION; // Use M201 to override by software
float minimumfeedrate = 0;
float max_xy_jerk = DEFAULT_XYJERK; //speed than can be stopped at once, if i understand correctly.
float max_z_jerk = DEFAULT_ZJERK;
unsigned long axis_steps_per_sqr_second[OUTPUT_AXIS_COUNT];

// The current position of the tool in absolute steps
static long final_step_position[OUTPUT_AXIS_COUNT];
static float previous_speed[OUTPUT_AXIS_COUNT];  // Speed of previous path line segment
static float previous_nominal_speed;    // Nominal speed of previous path line segment

//===========================================================================
//=================semi-private variables, used in inline  functions    =====
//===========================================================================
block_t block_buffer[BLOCK_BUFFER_SIZE];            // A ring buffer for motion instructions. Note that maximum number of planned motions is this number-1 since we stop planning when next_head==tail.
volatile uint8_t block_buffer_head;                 // Index of the next block to be pushed
volatile uint8_t block_buffer_tail;                 // Index of the block to process now

//===========================================================================
//=============================private variables ============================
//===========================================================================
static uint8_t moves_planned(); //return the nr of buffered moves

// Returns the index of the next block in the ring buffer
// NOTE: Removed modulo (%) operator, which uses an expensive divide and multiplication.
static int8_t next_block_index(int8_t block_index)
{
    block_index++;
    if (block_index == BLOCK_BUFFER_SIZE)
        block_index = 0;
    return block_index;
}


// Returns the index of the previous block in the ring buffer
static int8_t prev_block_index(int8_t block_index)
{
    if (block_index == 0)
        block_index = BLOCK_BUFFER_SIZE;
    block_index--;
    return block_index;
}

//===========================================================================
//=============================functions         ============================
//===========================================================================

// Calculates the distance (not time) it takes to accelerate from initial_rate to target_rate using the
// given acceleration:
static inline float estimate_acceleration_distance(float initial_rate, float target_rate, float acceleration_st)
{
    if(acceleration_st == 0)
        return 0.0;  // acceleration was 0, set acceleration distance to 0
    return (target_rate*target_rate-initial_rate*initial_rate) / (2.0*acceleration_st);
}

// This function gives you the point at which you must start braking (at the rate of -acceleration) if
// you started at speed initial_rate and accelerated until this point and want to end at the final_rate after
// a total travel of distance. This can be used to compute the intersection point between acceleration and
// deceleration in the cases where the trapezoid has no plateau (i.e. never reaches maximum speed)

static inline float intersection_distance(float initial_rate, float final_rate, float acceleration_st, float distance)
{
    if(acceleration_st == 0)
        return 0.0;  // acceleration was 0, set intersection distance to 0
    return (2.0*acceleration_st*distance-initial_rate*initial_rate+final_rate*final_rate) / (4.0*acceleration_st);
}

// Calculates trapezoid parameters so that the entry- and exit-speed is compensated by the provided factors.

void calculate_trapezoid_for_block(block_t* const block, const float entry_factor, const float exit_factor)
{
    uint32_t initial_rate = ceil(block->nominal_rate * entry_factor); // (step/min)
    uint32_t final_rate = ceil(block->nominal_rate * exit_factor); // (step/min)

    // Limit minimal step rate (Otherwise the timer will overflow.)
    if(initial_rate < 120)
        initial_rate = 120;
    if(final_rate < 120)
        final_rate=120;

    const int32_t acceleration_st = block->acceleration_st;

    // Steps required for acceleration, deceleration to/from nominal rate.
    int32_t accelerate_steps = ceil(estimate_acceleration_distance(initial_rate, block->nominal_rate, acceleration_st));
    int32_t decelerate_steps = floor(estimate_acceleration_distance(block->nominal_rate, final_rate, -acceleration_st));

    // Steps between acceleration and deceleration, if any.
    int32_t plateau_steps = block->step_event_count-accelerate_steps-decelerate_steps;

    // Does accelerate_steps + decelerate_steps exceed step_event_count?
    // Then we can't possibly reach the nominal rate, i.e. there will be no cruising.
    // Use intersection_distance() to calculate accel / braking time in order to
    // reach the final_rate exactly at the end of this block.
    if (plateau_steps < 0)
    {
        accelerate_steps = ceil(intersection_distance(initial_rate, final_rate, acceleration_st, block->step_event_count));
        accelerate_steps = std::max(accelerate_steps, (int32_t)0); // Check limits due to numerical round-off
        accelerate_steps = std::min((uint32_t)accelerate_steps, (uint32_t)block->step_event_count);//(We can cast here to unsigned, because the above line ensures that we are above zero)
        plateau_steps = 0;
    }

    {
        stepper_motors_interrupt_disable();
        // Fill variables used by the stepper in a critical section
        if(!block->busy) // Don't update variables if block is busy.
        {
            block->accelerate_until = accelerate_steps;
            block->decelerate_after = accelerate_steps+plateau_steps;
            block->initial_rate = initial_rate;
            block->final_rate = final_rate;
        }
        stepper_motors_interrupt_enable();
    }
}

// Calculates the maximum allowable speed at this point when you must be able to reach target_velocity using the
// acceleration within the allotted distance.
static inline float max_allowable_speed(float acceleration, float target_velocity, float distance)
{
    return sqrt(target_velocity*target_velocity-2*acceleration*distance);
}

// The kernel called by planner_recalculate() when scanning the plan from last to first entry.
void planner_reverse_pass_kernel(block_t *previous, block_t *current, block_t *next)
{
    (void)previous;
    if(!current)
        return;

    if (next)
    {
        // If entry speed is already at the maximum entry speed, no need to re-check. Block is cruising.
        // If not, block in state of acceleration or deceleration. Reset entry speed to maximum and
        // check for maximum allowable speed reductions to ensure maximum possible planned speed.
        if (current->entry_speed != current->max_entry_speed)
        {
            // If nominal length true, max junction speed is guaranteed to be reached. Only compute
            // for max allowable speed if block is decelerating and nominal length is false.
            if ((!current->nominal_length_flag) && (current->max_entry_speed > next->entry_speed))
            {
                current->entry_speed = std::min(current->max_entry_speed, max_allowable_speed(-current->acceleration,next->entry_speed,current->millimeters));
            }
            else
            {
                current->entry_speed = current->max_entry_speed;
            }
            current->recalculate_flag = true;
        }
    } // Skip last block. Already initialized and set for recalculation.
}

// planner_recalculate() needs to go over the current plan twice. Once in reverse and once forward. This
// implements the reverse pass.
// The reverse pass adjusts the entry speeds when deceleration does not fit within one move.
void planner_reverse_pass()
{
    uint8_t block_index = block_buffer_head;
    uint8_t tail;

    //Make a local copy of block_buffer_tail, because the interrupt can alter it
    {
        stepper_motors_interrupt_disable();
        tail = block_buffer_tail;
        stepper_motors_interrupt_enable();
    }

    // When we have 3 or more moves in the planner buffer, then ...
    if (((block_buffer_head - tail + BLOCK_BUFFER_SIZE) & (BLOCK_BUFFER_SIZE - 1)) > 3)
    {
        block_index = (block_buffer_head - 3) & (BLOCK_BUFFER_SIZE - 1);
        block_t *block[3] = {NULL, NULL, NULL};

        // Loop for all blocks in the planner buffer. Process in segments of 2 blocks: current and next.
        while(block_index != tail)
        {
            block_index = prev_block_index(block_index);
            block[2]= block[1];
            block[1]= block[0];
            block[0] = &block_buffer[block_index];
            planner_reverse_pass_kernel(block[0], block[1], block[2]);
        }
    }
}

// The kernel called by planner_recalculate() when scanning the plan from first to last entry.
void planner_forward_pass_kernel(block_t *previous, block_t *current, block_t *next)
{
    (void)next;
    if(!previous)
        return;

    // If the previous block is an acceleration block, but it is not long enough to complete the
    // full speed change within the block, we need to adjust the entry speed accordingly. Entry
    // speeds have already been reset, maximized, and reverse planned by reverse planner.
    // If nominal length is true, max junction speed is guaranteed to be reached. No need to re-check.
    if (!previous->nominal_length_flag)
    {
        if (previous->entry_speed < current->entry_speed)
        {
            float entry_speed = std::min(current->entry_speed, max_allowable_speed(-previous->acceleration, previous->entry_speed, previous->millimeters));

            // Check for junction speed change
            if (current->entry_speed != entry_speed)
            {
                current->entry_speed = entry_speed;
                current->recalculate_flag = true;
            }
        }
    }
}

// planner_recalculate() needs to go over the current plan twice. Once in reverse and once forward. This
// implements the forward pass.
// This adjusts the entry speeds when acceleration does not fit within one move.
void planner_forward_pass()
{
    uint8_t block_index = block_buffer_tail;
    block_t *block[3] = {NULL, NULL, NULL};

    // Loop for all blocks in the planner buffer. Process in segments of 2 blocks: previous and current.
    while(block_index != block_buffer_head)
    {
        block[0] = block[1];
        block[1] = block[2];
        block[2] = &block_buffer[block_index];
        planner_forward_pass_kernel(block[0], block[1], block[2]);
        block_index = next_block_index(block_index);
    }
    planner_forward_pass_kernel(block[1], block[2], NULL);
}

// Recalculates the trapezoid speed profiles for all blocks in the plan according to the
// entry_factor for each junction. Must be called by planner_recalculate() after
// updating the blocks.
void planner_recalculate_trapezoids()
{
    int8_t block_index = block_buffer_tail;
    block_t *current;
    block_t *next = NULL;

    while(block_index != block_buffer_head)
    {
        current = next;
        next = &block_buffer[block_index];
        if (current)
        {
            // Recalculate if current block entry or exit junction speed has changed.
            if (current->recalculate_flag || next->recalculate_flag)
            {
                // NOTE: Entry and exit factors always > 0 by all previous logic operations.
                calculate_trapezoid_for_block(current, current->entry_speed/current->nominal_speed, next->entry_speed/current->nominal_speed);
                current->recalculate_flag = false; // Reset current only to ensure next trapezoid is computed
            }
        }
        block_index = next_block_index( block_index );
    }
    // Last/newest block in buffer. Exit speed is set with MINIMUM_PLANNER_SPEED. Always recalculated.
    if(next != NULL)
    {
        calculate_trapezoid_for_block(next, next->entry_speed/next->nominal_speed, MINIMUM_PLANNER_SPEED/next->nominal_speed);
        next->recalculate_flag = false;
    }
}

// Recalculates the motion plan according to the following algorithm:
//
//   1. Go over every block in reverse order and calculate a junction speed reduction (i.e. block_t.entry_factor)
//      so that:
//     a. The junction jerk is within the set limit
//     b. No speed reduction within one block requires faster deceleration than the one, true constant
//        acceleration.
//   2. Go over every block in chronological order and dial down junction speed reduction values if
//     a. The speed increase within one block would require faster acceleration than the one, true
//        constant acceleration.
//
// When these stages are complete all blocks will have an entry_factor allowing all speed changes to
// be performed using only the one, true constant acceleration, and where no junction jerk is jerkier than
// the set limit. Finally it will:
//
//   3. Recalculate trapezoids for all blocks.

void planner_recalculate()
{
    planner_reverse_pass();     // Adjust the entry speeds when deceleration does not fit within one move.
    planner_forward_pass();     // Adjust the entry speeds when acceleration does not fit within one move.
    planner_recalculate_trapezoids();
}

void planner_init()
{
    block_buffer_head = 0;
    block_buffer_tail = 0;
    memset(final_step_position, 0, sizeof(final_step_position)); // clear position
    memset(previous_speed, 0, sizeof(previous_speed));
    previous_nominal_speed = 0.0;
    reset_acceleration_rates();
}

// Add a new linear movement to the buffer.
bool planner_buffer_line(const float (&position)[INPUT_AXIS_COUNT], float feed_rate, float acceleration)
{
    // Calculate the buffer head after we push this byte
    int8_t next_buffer_head = next_block_index(block_buffer_head);

    // If the buffer is full: good! That means we are well ahead of the robot.
    // Rest here until there is room in the buffer.
    if(block_buffer_tail == next_buffer_head)
        return false;

    // The target position of the tool in absolute steps.
    // Calculate target position in absolute steps.
    // This should be done after the wait, because otherwise a M92 code within the gcode disrupts this calculation somehow.
    long target_step_position[OUTPUT_AXIS_COUNT];
    planner_position_to_steps(position, target_step_position);

    // Prepare to set up new block
    block_t *block = &block_buffer[block_buffer_head];

    // Mark block as not busy (Not executed by the stepper interrupt)
    block->busy = false;

    block->step_event_count = 0;
    block->direction_bits = 0;
    for(uint8_t n=0; n<OUTPUT_AXIS_COUNT; n++)
    {
        // Number of steps for each axis
        block->steps[n] = std::abs(target_step_position[n]-final_step_position[n]);
        block->step_event_count = std::max(block->step_event_count, block->steps[n]);
        // Compute direction bits for this block
        if (target_step_position[n] < final_step_position[n])
            block->direction_bits |= 1 << n;
    }

    // Bail if this is a zero-length block
    if (block->step_event_count <= 0)
    {
        return true;
    }

    feed_rate = std::max(minimumfeedrate, feed_rate);

    float delta_mm[OUTPUT_AXIS_COUNT];
    for(uint8_t n=0; n<OUTPUT_AXIS_COUNT; n++)
        delta_mm[n] = (target_step_position[n]-final_step_position[n])/axis_steps_per_unit[n];
#if OUTPUT_AXIS_COUNT > 2
    if (block->steps[0] || block->steps[1] || block->steps[2])
    {
        block->millimeters = sqrt(square(delta_mm[0]) + square(delta_mm[1]) + square(delta_mm[2]));
    }
    else
    {
        for(uint8_t n=3; n<OUTPUT_AXIS_COUNT; n++)
        {
            if (block->steps[n])
            {
                block->millimeters = fabs(delta_mm[n]);
                break;
            }
        }
    }
#elif OUTPUT_AXIS_COUNT == 2
    block->millimeters = sqrt(square(delta_mm[0]) + square(delta_mm[1]));
#else
    block->millimeters = delta_mm[0];
#endif
    float inverse_millimeters = 1.0/block->millimeters;  // Inverse millimeters to remove multiple divides

    // Calculate speed in mm/second for each axis. No divide by zero due to previous checks.
    float inverse_second = feed_rate * inverse_millimeters;

    block->nominal_speed = block->millimeters * inverse_second; // (mm/sec) Always > 0
    block->nominal_rate = ceil(block->step_event_count * inverse_second); // (step/sec) Always > 0

    // Calculate and limit speed in mm/sec for each axis
    float current_speed[OUTPUT_AXIS_COUNT];
    float speed_factor = 1.0; // factor <1 decreases speed
    for (uint8_t i = 0; i < OUTPUT_AXIS_COUNT; i++)
    {
        current_speed[i] = delta_mm[i] * inverse_second;
        if(fabs(current_speed[i]) > max_feedrate[i])
            speed_factor = std::min(speed_factor, max_feedrate[i] / std::abs(current_speed[i]));
    }

    // Correct the speed
    if (speed_factor < 1.0)
    {
        for (uint8_t n = 0; n < OUTPUT_AXIS_COUNT; n++)
            current_speed[n] *= speed_factor;
        block->nominal_speed *= speed_factor;
        block->nominal_rate *= speed_factor;
    }

    // Compute and limit the acceleration rate for the trapezoid generator.
    float steps_per_mm = block->step_event_count/block->millimeters;
    block->acceleration_st = ceil(acceleration * steps_per_mm); // convert to: acceleration steps/sec^2
    
    // Limit acceleration per axis
    for (uint8_t n = 0; n < OUTPUT_AXIS_COUNT; n++)
        if(((float)block->acceleration_st * (float)block->steps[n] / (float)block->step_event_count ) > axis_steps_per_sqr_second[n])
            block->acceleration_st = axis_steps_per_sqr_second[n]; //TODO: This is wrong, but fix is more complex then you would think.

    block->acceleration = block->acceleration_st / steps_per_mm;

    // Start with a safe speed (from which the machine may halt to stop immediately).
    float vmax_junction = max_xy_jerk/2;
    float vmax_junction_factor = 1.0;
#if OUTPUT_AXIS_COUNT > 2
    if(fabs(current_speed[2]) > max_z_jerk/2)
        vmax_junction = std::min(vmax_junction, max_z_jerk/2);
#endif
    vmax_junction = std::min(vmax_junction, block->nominal_speed);
    float safe_speed = vmax_junction;
    
    //As we cannot modify the first planned move, we need at least 2 moves in the buffer to keep a junction speed.
    if (moves_planned() > 1 && (previous_nominal_speed > 0.0001))
    {
        float xy_jerk = sqrt(square(current_speed[0]-previous_speed[0])+square(current_speed[1]-previous_speed[1]));
        vmax_junction = block->nominal_speed;
        if (xy_jerk > max_xy_jerk)
            vmax_junction_factor = (max_xy_jerk / xy_jerk);
#if OUTPUT_AXIS_COUNT > 2
        if(fabs(current_speed[2] - previous_speed[2]) > max_z_jerk)
            vmax_junction_factor = std::min(vmax_junction_factor, (max_z_jerk/fabs(current_speed[2] - previous_speed[2])));
#endif
        vmax_junction = std::min(previous_nominal_speed, vmax_junction * vmax_junction_factor); // Limit speed to max previous speed
    }

    // Max entry speed of this block equals the max exit speed of the previous block.
    block->max_entry_speed = vmax_junction;

    // Initialize block entry speed. Compute based on deceleration to user-defined MINIMUM_PLANNER_SPEED.
    float v_allowable = max_allowable_speed(-block->acceleration,MINIMUM_PLANNER_SPEED,block->millimeters);
    block->entry_speed = std::min(vmax_junction, v_allowable);

    // Initialize planner efficiency flags
    // Set flag if block will always reach maximum junction speed regardless of entry/exit speeds.
    // If a block can de/ac-celerate from nominal speed to zero within the length of the block, then
    // the current block and next block junction speeds are guaranteed to always be at their maximum
    // junction speeds in deceleration and acceleration, respectively. This is due to how the current
    // block nominal speed limits both the current and next maximum junction speeds. Hence, in both
    // the reverse and forward planners, the corresponding block junction speed will always be at the
    // the maximum junction speed and may always be ignored for any speed reduction checks.
    if (block->nominal_speed <= v_allowable)
    {
        block->nominal_length_flag = true;
    }
    else
    {
        block->nominal_length_flag = false;
    }
    block->recalculate_flag = true; // Always calculate trapezoid for new block

    // Update previous path unit_vector and nominal speed
    memcpy(previous_speed, current_speed, sizeof(previous_speed)); // previous_speed[] = current_speed[]
    previous_nominal_speed = block->nominal_speed;

    calculate_trapezoid_for_block(block, block->entry_speed/block->nominal_speed, safe_speed/block->nominal_speed);

    // Move buffer head
    block_buffer_head = next_buffer_head;

    // Update position
    memcpy(final_step_position, target_step_position, sizeof(target_step_position)); // position[] = target[]

    planner_recalculate();
    return true;
}

void planner_set_position(const float (&position)[INPUT_AXIS_COUNT])
{
    planner_position_to_steps(position, final_step_position);
}

// Return the number of buffered moves.
static uint8_t moves_planned()
{
    return (block_buffer_head - block_buffer_tail + BLOCK_BUFFER_SIZE) & (BLOCK_BUFFER_SIZE - 1);
}

uint8_t planner_buf_free_positions()
{
    return (BLOCK_BUFFER_SIZE - 1) - moves_planned();
}

// Calculate the steps/s^2 acceleration rates, based on the mm/s^s
void reset_acceleration_rates()
{
    for (uint8_t i = 0; i < OUTPUT_AXIS_COUNT; i++)
    {
        axis_steps_per_sqr_second[i] = max_acceleration_units_per_sq_second[i] * axis_steps_per_unit[i];
    }
}
