#pragma once
#include <stdint.h>
#include "multiboot.h"

#define FB_MAX_W 1280
#define FB_MAX_H 1024

extern int fb_w, fb_h;            /* drawing size (<= FB_MAX_*) */
extern uint32_t* fb_back;         /* back buffer, 0x00RRGGBB     */
extern const char* fb_source;     /* "GRUB/VBE", "Bochs BGA", ... */

/* Find a 32-bit linear framebuffer (Multiboot info, else Bochs/VirtualBox BGA). 0 = none -> text mode */
int  fb_init(struct multiboot_info* mbi, uint32_t magic);
void fb_flip(void);               /* back buffer -> screen */

/* Clip rectangle for all primitives below (screen coordinates, exclusive max) */
void fb_clip(int x0, int y0, int x1, int y1);
void fb_noclip(void);

void fb_clear(uint32_t c);
void fb_fill(int x, int y, int w, int h, uint32_t c);
void fb_fill_a(int x, int y, int w, int h, uint32_t c, int alpha);   /* alpha 0..255 */
void fb_hline(int x, int y, int w, uint32_t c);
void fb_vline(int x, int y, int h, uint32_t c);
void fb_rect(int x, int y, int w, int h, uint32_t c);
void fb_line(int x0, int y0, int x1, int y1, uint32_t c, int alpha);
void fb_blend(int x, int y, uint32_t c, int alpha);

static inline uint32_t rgb(int r, int g, int b) { return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; }
