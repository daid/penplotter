#pragma once


using InterruptFunctionPtr = void (*)();

void stepper_motors_init(InterruptFunctionPtr interrupt_function);
void stepper_motors_interrupt_disable();
void stepper_motors_interrupt_enable();
void stepper_motors_set_interval(unsigned int interval_us);
void stepper_motors_enable();
void stepper_motors_disable();

void stepper_motors_set_direction(int index, bool active);
void stepper_motors_set_step_pulse(int index, bool active);
