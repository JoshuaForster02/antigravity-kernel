#pragma once
#include <stdint.h>

struct idt_entry_t {
    uint16_t base_lo;   /* lower 16 bits of handler address  */
    uint16_t sel;       /* kernel code segment selector      */
    uint8_t  always0;
    uint8_t  flags;     /* type and attributes               */
    uint16_t base_hi;   /* upper 16 bits of handler address  */
} __attribute__((packed));

struct idt_ptr_t {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);
