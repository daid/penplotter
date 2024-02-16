#include "arch/sleep.h"

void sim_run_interrupts();

void arch_sleep(unsigned int delay_us)
{
    (void)delay_us;
    sim_run_interrupts();
}