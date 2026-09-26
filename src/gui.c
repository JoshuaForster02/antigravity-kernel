/* gui.c — ENCOM OS-12 desktop: boot sequence, window manager and apps.
 *
 * Everything is redrawn into the back buffer every frame (~33 fps at 100 Hz / 3)
 * and flipped to VRAM. No damage tracking: simple and fast enough at 1024x768.
 */
#include "gui.h"
#include "fb.h"
#include "font.h"
#include "timer.h"
#include "mouse.h"
#include "keyboard.h"
#include "rtc.h"
#include "shell.h"
#include "system.h"
#include "serial.h"
#include "string.h"
#include <stdint.h>

/* ── Palette ─────────────────────────────────────────────────────────────── */
#define C_BG      0x01040A
#define C_PANEL   0x020A10
#define C_CYAN    0x3FE4FF
#define C_CYAN_D  0x0E5A6E
#define C_INK     0xDFF8FF
#define C_DIM     0x6F8F99
#define C_ORANGE  0xFF9A1F
#define C_RED     0xFF5A4A
#define C_GREEN   0x5CFFB4

#define BAR_H   30
#define TITLE_H 26

/* ── small helpers ───────────────────────────────────────────────────────── */
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }
static void utoa(uint32_t v, char* buf) {
    char t[12]; int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    for (int i = 0; i < n; i++) buf[i] = t[n - 1 - i];
    buf[n] = 0;
}
static void two(char* p, int v) { p[0] = (char)('0' + v / 10 % 10); p[1] = (char)('0' + v % 10); }
static char* cat(char* d, const char* s) { while (*d) d++; while (*s) *d++ = *s++; *d = 0; return d; }
static uint32_t rng = 0x2545F491;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* Corner brackets — the signature ENCOM frame accent */
static void brackets(int x, int y, int w, int h, int len, uint32_t c) {
    fb_hline(x, y, len, c);               fb_vline(x, y, len, c);
    fb_hline(x + w - len, y, len, c);     fb_vline(x + w - 1, y, len, c);
    fb_hline(x, y + h - 1, len, c);       fb_vline(x, y + h - len, len, c);
    fb_hline(x + w - len, y + h - 1, len, c); fb_vline(x + w - 1, y + h - len, len, c);
}

/* ── Windows ─────────────────────────────────────────────────────────────── */
struct win {
    const char* title;
    const char* id;                 /* name for `open <id>` */
    int x, y, w, h, open;
    void (*draw)(struct win*, int x, int y, int w, int h);
    void (*key)(struct win*, int k);
};

enum { W_TERM, W_SYS, W_CYCLE, W_ID, NWIN };
static struct win wins[NWIN];
static int order[NWIN];            /* z-order, last = top */
static int norder = 0;
static int drag = -1, drag_dx, drag_dy, last_buttons;
static uint32_t frames, fps, input_events, hist_ev[60], hist_fps[60];

static int top_win(void) { return norder ? order[norder - 1] : -1; }
static void focus(int i) {
    int k = 0;
    for (int j = 0; j < norder; j++) if (order[j] != i) order[k++] = order[j];
    order[k++] = i; norder = k;
    wins[i].open = 1;
}
static void close_win(int i) {
    int k = 0;
    for (int j = 0; j < norder; j++) if (order[j] != i) order[k++] = order[j];
    norder = k; wins[i].open = 0;
    if (drag == i) drag = -1;
}
static int open_by_name(const char* n) {
    for (int i = 0; i < NWIN; i++) if (!strcmp(n, wins[i].id)) { focus(i); return 1; }
    return 0;
}

/* ── App: TERMINAL ───────────────────────────────────────────────────────── */
#define TCOLS 72
#define TROWS 240
static char    tbuf[TROWS][TCOLS + 1];
static uint8_t tsty[TROWS][TCOLS];
static int trow, tcol, tlen;
static char tin[128];

