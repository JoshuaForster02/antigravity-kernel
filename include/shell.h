#pragma once
/* Output styles, mapped to VGA colors in text mode and to palette colors in the GUI terminal */
enum { S_NORMAL, S_ACCENT, S_OK, S_ERR, S_DIM };

/* Implemented by the active console (text mode or GUI terminal) */
extern void (*shell_out)(const char* s, int style);
extern void (*shell_clear)(void);

void shell_exec(const char* cmd);
void shell_prompt(void);
/* Hook for `open <app>` in GUI mode; returns 0 if unknown */
extern int (*shell_open_app)(const char* name);
