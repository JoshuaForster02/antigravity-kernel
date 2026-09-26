#pragma once
#include <stdint.h>

/* Register snapshot pushed by isr_common in interrupts.asm */
struct regs_t {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  /* pushad */
    uint32_t int_no, err_code;                        /* pushed by the ISR stub */
    uint32_t eip, cs, eflags;                         /* pushed by the CPU */
};

void exception_handler(struct regs_t* r);   /* called from interrupts.asm, never returns */
