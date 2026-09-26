/* Linear framebuffer + software back buffer. All drawing goes to fb_back, fb_flip() copies to VRAM. */
#include "fb.h"
#include "pci.h"
#include "io.h"
#include <stddef.h>

int fb_w, fb_h;
uint32_t* fb_back;
const char* fb_source = "none";

static uint32_t backbuf[FB_MAX_W * FB_MAX_H];   /* ~5 MiB in .bss */
static uint8_t* vram;
static uint32_t pitch;
static int cx0, cy0, cx1, cy1;

/* ── Bochs Graphics Adapter (QEMU std-vga, Bochs, VirtualBox VBoxVGA) ───── */
static void bga_write(uint16_t idx, uint16_t v) {
    __asm__ volatile ("outw %0, %1" : : "a"(idx), "Nd"((uint16_t)0x1CE));
    __asm__ volatile ("outw %0, %1" : : "a"(v),   "Nd"((uint16_t)0x1CF)); }
static uint16_t bga_read(uint16_t idx) { uint16_t r;
    __asm__ volatile ("outw %0, %1" : : "a"(idx), "Nd"((uint16_t)0x1CE));
    __asm__ volatile ("inw %1, %0"  : "=a"(r) : "Nd"((uint16_t)0x1CF)); return r; }

static int bga_init(int w, int h) {
    uint16_t id = bga_read(0);
    if (id < 0xB0C0 || id > 0xB0C5) return 0;
    uint32_t lfb = pci_find_bar0(0x1234, 0x1111);          /* QEMU / Bochs */
    if (!lfb) lfb = pci_find_bar0(0x80EE, 0xBEEF);         /* VirtualBox   */
    if (!lfb) lfb = 0xFD000000;
    bga_write(4, 0);                   /* disable */
    bga_write(1, (uint16_t)w);
    bga_write(2, (uint16_t)h);
    bga_write(3, 32);
    bga_write(4, 0x41);                /* enable | linear framebuffer */
    vram = (uint8_t*)lfb; pitch = (uint32_t)w * 4;
    fb_w = w; fb_h = h; fb_source = "Bochs BGA";
    return 1;
}

int fb_init(struct multiboot_info* mbi, uint32_t magic) {
    fb_back = backbuf;
    if (magic == MULTIBOOT_MAGIC && mbi && (mbi->flags & (1u << 12)) &&
        mbi->framebuffer_type == 1 && mbi->framebuffer_bpp == 32 && mbi->framebuffer_addr < 0xFFFFFFFFull) {
        vram  = (uint8_t*)(uint32_t)mbi->framebuffer_addr;
        pitch = mbi->framebuffer_pitch;
        fb_w  = (int)mbi->framebuffer_width  > FB_MAX_W ? FB_MAX_W : (int)mbi->framebuffer_width;
        fb_h  = (int)mbi->framebuffer_height > FB_MAX_H ? FB_MAX_H : (int)mbi->framebuffer_height;
        fb_source = "GRUB/VBE";
    } else if (!bga_init(1024, 768)) {
        return 0;
    }
    fb_noclip();
    return 1;
}

void fb_flip(void) {
    for (int y = 0; y < fb_h; y++) {
        const uint32_t* src = fb_back + y * fb_w;
        void* dst = vram + (uint32_t)y * pitch;
        int n = fb_w;
        __asm__ volatile ("rep movsl" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    }
}

void fb_clip(int x0, int y0, int x1, int y1) {
    cx0 = x0 < 0 ? 0 : x0;  cy0 = y0 < 0 ? 0 : y0;
    cx1 = x1 > fb_w ? fb_w : x1;  cy1 = y1 > fb_h ? fb_h : y1;
}
void fb_noclip(void) { cx0 = 0; cy0 = 0; cx1 = fb_w; cy1 = fb_h; }

void fb_clear(uint32_t c) {
    uint32_t* p = fb_back; int n = fb_w * fb_h;
    __asm__ volatile ("rep stosl" : "+D"(p), "+c"(n) : "a"(c) : "memory");
}

void fb_fill(int x, int y, int w, int h, uint32_t c) {
    int x1 = x + w, y1 = y + h;
    if (x < cx0) x = cx0;
    if (y < cy0) y = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    if (x >= x1 || y >= y1) return;
    for (int yy = y; yy < y1; yy++) {
        uint32_t* p = fb_back + yy * fb_w + x; int n = x1 - x;
        __asm__ volatile ("rep stosl" : "+D"(p), "+c"(n) : "a"(c) : "memory");
    }
}

static inline uint32_t mix(uint32_t d, uint32_t c, int a) {
    uint32_t rb = ((c & 0xFF00FF) * a + (d & 0xFF00FF) * (256 - a)) >> 8;
    uint32_t g  = ((c & 0x00FF00) * a + (d & 0x00FF00) * (256 - a)) >> 8;
    return (rb & 0xFF00FF) | (g & 0x00FF00);
}

void fb_blend(int x, int y, uint32_t c, int alpha) {
    if (x < cx0 || y < cy0 || x >= cx1 || y >= cy1 || alpha <= 0) return;
    uint32_t* p = fb_back + y * fb_w + x;
    *p = alpha >= 255 ? c : mix(*p, c, alpha + 1);
}

void fb_fill_a(int x, int y, int w, int h, uint32_t c, int alpha) {
    int x1 = x + w, y1 = y + h;
    if (x < cx0) x = cx0;
    if (y < cy0) y = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    for (int yy = y; yy < y1; yy++)
        for (int xx = x; xx < x1; xx++) { uint32_t* p = fb_back + yy * fb_w + xx; *p = mix(*p, c, alpha + 1); }
}

void fb_hline(int x, int y, int w, uint32_t c) { fb_fill(x, y, w, 1, c); }
void fb_vline(int x, int y, int h, uint32_t c) { fb_fill(x, y, 1, h, c); }
void fb_rect(int x, int y, int w, int h, uint32_t c) {
    fb_hline(x, y, w, c); fb_hline(x, y + h - 1, w, c); fb_vline(x, y, h, c); fb_vline(x + w - 1, y, h, c);
}

void fb_line(int x0, int y0, int x1, int y1, uint32_t c, int alpha) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 8192; guard++) {
        fb_blend(x0, y0, c, alpha);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
