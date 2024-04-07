#include "arch/pen.h"
#include <stdio.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>

static constexpr uint32_t pen_pin = 14;

void pen_init()
{
    gpio_init(pen_pin);
    gpio_set_dir(pen_pin, true);
    pen_up();
}

static void update_servo(uint32_t delay_us)
{
    for(int n=0; n<15; n++) {
        gpio_put(pen_pin, true);
        sleep_us(delay_us);
        gpio_put(pen_pin, false);
        sleep_us(20000 - delay_us);
    }
}

void pen_up()
{
    update_servo(1300);
}

void pen_down()
{
    update_servo(2000);
}
