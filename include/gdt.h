#pragma once
#include <stdint.h>

/* Flat 4 GiB segments. GRUB's GDT is not guaranteed to stay valid (Multiboot spec),
   so the kernel loads its own before touching the IDT. */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10

void gdt_init(void);
