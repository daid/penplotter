#include "arch/stepperMotor.h"
#include "config/planner.h"
#include "stdio.h"
#include <assert.h>


static bool sim_direction[OUTPUT_AXIS_COUNT];
static int sim_position[OUTPUT_AXIS_COUNT];
static unsigned int timer_interval_us;
static InterruptFunctionPtr sim_interrupt_function;

void sim_run_interrupts()
{
    if (sim_interrupt_function)
        sim_interrupt_function();
    printf("%d %d %d\n", sim_position[0], sim_position[1], timer_interval_us);
}

void stepper_motors_init(InterruptFunctionPtr interrupt_function)
{
    sim_interrupt_function = interrupt_function;
}

void stepper_motors_interrupt_disable()
{
}

void stepper_motors_interrupt_enable()
{
}

void stepper_motors_disable()
{
}

void stepper_motors_enable()
{
}

void stepper_motors_set_interval(unsigned int interval_us)
{
    timer_interval_us = interval_us;
}

void stepper_motors_set_direction(int index, bool active)
{
    assert(index >= 0 && index < OUTPUT_AXIS_COUNT);
    sim_direction[index] = active;
}

void stepper_motors_set_step_pulse(int index, bool active)
{
    assert(index >= 0 && index < OUTPUT_AXIS_COUNT);
    if (active)
        sim_position[index] += sim_direction[index] ? -1 : 1;
}
