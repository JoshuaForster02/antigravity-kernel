#pragma once
#include <stdint.h>

/* Facts gathered at boot, shown by `sysinfo` and the SYSTEM app */
struct sysinfo {
    uint32_t mem_kb;          /* usable RAM from Multiboot (lower + upper) */
    const char* loader;       /* bootloader name */
    int gfx;                  /* 1 = graphical desktop, 0 = VGA text mode */
    char cpu[49];             /* CPUID brand string */
};
extern struct sysinfo sys;
