# Antigravity OS — kernel

Bare-metal i686 hobby kernel (C + x86 assembly), TRON/Flynn aesthetic. Multiboot, VGA text mode, PS/2 keyboard, mini shell.

## Build & run

```bash
sudo apt install nasm gcc-multilib qemu-system-x86   # or an i686-elf cross compiler
make          # -> kernel.bin (uses i686-elf-gcc if present, else gcc -m32)
make run      # boot in QEMU
make test     # headless boot smoke test (also runs in CI)
```

Docker: `docker build -t antigravity-dev . && docker run --rm -v "$PWD":/os antigravity-dev make test`

## Shell

`help` · `clear` · `sysinfo` · `about` · `echo <text>` · `derez` (triggers a CPU exception → panic screen) · `reboot` · `halt`

## Boot sequence

1. `boot.asm` — Multiboot header, 16 KiB stack, calls `kernel_main`
2. `gdt.c` — own flat GDT (GRUB's is not guaranteed valid after handoff)
3. `idt.c` — exceptions 0–31 → `panic.c` ("DEREZZED" screen with registers), IRQ1 → keyboard
4. `pic.c` — remap PIC to 0x20/0x28, only IRQ1 unmasked
5. shell loop sleeps with `hlt` between keystrokes

## Test

`tools/smoketest.py` boots the kernel in QEMU, types commands via the QEMU monitor, reads the VGA buffer (0xB8000) back out of guest memory and checks: splash + prompt, `help`, Shift/symbols, unknown command, CPU exception → panic screen without triple fault.
