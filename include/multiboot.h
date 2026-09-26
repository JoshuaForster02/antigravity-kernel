#pragma once
#include <stdint.h>

#define MULTIBOOT_MAGIC 0x2BADB002

/* Multiboot 1 information structure (only the fields the kernel reads) */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;          /* KiB, valid if flags bit 0 */
    uint32_t boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr;
    uint32_t config_table, boot_loader_name; /* name valid if flags bit 9 */
    uint32_t apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;               /* valid if flags bit 12 */
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t  framebuffer_bpp, framebuffer_type;
} __attribute__((packed));
