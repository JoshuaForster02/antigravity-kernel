#include "gdt.h"
#include <stdint.h>

struct gdt_entry_t {
    uint16_t limit_lo;
    uint16_t base_lo;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;      /* flags (upper nibble) | limit bits 16-19 */
    uint8_t  base_hi;
} __attribute__((packed));

struct gdt_ptr_t {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry_t gdt[3];
static struct gdt_ptr_t   gdt_ptr;

/* Defined in interrupts.asm: lgdt + reload of all segment registers */
extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_lo  = base & 0xFFFF;
    gdt[i].base_mid = (base >> 16) & 0xFF;
    gdt[i].base_hi  = (base >> 24) & 0xFF;
    gdt[i].limit_lo = limit & 0xFFFF;
    gdt[i].gran     = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access   = access;
}

void gdt_init(void) {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    gdt_set(0, 0, 0,          0x00, 0x00);  /* null descriptor            */
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  /* 0x08: ring 0 code, 4 KiB gran */
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  /* 0x10: ring 0 data          */

    gdt_flush((uint32_t)&gdt_ptr);
}