static void term_newline(void) {
    tcol = 0;
    if (++trow >= TROWS) {                         /* scroll history up by one line */
        for (int r = 1; r < TROWS; r++) { memcpy(tbuf[r - 1], tbuf[r], TCOLS + 1); memcpy(tsty[r - 1], tsty[r], TCOLS); }
        trow = TROWS - 1;
        memset(tbuf[trow], 0, TCOLS + 1); memset(tsty[trow], 0, TCOLS);
    }
}
static void term_putc(char c, int style) {
    if (c == '\n') { term_newline(); return; }
    if (tcol >= TCOLS) term_newline();
    tbuf[trow][tcol] = c; tsty[trow][tcol] = (uint8_t)style; tcol++;
}
static void term_out(const char* s, int style) {
    for (const char* p = s; *p; p++) term_putc(*p, style);
    serial_write(s);
}
static void term_clear(void) { memset(tbuf, 0, sizeof tbuf); memset(tsty, 0, sizeof tsty); trow = tcol = 0; }

static const uint32_t style_col[] = { C_INK, C_CYAN, C_GREEN, C_RED, C_DIM };

static void term_draw(struct win* w, int x, int y, int ww, int hh) {
    (void)w;
    int lh = font_mono.height + 1, cw = font_mono.adv[0];
    int rows = (hh - 12) / lh;
    int first = imax(0, trow + 1 - rows);
    char one[2] = {0, 0};
    for (int r = first; r <= trow; r++) {
        int yy = y + 6 + (r - first) * lh;
        for (int c = 0; c < TCOLS && tbuf[r][c]; c++) {
            one[0] = tbuf[r][c];
            font_draw(&font_mono, x + 10 + c * cw, yy, one, style_col[tsty[r][c]], 0);
        }
    }
    if (top_win() == W_TERM && (timer_ticks / 50) % 2 == 0) {  /* blinking block cursor */
        int cy = y + 6 + (trow - first) * lh;
        fb_fill(x + 10 + tcol * cw, cy + 2, cw, lh - 3, C_CYAN);
    }
    (void)ww;
}
static void term_key(struct win* w, int k) {
    (void)w;
    if (k >= 0x80) return;
    if (k == '\n') {
        tin[tlen] = 0; term_newline();
        shell_exec(tin);
        tlen = 0; shell_prompt();
    } else if (k == '\b') {
        if (tlen > 0) { tlen--; if (tcol > 0) { tcol--; tbuf[trow][tcol] = 0; } }
    } else if (k >= 32 && tlen < (int)sizeof tin - 1) {
        tin[tlen++] = (char)k; term_putc((char)k, S_NORMAL);
    }
}

/* ── App: SYSTEM ─────────────────────────────────────────────────────────── */
static void kv(int x, int y, const char* k, const char* v) {
    font_draw(&font_bold, x, y + 1, k, C_DIM, 1);
    font_draw(&font_ui, x + 118, y, v, C_INK, 0);
}
static void graph(int x, int y, int w, int h, uint32_t* hist, uint32_t maxv, uint32_t color, const char* label) {
    fb_rect(x, y, w, h, C_CYAN_D);
    for (int i = 1; i < 4; i++) fb_hline(x + 1, y + h * i / 4, w - 2, 0x06202A);
    if (!maxv) maxv = 1;
    int px = -1, py = 0;
    for (int i = 0; i < 60; i++) {
        uint32_t v = hist[i] > maxv ? maxv : hist[i];
        int gx = x + 2 + i * (w - 4) / 59, gy = y + h - 3 - (int)(v * (uint32_t)(h - 6) / maxv);
        if (px >= 0) { fb_line(px, py, gx, gy, color, 255); fb_line(px, py + 1, gx, gy + 1, color, 90); }
        px = gx; py = gy;
    }
    font_draw(&font_bold, x + 8, y + 6, label, C_DIM, 1);
}
static void sys_draw(struct win* w, int x, int y, int ww, int hh) {
    (void)w; (void)hh;
    char b[64], n[12];
    uint32_t s = timer_ticks / TIMER_HZ;
    int yy = y + 14, lh = 22;
    kv(x + 16, yy, "OS", "ENCOM OS-12  /  Antigravity v0.4"); yy += lh;
    kv(x + 16, yy, "CPU", sys.cpu[0] ? sys.cpu : "unknown"); yy += lh;
    b[0] = 0; utoa(sys.mem_kb / 1024, n); cat(b, n); cat(b, " MiB");
    kv(x + 16, yy, "MEMORY", b); yy += lh;
    b[0] = 0; utoa((uint32_t)fb_w, n); cat(b, n); cat(b, " x "); utoa((uint32_t)fb_h, n); cat(b, n); cat(b, " x 32  /  "); cat(b, fb_source);
    kv(x + 16, yy, "DISPLAY", b); yy += lh;
    kv(x + 16, yy, "LOADER", sys.loader); yy += lh;
    b[0] = 0; char t[9] = "00:00:00"; two(t, (int)(s / 3600)); two(t + 3, (int)(s / 60 % 60)); two(t + 6, (int)(s % 60)); cat(b, t);
    cat(b, "   ("); utoa(timer_ticks, n); cat(b, n); cat(b, " ticks)");
    kv(x + 16, yy, "UPTIME", b); yy += lh;
    b[0] = 0; utoa(keyboard_irqs, n); cat(b, n); cat(b, " keyboard  /  "); utoa(mouse.events, n); cat(b, n); cat(b, " mouse");
    kv(x + 16, yy, "INTERRUPTS", b); yy += lh;
    b[0] = 0; utoa(fps, n); cat(b, n); cat(b, " fps");
    kv(x + 16, yy, "RENDER", b); yy += lh + 10;
    int gw = (ww - 48) / 2;
    graph(x + 16, yy, gw, 80, hist_fps, 40, C_CYAN, "FPS");
    graph(x + 32 + gw, yy, gw, 80, hist_ev, 40, C_ORANGE, "INPUT / S");
}

