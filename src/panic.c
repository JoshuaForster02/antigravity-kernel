/* panic.c — CPU exception handler: "DEREZZED" screen instead of a silent triple fault */
#include "panic.h"
#include "terminal.h"
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

void exception_handler(struct regs_t* r) {
    __asm__ volatile ("cli");
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
