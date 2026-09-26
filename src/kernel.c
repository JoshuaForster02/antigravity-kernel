/* kernel.c — ENCOM OS-12 (Antigravity kernel)
 * Boot: GDT -> IDT -> PIC -> PIT/keyboard/mouse -> graphical desktop.
 * Without a linear framebuffer it falls back to the VGA text-mode shell.
 */
#include "terminal.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "timer.h"
#include "mouse.h"
#include "serial.h"
#include "fb.h"
#include "gui.h"
#include "shell.h"
#include "system.h"
#include "multiboot.h"
#include "io.h"
#include <stdint.h>

struct sysinfo sys = { 0, "unknown", 0, "" };

static void cpu_brand(char* out) {
    uint32_t r[4];
    __asm__ volatile ("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(0x80000000));
    if (r[0] < 0x80000004) { out[0] = 0; return; }
    uint32_t* o = (uint32_t*)out;
    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++, o += 4)
        __asm__ volatile ("cpuid" : "=a"(o[0]), "=b"(o[1]), "=c"(o[2]), "=d"(o[3]) : "a"(leaf));
    out[48] = 0;
    char* s = out; while (*s == ' ') s++;               /* brand strings are often left-padded */
    char* d = out; while (*s) *d++ = *s++; *d = 0;
}

/* ── VGA text-mode console (fallback when there is no framebuffer) ─────────── */
static const uint8_t text_colors[] = {
    VGA_COLOR_WHITE, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_LIGHT_RED, VGA_COLOR_DARK_GREY };
static void text_out(const char* s, int style) {
    terminal_setcolor(vga_color(text_colors[style], VGA_COLOR_BLACK));
    terminal_writestring(s);
    serial_write(s);
}

static void text_mode_shell(void) {
    terminal_initialize();
    terminal_setcolor(vga_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring(
        "+==============================================================+\n"
        "|                    E N C O M   O S - 1 2                     |\n"
        "|           text mode  |  no linear framebuffer found          |\n"
        "+==============================================================+\n\n");
    shell_out = text_out; shell_clear = terminal_clear; shell_open_app = 0;
    shell_out("  Type 'help' for commands.\n\n", S_DIM);

    static char buf[256];
    int len = 0;
    shell_prompt();
    for (;;) {
        __asm__ volatile ("cli");
        if (!keyboard_available()) { __asm__ volatile ("sti; hlt"); continue; }
        __asm__ volatile ("sti");
        unsigned char c = (unsigned char)keyboard_getchar();
        if (c == '\n') { buf[len] = 0; terminal_putchar('\n'); shell_exec(buf); len = 0; shell_prompt(); }
        else if (c == '\b') { if (len > 0) { len--; terminal_delete_last(); } }
        else if (c >= 32 && c < 0x80 && len < 254) { buf[len++] = (char)c; terminal_putchar((char)c); }
    }
}

void kernel_main(uint32_t magic, struct multiboot_info* mbi) {
    serial_init();
    serial_write("\nENCOM OS-12 / Antigravity kernel v0.4\n");

    if (magic == MULTIBOOT_MAGIC && mbi) {
        if (mbi->flags & 1) sys.mem_kb = mbi->mem_lower + mbi->mem_upper;
        if (mbi->flags & (1u << 9)) sys.loader = (const char*)mbi->boot_loader_name;
    }
    cpu_brand(sys.cpu);

    gdt_init();
    idt_init();
    pic_remap(0x20, 0x28);
    timer_init();

    sys.gfx = fb_init(mbi, magic);
    if (sys.gfx) mouse_init(fb_w, fb_h);

    /* Unmask IRQ0 timer, IRQ1 keyboard, IRQ2 cascade (+ IRQ12 mouse in GUI mode) */
    outb(PIC1_DATA, 0xF8);
    outb(PIC2_DATA, sys.gfx ? 0xEF : 0xFF);
    __asm__ volatile ("sti");

    if (!sys.gfx) { serial_write("no framebuffer -> text mode\n"); text_mode_shell(); }
    serial_write("framebuffer: "); serial_write(fb_source); serial_write("\n");
    gui_boot_sequence();
    gui_run();
}