/* ── App: LIGHTCYCLE ─────────────────────────────────────────────────────── */
#define GW 58
#define GH 36
#define CELL 9
static uint8_t grid[GH][GW];             /* 0 free, 1 player trail, 2 program trail */
static int px, py, pdx, pdy, ax, ay, adx, ady, lc_state, lc_you, lc_them;   /* state: 0 run, 1 won, 2 lost, 3 draw */
static uint32_t lc_next;

static int lc_free(int x, int y) { return x >= 0 && y >= 0 && x < GW && y < GH && !grid[y][x]; }
static void lc_reset(void) {
    memset(grid, 0, sizeof grid);
    px = 8; py = GH / 2 + 5; pdx = 1; pdy = 0;          /* offset rows: no instant head-on */
    ax = GW - 9; ay = GH / 2 - 5; adx = -1; ady = 0;
    grid[py][px] = 1; grid[ay][ax] = 2; lc_state = 0;
}
static int lc_space(int x, int y, int dx, int dy) {   /* free cells straight ahead, capped */
    int n = 0;
    while (n < 12 && lc_free(x + dx * (n + 1), y + dy * (n + 1))) n++;
    return n;
}
static void lc_step(void) {
    if (lc_state) return;
    /* Program: keep going unless blocked soon; then turn to the side with more room */
    int ahead = lc_space(ax, ay, adx, ady);
    if (ahead < 2 || rnd() % 23 == 0) {
        int lx = ady, ly = -adx, rx = -ady, ry = adx;
        int l = lc_space(ax, ay, lx, ly), r = lc_space(ax, ay, rx, ry);
        if (l > ahead || r > ahead || ahead < 2) {
            if (l > r || (l == r && (rnd() & 1))) { adx = lx; ady = ly; } else { adx = rx; ady = ry; }
        }
    }
    int npx = px + pdx, npy = py + pdy, nax = ax + adx, nay = ay + ady;
    int pdead = !lc_free(npx, npy), adead = !lc_free(nax, nay) || (nax == npx && nay == npy);
    if (nax == npx && nay == npy) pdead = 1;
    if (pdead || adead) {
        lc_state = pdead && adead ? 3 : pdead ? 2 : 1;
        if (lc_state == 1) lc_you++; else if (lc_state == 2) lc_them++;
        return;
    }
    px = npx; py = npy; ax = nax; ay = nay;
    grid[py][px] = 1; grid[ay][ax] = 2;
}
static void cycle_draw(struct win* w, int x, int y, int ww, int hh) {
    (void)w; (void)ww; (void)hh;
    if (top_win() == W_CYCLE && !lc_state && timer_ticks >= lc_next) { lc_next = timer_ticks + 5; lc_step(); }
    int ox = x + 12, oy = y + 34;
    char b[48], n[12];
    b[0] = 0; cat(b, "USER "); utoa((uint32_t)lc_you, n); cat(b, n); cat(b, "   PROGRAM "); utoa((uint32_t)lc_them, n); cat(b, n);
    font_draw(&font_bold, ox, y + 12, b, C_DIM, 2);
    font_draw(&font_ui, ox + GW * CELL - 230, y + 10, "ARROWS / WASD  -  SPACE", C_DIM, 0);
    fb_rect(ox - 1, oy - 1, GW * CELL + 2, GH * CELL + 2, C_CYAN_D);
    for (int gx = 0; gx <= GW; gx += 6) fb_vline(ox + gx * CELL, oy, GH * CELL, 0x041820);
    for (int gy = 0; gy <= GH; gy += 6) fb_hline(ox, oy + gy * CELL, GW * CELL, 0x041820);
    for (int gy = 0; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++)
            if (grid[gy][gx]) {
                uint32_t c = grid[gy][gx] == 1 ? C_CYAN : C_ORANGE;
                fb_fill_a(ox + gx * CELL - 1, oy + gy * CELL - 1, CELL + 2, CELL + 2, c, 50);  /* glow */
                fb_fill(ox + gx * CELL + 2, oy + gy * CELL + 2, CELL - 4, CELL - 4, c);
            }
    fb_fill(ox + px * CELL, oy + py * CELL, CELL, CELL, 0xFFFFFF);
    fb_fill(ox + ax * CELL, oy + ay * CELL, CELL, CELL, 0xFFE0B0);
    if (lc_state) {
        const char* m = lc_state == 1 ? "PROGRAM DEREZZED" : lc_state == 2 ? "USER DEREZZED" : "BOTH DEREZZED";
        int tw = font_width(&font_bold, m, 4);
        int cx = ox + GW * CELL / 2, cy = oy + GH * CELL / 2;
        fb_fill(cx - tw / 2 - 24, cy - 30, tw + 48, 60, C_BG);
        brackets(cx - tw / 2 - 24, cy - 30, tw + 48, 60, 10, lc_state == 1 ? C_CYAN : C_ORANGE);
        font_draw(&font_bold, cx - tw / 2, cy - 18, m, lc_state == 1 ? C_CYAN : C_ORANGE, 4);
        font_draw(&font_ui, cx - font_width(&font_ui, "SPACE: next round", 0) / 2, cy + 2, "SPACE: next round", C_DIM, 0);
    }
}
static void cycle_key(struct win* w, int k) {
    (void)w;
    if (lc_state) { if (k == ' ' || k == '\n') lc_reset(); return; }
    int dx = pdx, dy = pdy;
    if (k == K_UP || k == 'w') { dx = 0; dy = -1; }
    else if (k == K_DOWN || k == 's') { dx = 0; dy = 1; }
    else if (k == K_LEFT || k == 'a') { dx = -1; dy = 0; }
    else if (k == K_RIGHT || k == 'd') { dx = 1; dy = 0; }
    if (dx != -pdx || dy != -pdy) { pdx = dx; pdy = dy; }   /* no 180-degree turns */
}

