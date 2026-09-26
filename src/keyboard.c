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

/* Same layout with Shift held */
static const char scancode_ascii_shift[128] = {
/*        0     1     2     3     4     5     6     7     8     9   */
/*0x00*/  0,   27,  '!', '@', '#', '$', '%', '^', '&', '*',
/*0x0A*/ '(', ')', '_', '+','\b','\t', 'Q', 'W', 'E', 'R',
/*0x14*/ 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}','\n',  0,
/*0x1E*/ 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
/*0x28*/ '"', '~',  0,  '|', 'Z', 'X', 'C', 'V', 'B', 'N',
/*0x32*/ 'M', '<', '>', '?',  0,  '*',  0,  ' ',
};

#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CAPSLOCK 0x3A
#define SC_RELEASE  0x80

volatile uint32_t keyboard_irqs = 0;
static volatile int shift_held = 0;
static volatile int caps_lock  = 0;

#define KEY_BUF_SIZE 256
static volatile char key_buf[KEY_BUF_SIZE];
static volatile int  buf_head = 0;
static volatile int  buf_tail = 0;

/* Called from keyboard_handler_wrapper in interrupts.asm */
void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    uint8_t key      = scancode & 0x7F;
    keyboard_irqs++;

    /* Modifiers: Shift is tracked on press and release, Caps Lock toggles on press */
    if (key == SC_LSHIFT || key == SC_RSHIFT) {
        shift_held = !(scancode & SC_RELEASE);
    } else if (scancode == SC_CAPSLOCK) {
        caps_lock = !caps_lock;
    } else if (!(scancode & SC_RELEASE)) {   /* bit 7 set = key release, ignore */
        char c = shift_held ? scancode_ascii_shift[key] : scancode_ascii[key];
        /* Arrows (E0-prefixed; the E0 byte itself is dropped as a 'release') and F1-F4 */
        switch (scancode) {
            case 0x48: c = (char)K_UP;    break;
            case 0x50: c = (char)K_DOWN;  break;
            case 0x4B: c = (char)K_LEFT;  break;
            case 0x4D: c = (char)K_RIGHT; break;
            case 0x3B: case 0x3C: case 0x3D: case 0x3E: c = (char)(K_F1 + scancode - 0x3B); break;
        }
        /* Caps Lock inverts case for letters only */
        if (caps_lock && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            c ^= 0x20;
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
