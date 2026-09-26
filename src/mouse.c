/* PS/2 mouse (IRQ12): 3-byte packets -> absolute cursor position clamped to the screen */
#include "mouse.h"
#include "pic.h"
#include "io.h"

struct mouse_state mouse;
static int maxw = 1024, maxh = 768;
static uint8_t packet[3];
static int cycle = 0;

static void wait_write(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {} }
static void wait_read(void)  { for (int i = 0; i < 100000 && !(inb(0x64) & 1); i++) {} }
static void mouse_cmd(uint8_t c) {
    wait_write(); outb(0x64, 0xD4);   /* next byte -> aux device */
    wait_write(); outb(0x60, c);
    wait_read();  inb(0x60);          /* ACK */
}

void mouse_init(int w, int h) {
    maxw = w; maxh = h; mouse.x = w / 2; mouse.y = h / 2;
    wait_write(); outb(0x64, 0xA8);                 /* enable aux port */
    wait_write(); outb(0x64, 0x20);                 /* read controller config */
    wait_read();  uint8_t cfg = inb(0x60);
    cfg |= 0x02;                                    /* IRQ12 on */
    cfg &= (uint8_t)~0x20;                          /* aux clock on */
    wait_write(); outb(0x64, 0x60);
    wait_write(); outb(0x60, cfg);
    mouse_cmd(0xF6);                                /* defaults */
    mouse_cmd(0xF4);                                /* start streaming */
}

void mouse_handler(void) {
    uint8_t st = inb(0x64);
    if (st & 0x20) {                                /* byte came from the mouse */
        uint8_t b = inb(0x60);
        if (cycle == 0 && !(b & 0x08)) { pic_send_eoi(12); return; }  /* resync */
        packet[cycle++] = b;
        if (cycle == 3) {
            cycle = 0;
            if (!(packet[0] & 0xC0)) {              /* ignore overflow packets */
                int dx = (int)packet[1] - ((packet[0] & 0x10) ? 256 : 0);
                int dy = (int)packet[2] - ((packet[0] & 0x20) ? 256 : 0);
                mouse.x += dx; mouse.y -= dy;
                if (mouse.x < 0) mouse.x = 0;
                if (mouse.x >= maxw) mouse.x = maxw - 1;
                if (mouse.y < 0) mouse.y = 0;
                if (mouse.y >= maxh) mouse.y = maxh - 1;
                mouse.buttons = packet[0] & 7;
                mouse.events++;
            }
        }
    }
    pic_send_eoi(12);
}