/* ── App: IDENTITY ───────────────────────────────────────────────────────── */
static void id_draw(struct win* w, int x, int y, int ww, int hh) {
    (void)w; (void)hh;
    int tw = font_width(&font_big, "ENCOM", 14);
    font_draw(&font_big, x + (ww - tw) / 2, y + 26, "ENCOM", C_INK, 14);
    int lw = (int)(timer_ticks % 200) * (ww - 80) / 200;
    fb_hline(x + 40, y + 86, ww - 80, C_CYAN_D);
    fb_hline(x + 40, y + 86, lw, C_CYAN);
    const char* sub = "OS-12";
    font_draw(&font_bold, x + (ww - font_width(&font_bold, sub, 8)) / 2, y + 98, sub, C_CYAN, 8);
    const char* lines[] = {
        "A bare-metal operating system from the grid up.",
        "Own kernel, drivers and window manager in C + x86 assembly.",
        "Multiboot  /  32-bit protected mode  /  no Linux, no libc.",
    };
    for (int i = 0; i < 3; i++)
        font_draw(&font_ui, x + (ww - font_width(&font_ui, lines[i], 0)) / 2, y + 138 + i * 22, lines[i], i ? C_DIM : C_INK, 0);
}

/* ── Desktop ─────────────────────────────────────────────────────────────── */
static void draw_grid_floor(void) {
    int hz = fb_h * 52 / 100, cx = fb_w / 2;
    for (int i = 0; i < 26; i++) fb_hline(0, hz - 26 + i, fb_w, rgb(0, i * 3 / 4, 2 + i));   /* horizon haze */
    fb_hline(0, hz, fb_w, 0x1B8FB0);
    /* receding lines: z steps scroll toward the viewer */
    int phase = (int)(timer_ticks % 40);
    for (int k = 0; k < 18; k++) {
        int z = 40 * 18 - (k * 40 + phase);                /* virtual depth */
        if (z <= 20) continue;
        int yy = hz + (fb_h - hz) * 40 / z;
        if (yy >= fb_h) continue;
        int a = imin(200, 40 + (yy - hz) * 3);
        fb_line(0, yy, fb_w - 1, yy, 0x1FB3D9, a);
    }
    /* converging lines to the vanishing point */
    for (int i = -16; i <= 16; i++) {
        int bx = cx + i * fb_w / 9;
        fb_line(cx + i * 6, hz, bx, fb_h - 1, 0x1FB3D9, 120);
    }
}

