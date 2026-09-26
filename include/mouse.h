#pragma once
#include <stdint.h>
struct mouse_state { int x, y, buttons; volatile uint32_t events; };
extern struct mouse_state mouse;
void mouse_init(int w, int h);
void mouse_handler(void);   /* IRQ12 */
