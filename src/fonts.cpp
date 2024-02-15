#include "fonts.h"
#include "fonts.inc"
#include <cstring>

const Font* current_font = nullptr;


bool set_font(const char* name)
{
    auto f = _all_fonts;
    while(*f) {
        if (strcmp((*f)->name, name) == 0) {
            current_font = *f;
            return true;
        }
        f++;
    }
    return false;
}