struct btn { int x, y, w, h, target; };
static struct btn bar_btns[NWIN];

static void draw_bar(void) {
    fb_fill_a(0, 0, fb_w, BAR_H, 0x000000, 220);
    fb_hline(0, BAR_H - 1, fb_w, C_CYAN_D);
    int x = font_draw(&font_bold, 16, 9, "ENCOM", C_INK, 4);
    x = font_draw(&font_bold, x + 6, 9, "OS-12", C_CYAN, 2) + 26;
    for (int i = 0; i < NWIN; i++) {
        int tw = font_width(&font_bold, wins[i].title, 2);
        int active = top_win() == i, open = wins[i].open;
        font_draw(&font_bold, x, 9, wins[i].title, active ? C_CYAN : open ? C_INK : C_DIM, 2);
        if (active) fb_hline(x, BAR_H - 3, tw, C_CYAN);
        else if (open) fb_hline(x, BAR_H - 3, tw, C_CYAN_D);
        bar_btns[i] = (struct btn){ x - 8, 0, tw + 16, BAR_H, i };
        x += tw + 22;
    }
    struct rtc_time t; rtc_read(&t);
    char clk[32] = "0000-00-00  00:00:00 UTC";
    two(clk + 2, t.year % 100); clk[0] = '2'; clk[1] = '0';
    two(clk + 5, t.month); two(clk + 8, t.day); two(clk + 12, t.hour); two(clk + 15, t.min); two(clk + 18, t.sec);
    font_draw(&font_mono, fb_w - 16 - font_width(&font_mono, clk, 0), 8, clk, C_INK, 0);
}

static void draw_window(int i) {
    struct win* w = &wins[i];
    int active = top_win() == i;
    fb_fill(w->x, w->y, w->w, w->h, C_PANEL);
    fb_rect(w->x, w->y, w->w, w->h, active ? 0x1C8CAA : 0x0B3A48);
    fb_fill(w->x + 1, w->y + 1, w->w - 2, TITLE_H - 1, active ? 0x03141C : 0x020D12);
    fb_hline(w->x + 1, w->y + TITLE_H, w->w - 2, active ? C_CYAN_D : 0x082A34);
    font_draw(&font_bold, w->x + 14, w->y + 7, w->title, active ? C_CYAN : C_DIM, 3);
    /* close button */
    int bx = w->x + w->w - 22, by = w->y + 8;
    fb_line(bx, by, bx + 9, by + 9, active ? C_INK : C_DIM, 255);
    fb_line(bx + 9, by, bx, by + 9, active ? C_INK : C_DIM, 255);
    if (active) brackets(w->x - 3, w->y - 3, w->w + 6, w->h + 6, 14, C_CYAN);
    fb_clip(w->x + 1, w->y + TITLE_H + 1, w->x + w->w - 1, w->y + w->h - 1);
    w->draw(w, w->x + 1, w->y + TITLE_H + 1, w->w - 2, w->h - TITLE_H - 2);
    fb_noclip();
}

