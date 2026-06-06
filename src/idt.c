#include "idt.h"
#include <stdint.h>

#define IDT_ENTRIES 256

static struct idt_entry_t idt[IDT_ENTRIES];
static struct idt_ptr_t   idt_ptr;

/* Defined in interrupts.asm */
extern void keyboard_handler_wrapper(void);
extern void ignore_handler(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

void idt_init(void) {
    idt_ptr.limit = sizeof(struct idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint32_t)&idt;

    /* Fill all entries with a safe no-op handler */
    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate((uint8_t)i, (uint32_t)ignore_handler, 0x08, 0x8E);

    /* IRQ1 (keyboard) remapped to interrupt 0x21 = 33 */
    idt_set_gate(0x21, (uint32_t)keyboard_handler_wrapper, 0x08, 0x8E);

    __asm__ volatile ("lidt (%0)" : : "r"(&idt_ptr));
}
