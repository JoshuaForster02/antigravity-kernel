#pragma once

#define KEYBOARD_DATA_PORT 0x60

/* Non-ASCII keys are delivered as codes >= 0x80 (read the char as unsigned) */
#define K_UP    0x80
#define K_DOWN  0x81
#define K_LEFT  0x82
#define K_RIGHT 0x83
#define K_F1    0x84
#define K_F2    0x85
#define K_F3    0x86
#define K_F4    0x87

#include <stdint.h>
extern volatile uint32_t keyboard_irqs;

void keyboard_handler(void);   /* called from interrupts.asm wrapper */
char keyboard_getchar(void);   /* blocking read                       */
int  keyboard_available(void); /* non-blocking: 1 if char waiting     */
