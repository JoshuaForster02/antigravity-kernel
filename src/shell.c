/* shell.c — command interpreter shared by the VGA text console and the GUI terminal */
#include "shell.h"
#include "system.h"
#include "timer.h"
#include "rtc.h"
#include "fb.h"
#include "idt.h"
#include "io.h"
#include "serial.h"
#include "string.h"
#include <stdint.h>

void (*shell_out)(const char* s, int style);
void (*shell_clear)(void);
int  (*shell_open_app)(const char* name);

static void out(const char* s) { shell_out(s, S_NORMAL); }
static void utoa(uint32_t v, char* buf) {
    char t[12]; int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    for (int i = 0; i < n; i++) buf[i] = t[n - 1 - i];
    buf[n] = 0;
}
static void out_num(uint32_t v) { char b[12]; utoa(v, b); out(b); }
static void two(int v) { char b[3] = { (char)('0' + v / 10 % 10), (char)('0' + v % 10), 0 }; out(b); }

void shell_prompt(void) {
    shell_out("flynn@encom", S_ACCENT);
    shell_out(":~$ ", S_DIM);
}

static void uptime(void) {
    uint32_t s = timer_ticks / TIMER_HZ;
    out("  up "); two((int)(s / 3600)); out(":"); two((int)(s / 60 % 60)); out(":"); two((int)(s % 60));
    out("  ("); out_num(timer_ticks); out(" ticks @ "); out_num(TIMER_HZ); out(" Hz)\n");
}

void shell_exec(const char* cmd) {
    serial_write(cmd); serial_write("\n");
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;

    if (!strcmp(cmd, "help")) {
        shell_out("  COMMANDS\n", S_ACCENT);
        out("    help      this list\n"
            "    clear     clear the terminal\n"
            "    sysinfo   hardware and kernel facts\n"
            "    uptime    time since boot\n"
            "    date      real-time clock (UTC)\n"
            "    echo      print text\n"
            "    open      open an app: terminal, system, lightcycle, identity\n"
            "    about     about ENCOM OS-12\n"
            "    derez     trigger a CPU exception (panic screen)\n"
            "    reboot    restart the machine\n"
            "    halt      stop the CPU\n");
    } else if (!strcmp(cmd, "clear")) {
        shell_clear();
    } else if (!strcmp(cmd, "sysinfo")) {
        shell_out("  SYSTEM\n", S_ACCENT);
        out("  OS        ENCOM OS-12 (Antigravity kernel v0.4)\n");
        out("  CPU       "); out(sys.cpu[0] ? sys.cpu : "unknown"); out("\n");
        out("  Memory    "); out_num(sys.mem_kb / 1024); out(" MiB\n");
        out("  Loader    "); out(sys.loader); out("\n");
        out("  Display   ");
        if (sys.gfx) { out_num((uint32_t)fb_w); out("x"); out_num((uint32_t)fb_h); out("x32 via "); out(fb_source); }
        else out("VGA text 80x25");
        out("\n  Kernel    i686 protected mode, flat GDT, no paging\n");
        uptime();
    } else if (!strcmp(cmd, "uptime")) {
        uptime();
    } else if (!strcmp(cmd, "date")) {
        struct rtc_time t; rtc_read(&t);
        out("  "); out_num((uint32_t)t.year); out("-"); two(t.month); out("-"); two(t.day);
        out(" "); two(t.hour); out(":"); two(t.min); out(":"); two(t.sec); out(" UTC\n");
    } else if (!strcmp(cmd, "about")) {
        shell_out("  ENCOM OS-12\n", S_ACCENT);
        out("  A bare-metal hobby OS in C and x86 assembly, styled after the\n"
            "  operating systems of the TRON universe. Own kernel, own drivers,\n"
            "  own window manager. No Linux, no libc.\n"
            "  github.com/JoshuaForster02/antigravity-kernel\n");
    } else if (!strncmp(cmd, "open ", 5)) {
        if (!shell_open_app || !shell_open_app(cmd + 5)) shell_out("  no such app (try: terminal, system, lightcycle, identity)\n", S_ERR);
    } else if (!strcmp(cmd, "reboot")) {
        out("  Rebooting ...\n");
        uint8_t v = 0x02;
        while (v & 0x02) v = inb(0x64);
        outb(0x64, 0xFE);
        __asm__ volatile ("cli");
        struct idt_ptr_t null_idt = {0, 0};
        __asm__ volatile ("lidt (%0)" : : "r"(&null_idt));
        __asm__ volatile ("int $3");
    } else if (!strcmp(cmd, "halt")) {
        out("  System halted. Safe to power off.\n");
        serial_write("HALT\n");
        __asm__ volatile ("cli; hlt");
    } else if (!strcmp(cmd, "derez")) {
        /* Real #DE fault (not optimisable away) -> exception_handler */
        __asm__ volatile ("xor %%ecx, %%ecx\n\tdiv %%ecx" : : : "eax", "ecx", "edx");
    } else if (!strncmp(cmd, "echo ", 5)) {
        out("  "); out(cmd + 5); out("\n");
    } else {
        shell_out("  unknown command: ", S_ERR); shell_out(cmd, S_ERR);
        out("\n  type 'help' for commands\n");
    }
}
