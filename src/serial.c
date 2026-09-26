#include "serial.h"
#include "io.h"

#define COM1 0x3F8
static int serial_ok = 0;

void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* no interrupts            */
    outb(COM1 + 3, 0x80);   /* DLAB on                  */
    outb(COM1 + 0, 0x01);   /* divisor 1 -> 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8N1, DLAB off            */
    outb(COM1 + 2, 0xC7);   /* FIFO on, cleared         */
    outb(COM1 + 4, 0x0B);
    serial_ok = inb(COM1 + 5) != 0xFF;   /* 0xFF = no UART present */
}

void serial_putc(char c) {
    if (!serial_ok) return;
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++) {}
    outb(COM1, (uint8_t)c);
}

void serial_write(const char* s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}
