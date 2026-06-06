#pragma once

#define KEYBOARD_DATA_PORT 0x60

void keyboard_handler(void);   /* called from interrupts.asm wrapper */
char keyboard_getchar(void);   /* blocking read                       */
int  keyboard_available(void); /* non-blocking: 1 if char waiting     */
