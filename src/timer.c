#include "timer.h"
#include "pic.h"
#include "io.h"

volatile uint32_t timer_ticks = 0;

void timer_init(void) {
    uint32_t div = 1193182 / TIMER_HZ;
    outb(0x43, 0x36);                 /* channel 0, lo/hi, mode 3 (square wave) */
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}

void timer_handler(void) {
    timer_ticks++;
    pic_send_eoi(0);
}
