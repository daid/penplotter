#include "fonts.h"
#include "fonts.inc"
#include <cstring>

const Font* current_font = nullptr;


bool font_set(const char* name)
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

static const Glyph* font_get_glyph(int codepoint)
{
    if (!current_font) return nullptr;
    for(auto glyph = current_font->glyphs; glyph->codepoint; glyph++) {
        if (glyph->codepoint == codepoint) {
            return glyph;
        }
    }
    return nullptr;
}

const int16_t* font_get_lines(int codepoint)
{
    auto g = font_get_glyph(codepoint);
    if (g) return g->lines;
    return nullptr;
}

int16_t font_get_advance(int codepoint)
{
    auto g = font_get_glyph(codepoint);
    if (g) return g->advance;
    return 400;
}
