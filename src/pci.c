#include "pci.h"
#include "io.h"

static inline void outl(uint16_t p, uint32_t v) { __asm__ volatile ("outl %0, %1" : : "a"(v), "Nd"(p)); }
static inline uint32_t inl(uint16_t p) { uint32_t r; __asm__ volatile ("inl %1, %0" : "=a"(r) : "Nd"(p)); return r; }

uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    outl(0xCF8, 0x80000000u | (uint32_t)bus << 16 | (uint32_t)dev << 11 | (uint32_t)fn << 8 | (off & 0xFC));
    return inl(0xCFC);
}

uint32_t pci_find_bar0(uint16_t vendor, uint16_t device) {
    for (int bus = 0; bus < 256; bus++)
        for (int dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read(bus, dev, 0, 0);
            if ((id & 0xFFFF) == vendor && (id >> 16) == device)
                return pci_read(bus, dev, 0, 0x10) & 0xFFFFFFF0u;
        }
    return 0;
}
