/* kernel.c — Antigravity OS  //  Flynn's OS aesthetic
 * Bare-metal i686 kernel: VGA text mode, keyboard input, mini-shell.
 * Logic is split into terminal.c | pic.c | idt.c | keyboard.c
 */
#include "terminal.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "io.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

/* ── Boot splash ──────────────────────────────────────────────────────────── */
static void print_splash(void) {
    terminal_setcolor(vga_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
    terminal_writestring(
        "+==============================================================+\n"
        "|                                                              |\n"
        "|       A N T I G R A V I T Y   O S   //   v 0 . 2           |\n"
        "|            bare-metal kernel  |  Flynn mode active          |\n"
        "|                                                              |\n"
        "+==============================================================+\n\n"
    );

    const char* labels[] = {
        "  >> MEMORY SUBSYSTEM      ",
        "  >> INTERRUPT DESCRIPTOR  ",
        "  >> PIC REMAPPED (0x20)   ",
        "  >> KEYBOARD DRIVER IRQ1  ",
        "  >> INTERRUPTS            ",
        0
    };
    const char* status[] = {
        "[ OK ]\n",
        "[ OK ]\n",
        "[ OK ]\n",
        "[ OK ]\n",
        "[ ACTIVE ]\n",
        0
    };
    for (int i = 0; labels[i]; i++) {
        terminal_setcolor(vga_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring(labels[i]);
        terminal_setcolor(vga_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring(status[i]);
    }

    terminal_setcolor(vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n  All systems online.  Type 'help' for commands.\n\n");
}

/* ── Shell helpers ────────────────────────────────────────────────────────── */
static void print_prompt(void) {
    terminal_setcolor(vga_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[flynn@antigravity]> ");
    terminal_setcolor(vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

static void exec_cmd(const char* cmd) {
    terminal_putchar('\n');

    if (strcmp(cmd, "") == 0) {
        /* empty */
    } else if (strcmp(cmd, "help") == 0) {
        terminal_setcolor(vga_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("  Commands:\n");
        terminal_setcolor(vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring(
            "    help     -- show this message\n"
            "    clear    -- clear the screen\n"
            "    sysinfo  -- system information\n"
            "    about    -- about Antigravity OS\n"
            "    echo     -- echo text  (echo <text>)\n"
            "    reboot   -- reboot the system\n"
            "    halt     -- halt the CPU\n\n"
        );
    } else if (strcmp(cmd, "clear") == 0) {
        terminal_clear();
    } else if (strcmp(cmd, "sysinfo") == 0) {
        terminal_setcolor(vga_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("  SYSTEM INFORMATION\n");
        terminal_setcolor(vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring(
            "  OS        Antigravity OS v0.2\n"
            "  Arch      i686  (32-bit protected mode)\n"
            "  Bootload  GRUB Multiboot\n"
            "  Kernel    bare-metal, no libc\n"
            "  VGA       80x25 text mode @ 0xB8000\n"
            "  Stack     16 KiB BSS\n"
            "  Load addr 1 MiB\n\n"
        );
    } else if (strcmp(cmd, "about") == 0) {
        terminal_setcolor(vga_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("  Antigravity OS  --  Inspired by Flynn's OS (TRON Legacy)\n");
        terminal_setcolor(vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring(
            "  A bare-metal hobby kernel written in C and x86 Assembly.\n"
            "  Part of the Antigravity project.\n"
            "  github.com/JoshuaForster02/antigravity-kernel\n\n"
        );
    } else if (strcmp(cmd, "reboot") == 0) {
        terminal_writestring("  Rebooting ...\n");
        /* Pulse CPU reset via keyboard controller */
        uint8_t v = 0x02;
        while (v & 0x02) v = inb(0x64);
        outb(0x64, 0xFE);
        /* Fallback: triple fault */
        __asm__ volatile ("cli");
        struct idt_ptr_t null_idt = {0, 0};
        __asm__ volatile ("lidt (%0)" : : "r"(&null_idt));
        __asm__ volatile ("int $3");
    } else if (strcmp(cmd, "halt") == 0) {
        terminal_writestring("  System halted. Safe to power off.\n");
        __asm__ volatile ("cli; hlt");
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        terminal_writestring("  ");
        terminal_writestring(cmd + 5);
        terminal_writestring("\n\n");
    } else {
        terminal_setcolor(vga_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("  Unknown command: ");
        terminal_writestring(cmd);
        terminal_writestring("\n  Type 'help' for commands.\n\n");
        terminal_setcolor(vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
}

/* ── Entry point ──────────────────────────────────────────────────────────── */
void kernel_main(void) {
    terminal_initialize();
    print_splash();

    idt_init();
    pic_remap(0x20, 0x28);

    /* Mask all IRQs except IRQ1 (keyboard) */
    outb(PIC1_DATA, 0xFD);  /* 1111 1101 */
    outb(PIC2_DATA, 0xFF);

    __asm__ volatile ("sti");

    /* Shell loop */
    static char buf[256];
    int len = 0;
    print_prompt();

    while (1) {
        if (keyboard_available()) {
            char c = keyboard_getchar();
            if (c == '\n') {
                buf[len] = '\0';
                exec_cmd(buf);
                len = 0;
                print_prompt();
            } else if (c == '\b') {
                if (len > 0) { len--; terminal_delete_last(); }
            } else if (len < 254) {
                buf[len++] = c;
                terminal_putchar(c);
            }
        }
    }
}
