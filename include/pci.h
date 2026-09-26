#pragma once
#include <stdint.h>
uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off);
/* Returns BAR0 (masked) of the first device matching vendor:device, or 0 */
uint32_t pci_find_bar0(uint16_t vendor, uint16_t device);
