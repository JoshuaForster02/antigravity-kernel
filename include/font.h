#pragma once
#include <stdint.h>

/* Anti-aliased bitmap font: one 8-bit alpha cell (adv x height) per ASCII glyph 32..126 */
struct font {
    int height, ascent;
    const unsigned char* adv;
    const unsigned int*  off;
    const unsigned char* px;
};
extern const struct font font_mono, font_ui, font_bold, font_big;

/* Draw text at (x, y = top of line); spacing = extra pixels between glyphs. Returns end x. */
int  font_draw(const struct font* f, int x, int y, const char* s, uint32_t color, int spacing);
int  font_width(const struct font* f, const char* s, int spacing);
