#include "stepper.h"
#include "planner.h"
#include "arch/stepperMotor.h"
#include <algorithm>
#include <stdio.h>


static block_t* current_block;
static int counters[OUTPUT_AXIS_COUNT];
static unsigned int step_events_completed;
static unsigned int acceleration_time_us;
static unsigned int acceleration_step_rate;
static unsigned int deceleration_time_us;


static void stepper_interrupt_callback()
{
    if (!current_block) {
        current_block = planner_get_current_block();
        if (!current_block) {
            stepper_motors_set_interval(1000);
            return;
        }
        step_events_completed = 0;
        for(size_t n=0; n<OUTPUT_AXIS_COUNT; n++) {
            counters[n] = -int(current_block->step_event_count / 2);
            stepper_motors_set_direction(n, (current_block->direction_bits & (1 << n)));
        }
        acceleration_time_us = 0;
        deceleration_time_us = 0;
    }

    for(size_t n=0; n<OUTPUT_AXIS_COUNT; n++) {
        counters[n] += current_block->steps[n];
        if (counters[n] > 0) {
            stepper_motors_set_step_pulse(n, true);
            counters[n] -= current_block->step_event_count;
        }
    }
    step_events_completed += 1;

    if (step_events_completed < current_block->accelerate_until) {
        acceleration_step_rate = (uint64_t(acceleration_time_us) * uint64_t(current_block->acceleration_st)) / 1000000;
        acceleration_step_rate += current_block->initial_rate;
        if (acceleration_step_rate > current_block->nominal_rate)
            acceleration_step_rate = current_block->nominal_rate;
        auto delay_us = 1000000 / acceleration_step_rate;
        stepper_motors_set_interval(delay_us);
        acceleration_time_us += delay_us;
    } else if (step_events_completed > current_block->decelerate_after) {
        unsigned int rate = (uint64_t(deceleration_time_us) * uint64_t(current_block->acceleration_st)) / 1000000;
        if (rate < acceleration_step_rate)
            rate = std::max(acceleration_step_rate - rate, current_block->final_rate);
        else
            rate = current_block->final_rate;
        auto delay_us = 1000000 / rate;
        stepper_motors_set_interval(delay_us);
        deceleration_time_us += delay_us;
    } else {
        stepper_motors_set_interval(1000000 / current_block->nominal_rate);
    }

    if (step_events_completed >= current_block->step_event_count)
    {
        current_block = nullptr;
        planner_discard_current_block();
    }

    for(size_t n=0; n<OUTPUT_AXIS_COUNT; n++) {
        stepper_motors_set_step_pulse(n, false);
    }
}

void stepper_init()
{
    stepper_motors_init(stepper_interrupt_callback);
    stepper_motors_set_interval(1000);
}
