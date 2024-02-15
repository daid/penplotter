#pragma once

#include "../config/planner.h"

extern float axis_steps_per_unit[OUTPUT_AXIS_COUNT];

void planner_position_to_steps(const float (&position)[INPUT_AXIS_COUNT], long (&step_position)[OUTPUT_AXIS_COUNT]);
