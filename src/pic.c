#include "pic.h"
#include "io.h"
#include <stdint.h>

#define ICW1_ICW4    0x01
#define ICW1_INIT    0x10
#define ICW4_8086    0x01

/*
 * Remap the PIC so IRQ0-7 map to interrupts offset1..offset1+7
 * and IRQ8-15 map to offset2..offset2+7.
 * Standard: offset1=0x20, offset2=0x28 (avoids collision with CPU exceptions 0-31).
 */
void pic_remap(int offset1, int offset2) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Start init sequence (cascade mode) */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();

    /* Set vector offsets */
    outb(PIC1_DATA, (uint8_t)offset1); io_wait();
    outb(PIC2_DATA, (uint8_t)offset2); io_wait();

    /* Tell PICs about each other */
    outb(PIC1_DATA, 4); io_wait();  /* PIC1: slave on IRQ2 */
    outb(PIC2_DATA, 2); io_wait();  /* PIC2: cascade identity */

    /* 8086 mode */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/* Signal End-Of-Interrupt */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) | (uint8_t)(1 << irq));
}

void pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) & ~(uint8_t)(1 << irq));
}
