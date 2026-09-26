#pragma once
/* ENCOM OS-12 desktop: boot sequence, window manager, apps. Requires fb_init() == 1. */
void gui_boot_sequence(void);   /* ~3 s intro, any key skips */
void gui_run(void);             /* desktop main loop, never returns */
