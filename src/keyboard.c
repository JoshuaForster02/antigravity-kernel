#include "keyboard.h"
#include "pic.h"
#include "io.h"
#include <stdint.h>

/* US QWERTY PS/2 Set-1 scancode → ASCII
   Index = scancode byte (key-down only, bit7 = 0).
   0 means "no printable character". */
static const char scancode_ascii[128] = {
/*        0     1     2     3     4     5     6     7     8     9   */
/*0x00*/  0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',
/*0x0A*/ '9', '0', '-', '=','\b','\t', 'q', 'w', 'e', 'r',
/*0x14*/ 't', 'y', 'u', 'i', 'o', 'p', '[', ']','\n',  0,
/*0x1E*/ 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
/*0x28*/'\'', '`',  0, '\\','z', 'x', 'c', 'v', 'b', 'n',
/*0x32*/ 'm', ',', '.', '/',  0,  '*',  0,  ' ',
/*0x3A-0x7F: F-keys, arrows, etc. — not mapped yet */
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0
};

#define KEY_BUF_SIZE 256
static volatile char key_buf[KEY_BUF_SIZE];
static volatile int  buf_head = 0;
static volatile int  buf_tail = 0;

/* Called from keyboard_handler_wrapper in interrupts.asm */
void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* Bit 7 set = key release event, ignore */
    if (!(scancode & 0x80)) {
        char c = scancode_ascii[scancode & 0x7F];
        if (c) {
            int next = (buf_head + 1) % KEY_BUF_SIZE;
            if (next != buf_tail) {          /* drop if buffer full */
                key_buf[buf_head] = c;
                buf_head = next;
            }
        }
    }

    pic_send_eoi(1);  /* IRQ1 */
}

int keyboard_available(void) {
    return buf_head != buf_tail;
}

/* Blocking read */
char keyboard_getchar(void) {
    while (!keyboard_available())
        __asm__ volatile ("hlt"); /* sleep until next interrupt */
    char c = key_buf[buf_tail];
    buf_tail = (buf_tail + 1) % KEY_BUF_SIZE;
    return c;
}