static void draw_cursor(void) {
    int x = mouse.x, y = mouse.y;
    for (int i = 0; i < 16; i++) {                          /* filled arrow, dark core */
        fb_hline(x + 1, y + i, imin(i, 11) - (i > 11 ? i - 11 : 0), 0x001018);
    }
    fb_line(x, y, x, y + 16, C_CYAN, 255);
    fb_line(x, y, x + 11, y + 11, C_CYAN, 255);
    fb_line(x, y + 16, x + 4, y + 12, C_CYAN, 255);
    fb_line(x + 4, y + 12, x + 11, y + 11, C_CYAN, 255);
}

static void render(void) {
    fb_clear(C_BG);
    draw_grid_floor();
    if (!norder) {
        const char* h = "PRESS F1-F4 OR CLICK THE BAR";
        font_draw(&font_bold, (fb_w - font_width(&font_bold, h, 4)) / 2, fb_h * 40 / 100, h, C_DIM, 4);
    }
    for (int j = 0; j < norder; j++) draw_window(order[j]);
    draw_bar();
    draw_cursor();
    fb_flip();
    frames++;
}

/* ── Input ───────────────────────────────────────────────────────────────── */
static void gui_key(int k) {
    input_events++;
    if (k >= K_F1 && k <= K_F4) { focus(k - K_F1); return; }
    if (k == '\t' && norder > 1) { focus(order[0]); return; }   /* cycle windows */
    int t = top_win();
    if (t >= 0 && wins[t].key) wins[t].key(&wins[t], k);
}

static void gui_mouse(void) {
    input_events++;
    int x = mouse.x, y = mouse.y, down = mouse.buttons & 1, pressed = down && !(last_buttons & 1);
    last_buttons = mouse.buttons;
    if (drag >= 0) {
        if (!down) { drag = -1; return; }
        struct win* w = &wins[drag];
        w->x = imax(-w->w + 80, imin(fb_w - 80, x - drag_dx));
        w->y = imax(BAR_H, imin(fb_h - TITLE_H, y - drag_dy));
        return;
    }
    if (!pressed) return;
    if (y < BAR_H) {
        for (int i = 0; i < NWIN; i++) {
            struct btn* b = &bar_btns[i];
            if (x >= b->x && x < b->x + b->w) { if (top_win() == i) close_win(i); else focus(i); return; }
        }
        return;
    }
    for (int j = norder - 1; j >= 0; j--) {
        int i = order[j]; struct win* w = &wins[i];
        if (x < w->x || x >= w->x + w->w || y < w->y || y >= w->y + w->h) continue;
        focus(i);
        if (y < w->y + TITLE_H) {
            if (x >= w->x + w->w - 30) { close_win(i); return; }
            drag = i; drag_dx = x - w->x; drag_dy = y - w->y;
        }
        return;
    }
}

