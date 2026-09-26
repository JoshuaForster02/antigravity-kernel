/* panic.c — CPU exception handler: "DEREZZED" screen instead of a silent triple fault */
#include "panic.h"
#include "terminal.h"
#include "system.h"
#include "fb.h"
#include "font.h"
#include "serial.h"
#include <stdint.h>

static const char* const exception_names[32] = {
    "Division by zero",        "Debug",                  "Non-maskable interrupt", "Breakpoint",
    "Overflow",                "Bound range exceeded",   "Invalid opcode",         "Device not available",
    "Double fault",            "Coprocessor overrun",    "Invalid TSS",            "Segment not present",
    "Stack-segment fault",     "General protection",     "Page fault",             "Reserved",
    "x87 floating-point",      "Alignment check",        "Machine check",          "SIMD floating-point",
    "Virtualization",          "Control protection",     "Reserved",               "Reserved",
    "Reserved",                "Reserved",               "Reserved",               "Reserved",
    "Hypervisor injection",    "VMM communication",      "Security",               "Reserved",
};

static void write_hex(uint32_t v) {
    static const char digits[] = "0123456789ABCDEF";
    char buf[11] = "0x";
    for (int i = 0; i < 8; i++)
        buf[2 + i] = digits[(v >> (28 - 4 * i)) & 0xF];
    buf[10] = '\0';
    terminal_writestring(buf);
}

static void field(const char* name, uint32_t v) {
    terminal_setcolor(vga_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring(name);
    terminal_setcolor(vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    write_hex(v);
}

/* Graphical variant: same content, drawn straight into the framebuffer */
static char* hex(uint32_t v, char* buf) {
    static const char d[] = "0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 8; i++) buf[2 + i] = d[(v >> (28 - 4 * i)) & 0xF];
    buf[10] = 0; return buf;
}
static void gfx_panic(struct regs_t* r) {
    char b[12];
    fb_noclip();
    fb_clear(0x0A0000);
    int x = 80, y = 80;
    font_draw(&font_big, x, y, "DEREZZED", 0xFF5A4A, 10);
    fb_hline(x, y + 64, fb_w - 2 * x, 0x7A1A12);
    font_draw(&font_ui, x, y + 80, "The grid encountered an unrecoverable fault.", 0xFFD7CC, 0);
    font_draw(&font_bold, x, y + 124, "EXCEPTION", 0xFF9A1F, 2);
    font_draw(&font_ui, x + 110, y + 122, exception_names[r->int_no & 31], 0xFFFFFF, 0);
    const char* names[] = { "INT", "ERR", "EIP", "CS", "EFLAGS", "EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "ESP" };
    uint32_t vals[] = { r->int_no, r->err_code, r->eip, r->cs, r->eflags, r->eax, r->ebx, r->ecx, r->edx, r->esi, r->edi, r->ebp, r->esp };
    for (int i = 0; i < 13; i++) {
        int cx = x + (i % 2) * 260, cy = y + 164 + (i / 2) * 24;
        font_draw(&font_bold, cx, cy + 1, names[i], 0xB06A60, 2);
        font_draw(&font_mono, cx + 80, cy, hex(vals[i], b), 0xFFFFFF, 0);
    }
    font_draw(&font_ui, x, fb_h - 80, "System halted. Reset the machine to re-enter the grid.", 0x8A5A54, 0);
    fb_flip();
}

void exception_handler(struct regs_t* r) {
    __asm__ volatile ("cli");
    serial_write("\nDEREZZED: "); serial_write(exception_names[r->int_no & 31]); serial_write("\n");
    if (sys.gfx) {
        gfx_panic(r);
        for (;;) __asm__ volatile ("cli; hlt");
    }
    terminal_setcolor(vga_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_clear();
    terminal_writestring(
        "+==============================================================+\n"
        "|                  D E R E Z Z E D                             |\n"
        "|         the grid encountered an unrecoverable fault          |\n"
        "+==============================================================+\n\n");

    terminal_setcolor(vga_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    terminal_writestring("  EXCEPTION  ");
    terminal_writestring(exception_names[r->int_no & 31]);
    terminal_writestring("\n\n");

    field("  INT    ", r->int_no);   field("    ERR    ", r->err_code); terminal_putchar('\n');
    field("  EIP    ", r->eip);      field("    CS     ", r->cs);       terminal_putchar('\n');
    field("  EFLAGS ", r->eflags);   terminal_writestring("\n\n");
    field("  EAX    ", r->eax);      field("    EBX    ", r->ebx);      terminal_putchar('\n');
    field("  ECX    ", r->ecx);      field("    EDX    ", r->edx);      terminal_putchar('\n');
    field("  ESI    ", r->esi);      field("    EDI    ", r->edi);      terminal_putchar('\n');
    field("  EBP    ", r->ebp);      field("    ESP    ", r->esp);      terminal_putchar('\n');

    terminal_setcolor(vga_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\n  System halted. Reset the machine to re-enter the grid.\n");

    for (;;)
        __asm__ volatile ("cli; hlt");
}
