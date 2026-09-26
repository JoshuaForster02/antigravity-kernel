#pragma once
/* COM1 debug/log output (115200 8N1). Mirrors the shell so tests can read it headless. */
void serial_init(void);
void serial_write(const char* s);
void serial_putc(char c);