/* ── Boot sequence ───────────────────────────────────────────────────────── */
static void boot_frame(uint32_t t) {
    fb_clear(0x000000);
    static const char* post[] = {
        "ENCOM SYSTEMS BIOS  v12.0.4",
        "CPU  ................................",
        "MEMORY  .............................",
        "DISPLAY  ............................",
        "INTERRUPT CONTROLLER  ..... REMAPPED 0x20",
        "PIT TIMER  ................ 100 HZ",
        "PS/2 KEYBOARD + MOUSE  .... ONLINE",
        "LOADING GRID KERNEL  ...... OK",
    };
    char line[96], n[12];
    int shown = imin(8, (int)t / 9);
    if (t < 110) {
        for (int i = 0; i < shown; i++) {
            line[0] = 0; cat(line, post[i]);
            if (i == 1) { line[5] = 0; cat(line, sys.cpu[0] ? sys.cpu : "i686"); }
            if (i == 2) { line[8] = 0; utoa(sys.mem_kb / 1024, n); cat(line, n); cat(line, " MIB OK"); }
            if (i == 3) { line[9] = 0; utoa((uint32_t)fb_w, n); cat(line, n); cat(line, "X"); utoa((uint32_t)fb_h, n); cat(line, n); cat(line, "X32  "); cat(line, fb_source); }
            font_draw(&font_mono, 40, 40 + i * 20, line, i == 0 ? C_INK : C_CYAN, 0);
        }
        if ((t / 25) % 2 == 0) fb_fill(40, 40 + shown * 20 + 3, 8, 14, C_CYAN);
    } else {
        uint32_t u = t - 110;                                   /* logo phase */
        int a = (int)imin(255, (int)u * 8);
        int tw = font_width(&font_big, "ENCOM", 18);
        int x = (fb_w - tw) / 2, y = fb_h / 2 - 60;
        font_draw(&font_big, x, y, "ENCOM", rgb(a * 0xDF / 255, a * 0xF8 / 255, a), 18);
        int lw = (int)imin(tw, (int)u * tw / 70);
        fb_hline(x, y + 64, tw, 0x06202A);
        fb_hline(x + (tw - lw) / 2, y + 64, lw, C_CYAN);
        if (u > 30) font_draw(&font_bold, (fb_w - font_width(&font_bold, "OS-12", 10)) / 2, y + 80, "OS-12", C_CYAN, 10);
        if (u > 60) font_draw(&font_ui, (fb_w - font_width(&font_ui, "entering the grid", 1)) / 2, y + 108, "entering the grid", C_DIM, 1);
    }
    fb_flip();
}

void gui_boot_sequence(void) {
    uint32_t start = timer_ticks, last = 0xFFFFFFFF;
    serial_write("ENCOM OS-12 boot sequence\n");
    while (timer_ticks - start < 200) {
        if (keyboard_available()) { keyboard_getchar(); break; }
        uint32_t t = timer_ticks - start;
        if (t != last) { last = t; boot_frame(t); }
        __asm__ volatile ("hlt");
    }
}

/* ── Main loop ───────────────────────────────────────────────────────────── */
void gui_run(void) {
    wins[W_TERM]  = (struct win){ "TERMINAL",   "terminal",   32,  150, 0, 420, 0, term_draw,  term_key };
    wins[W_TERM].w = TCOLS * font_mono.adv[0] + 22;
    wins[W_SYS]   = (struct win){ "SYSTEM",     "system",     fb_w - 520, 62, 490, 330, 0, sys_draw, 0 };
    wins[W_CYCLE] = (struct win){ "LIGHTCYCLE", "lightcycle", 180, 150, GW * CELL + 26, GH * CELL + 76, 0, cycle_draw, cycle_key };
    wins[W_ID]    = (struct win){ "IDENTITY",   "identity",   fb_w / 2 - 280, fb_h - 300, 560, 240, 0, id_draw, 0 };
    lc_reset();
    rng ^= timer_ticks * 2654435761u;

    shell_out = term_out; shell_clear = term_clear; shell_open_app = open_by_name;
    term_out("ENCOM OS-12  /  Antigravity kernel v0.4\n", S_ACCENT);
    term_out("Type 'help' for commands. F1-F4 switch apps, Tab cycles windows.\n\n", S_DIM);
    shell_prompt();
    focus(W_SYS); focus(W_TERM);
    serial_write("DESKTOP READY\n");

    uint32_t last_frame = 0, last_ev = mouse.events, last_sec = timer_ticks, ev_at_sec = 0;
    for (;;) {
        __asm__ volatile ("cli");
        if (!keyboard_available() && mouse.events == last_ev && timer_ticks - last_frame < 3) {
            __asm__ volatile ("sti; hlt");
            continue;
        }
        __asm__ volatile ("sti");
        while (keyboard_available()) gui_key((unsigned char)keyboard_getchar());
        if (mouse.events != last_ev) { last_ev = mouse.events; gui_mouse(); }
        if (timer_ticks - last_frame >= 3) { last_frame = timer_ticks; render(); }
        if (timer_ticks - last_sec >= TIMER_HZ) {
            last_sec = timer_ticks; fps = frames; frames = 0;
            for (int i = 0; i < 59; i++) { hist_fps[i] = hist_fps[i + 1]; hist_ev[i] = hist_ev[i + 1]; }
            hist_fps[59] = fps; hist_ev[59] = input_events - ev_at_sec; ev_at_sec = input_events;
        }
    }
}
