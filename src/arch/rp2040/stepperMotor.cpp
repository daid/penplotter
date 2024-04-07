#include "arch/stepperMotor.h"
#include "config/planner.h"
#include "stdio.h"
#include <assert.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/time.h>
#include <pico/stdlib.h>
#include <stdio.h>


static constexpr uint32_t enable_pin = 2;
static constexpr uint32_t step0_dir = 11;
static constexpr uint32_t step0_pin = 12;
static constexpr uint32_t step1_dir = 9;
static constexpr uint32_t step1_pin = 10;


static InterruptFunctionPtr stepper_interrupt;
static repeating_timer_t stepper_timer;

static bool timer_callback(repeating_timer_t*)
{
    stepper_interrupt();
    return true;
}


void stepper_motors_init(InterruptFunctionPtr interrupt_function)
{
    stdio_init_all();
    gpio_init(enable_pin);
    gpio_set_dir(enable_pin, true);
    gpio_put(enable_pin, true);
    gpio_init(step0_dir);
    gpio_set_dir(step0_dir, true);
    gpio_init(step1_dir);
    gpio_set_dir(step1_dir, true);
    gpio_init(step0_pin);
    gpio_set_dir(step0_pin, true);
    gpio_init(step1_pin);
    gpio_set_dir(step1_pin, true);

    stepper_interrupt = interrupt_function;
    add_repeating_timer_us(1000, &timer_callback, nullptr, &stepper_timer);
}

void stepper_motors_interrupt_disable()
{
    irq_set_enabled(TIMER_IRQ_3, false);
}

void stepper_motors_interrupt_enable()
{
    irq_set_enabled(TIMER_IRQ_3, true);
}

void stepper_motors_disable()
{
    gpio_put(enable_pin, true);
}

void stepper_motors_enable()
{
    gpio_put(enable_pin, false);
}

void stepper_motors_set_interval(unsigned int interval_us)
{
    stepper_timer.delay_us = interval_us;
}

void stepper_motors_set_direction(int index, bool active)
{
    if (index == 0)
        gpio_put(step0_dir, active);
    else
        gpio_put(step1_dir, active);
}

void stepper_motors_set_step_pulse(int index, bool active)
{
    if (index == 0)
        gpio_put(step0_pin, active);
    else
        gpio_put(step1_pin, active);
}
