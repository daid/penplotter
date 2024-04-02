#include "arch/sleep.h"
#include "pico/stdlib.h"

void arch_sleep(unsigned int delay_us)
{
  sleep_us(delay_us);
}
