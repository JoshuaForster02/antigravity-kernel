#include "font.h"
#include "fb.h"

int font_draw(const struct font* f, int x, int y, const char* s, uint32_t color, int spacing) {
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) ch = '?';
        int g = ch - 32, w = f->adv[g];
        const unsigned char* px = f->px + f->off[g];
        for (int yy = 0; yy < f->height; yy++)
            for (int xx = 0; xx < w; xx++) {
                int a = px[yy * w + xx];
                if (a) fb_blend(x + xx, y + yy, color, a);
            }
        x += w + spacing;
    }
    return x;
}

int font_width(const struct font* f, const char* s, int spacing) {
    int w = 0;
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) ch = '?';
        w += f->adv[ch - 32] + spacing;
    }
    return w;
}
