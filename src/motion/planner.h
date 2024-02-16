/*
  planner.h - buffers movement commands and manages the acceleration profile plan
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
#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "../config/planner.h"

#define BLOCK_BUFFER_SIZE 32

// This struct is used when buffering the setup for each linear movement "nominal" values are as specified in
// the source g-code and may never actually be reached if acceleration management is active.
typedef struct {
    // Fields used by the bresenham algorithm for tracing the line
    unsigned int steps[OUTPUT_AXIS_COUNT];   // Step count along each axis
    unsigned int step_event_count;           // The number of step events required to complete this block
    unsigned int accelerate_until;           // The index of the step event on which to stop acceleration
    unsigned int decelerate_after;           // The index of the step event on which to start decelerating
    unsigned char direction_bits;             // The direction bit set for this block

    // Fields used by the motion planner to manage acceleration
    float nominal_speed;                               // The nominal speed for this block in mm/sec
    float entry_speed;                                 // Entry speed at previous-current junction in mm/sec
    float max_entry_speed;                             // Maximum allowable junction entry speed in mm/sec
    float millimeters;                                 // The total travel of this block in mm
    float acceleration;                                // acceleration mm/sec^2
    bool recalculate_flag;                             // Planner flag to recalculate trapezoids on entry junction
    bool nominal_length_flag;                          // Planner flag for nominal speed always reached

    // Settings for the trapezoid generator
    unsigned int nominal_rate;                        // The nominal step rate for this block in step_events/sec
    unsigned int initial_rate;                        // The jerk-adjusted step rate at start of block
    unsigned int final_rate;                          // The minimal rate at exit
    unsigned int acceleration_st;                     // acceleration steps/sec^2
    volatile bool busy;
} block_t;

// Initialize the motion plan subsystem
void planner_init();

// Add a new linear movement to the buffer. x, y and z is the signed, absolute target position in
// millimeters. Feed rate specifies the speed of the motion.
bool planner_buffer_line(const float (&position)[INPUT_AXIS_COUNT], float feed_rate, float acceleration);

// Set position. Used for G92 instructions.
void planner_set_position(const float (&position)[INPUT_AXIS_COUNT]);

uint8_t planner_buf_free_positions();  // return the number of free positions in the planner buffer.

extern float max_feedrate[OUTPUT_AXIS_COUNT]; // set the max speeds

extern unsigned long max_acceleration_units_per_sq_second[OUTPUT_AXIS_COUNT]; // Use M201 to override by software
extern float minimumfeedrate;
extern float max_xy_jerk;          //speed that can be stopped at once, if I understand correctly.
extern float max_z_jerk;
extern unsigned long axis_steps_per_sqr_second[OUTPUT_AXIS_COUNT];




extern block_t block_buffer[BLOCK_BUFFER_SIZE];            // A ring buffer for motion instructions
extern volatile unsigned char block_buffer_head;           // Index of the next block to be pushed
extern volatile unsigned char block_buffer_tail;
// Called when the current block is no longer needed. Discards the block and makes the memory
// available for new blocks.
static inline void planner_discard_current_block()
{
    if (block_buffer_head != block_buffer_tail)
        block_buffer_tail = (block_buffer_tail + 1) & (BLOCK_BUFFER_SIZE - 1);
}

// Gets the current block. Returns NULL if buffer empty
static inline block_t *planner_get_current_block()
{
    if (block_buffer_head == block_buffer_tail)
        return NULL;
    block_t *block = &block_buffer[block_buffer_tail];
    block->busy = true;
    return block;
}

// Returns true when blocks are queued, false otherwise.
static inline bool blocks_queued()
{
    if (block_buffer_head == block_buffer_tail)
        return false;
    else
        return true;
}

void reset_acceleration_rates();
