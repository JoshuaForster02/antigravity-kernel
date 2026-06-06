#include "terminal.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_BUFFER ((uint16_t*)0xB8000)

static size_t  term_row;
static size_t  term_col;
static uint8_t term_color;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(unsigned char)c | (uint16_t)color << 8;
}

/* Scroll the entire screen up one line */
static void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_BUFFER[y * VGA_WIDTH + x] = VGA_BUFFER[(y + 1) * VGA_WIDTH + x];

    /* Clear the last line */
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);

    term_row = VGA_HEIGHT - 1;
}

void terminal_initialize(void) {
    term_row   = 0;
    term_col   = 0;
    term_color = vga_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
}

void terminal_setcolor(uint8_t color) {
    term_color = color;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\r') {
        term_col = 0;
    } else if (c == '\b') {
        terminal_delete_last();
        return;
    } else {
        VGA_BUFFER[term_row * VGA_WIDTH + term_col] = vga_entry(c, term_color);
        if (++term_col >= VGA_WIDTH) {
            term_col = 0;
            term_row++;
        }
    }
    if (term_row >= VGA_HEIGHT)
        terminal_scroll();
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* s) {
    terminal_write(s, strlen(s));
}

void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
    term_row = 0;
    term_col = 0;
}

/* Erase the character to the left of the cursor */
void terminal_delete_last(void) {
    if (term_col > 0) {
        term_col--;
    } else if (term_row > 0) {
        term_row--;
        term_col = VGA_WIDTH - 1;
    }
    VGA_BUFFER[term_row * VGA_WIDTH + term_col] = vga_entry(' ', term_color);
}
