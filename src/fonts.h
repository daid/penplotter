#pragma once

#include <stdint.h>

static constexpr uint16_t font_end_of_line = 0x7FFF;

bool font_set(const char* name);
const int16_t* font_get_lines(int codepoint);
int16_t font_get_advance(int codepoint);