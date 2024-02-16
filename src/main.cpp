#include "motion/stepper.h"
#include "motion/planner.h"
#include "fonts.h"
#include "arch/pen.h"
#include "arch/sleep.h"
#include <stdio.h>


float text_scale = 10.0f / 1000.0f;
float travel_speed = 3000.0;
float draw_speed = 1000.0;
void plot_glyph(int c);

int main()
{
    planner_init();
    stepper_init();
    pen_init();

    font_set("EMSHerculean");
    for(auto c : "Hello world")
        plot_glyph(c);
/*
    planner_set_position({0.0, 0.0});
    planner_buffer_line({1.0, 0.0}, 3000, 1000);
    planner_buffer_line({2.0, 0.0}, 3000, 1000);
    planner_buffer_line({3.0, 0.0}, 3000, 1000);
    planner_buffer_line({4.0, 0.0}, 3000, 1000);
    planner_buffer_line({100.0, 0.0}, 3000, 1000);
    planner_buffer_line({200.0, 0.0}, 3000, 1000);
    while(planner_buf_free_positions() != BLOCK_BUFFER_SIZE - 1)
        arch_sleep(1);
*/
    return 0;
}

void wait_for_planner_done()
{
    while(planner_buf_free_positions() != BLOCK_BUFFER_SIZE - 1)
        arch_sleep(1);
}

void plot_glyph(int c)
{
    auto lines = font_get_lines(c);
    if (!lines)
        return;
    float pos[2] = {0, 0};
    planner_set_position(pos);
    while(*lines != font_end_of_line) {
        // First move
        pos[0] = float(*lines++) * text_scale;
        pos[1] = float(*lines++) * text_scale;
        while(!planner_buffer_line(pos, travel_speed, 100))
            arch_sleep(1);
        wait_for_planner_done();
        pen_down();
        while(*lines != font_end_of_line) {
            pos[0] = float(*lines++) * text_scale;
            pos[1] = float(*lines++) * text_scale;
            while(!planner_buffer_line(pos, draw_speed, 100))
                arch_sleep(1);
        }
        lines++;
        wait_for_planner_done();
        pen_up();
    }
    pos[0] = float(font_get_advance(c)) * text_scale;
    pos[1] = 0;
    while(!planner_buffer_line(pos, travel_speed, 100))
        arch_sleep(1);
    wait_for_planner_done();
}