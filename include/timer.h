#pragma once
#include <stdint.h>
#define TIMER_HZ 100
extern volatile uint32_t timer_ticks;
void timer_init(void);
void timer_handler(void);   /* IRQ0 */
