#include "plannerConfig.h"

#include <math.h>

float axis_steps_per_unit[OUTPUT_AXIS_COUNT] = DEFAULT_AXIS_STEPS_PER_UNIT;

void planner_position_to_steps(const float (&position)[INPUT_AXIS_COUNT], long (&step_position)[OUTPUT_AXIS_COUNT])
{
    step_position[0] = lround(position[0]*axis_steps_per_unit[0]);
    step_position[1] = lround(position[1]*axis_steps_per_unit[1]);
    step_position[2] = lround(position[2]*axis_steps_per_unit[2]);
    step_position[3] = -step_position[1];
    step_position[4] = 0;
}