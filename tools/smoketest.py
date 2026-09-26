#!/usr/bin/env python3
"""Headless boot smoke test for Antigravity OS.

Boots kernel.bin in QEMU, types commands via the QEMU monitor (sendkey),
reads the VGA text buffer at 0xB8000 back out of guest memory and checks
the screen contents. Exit code 0 = all checks passed.

Usage: tools/smoketest.py kernel.bin [--screens DIR]
"""
import os, socket, subprocess, sys, tempfile, time

KEYMAP = {" ": "spc", "\n": "ret", "-": "minus", "!": "shift-1", "?": "shift-slash"}

class VM:
    def __init__(self, kernel, tmp):
        self.tmp = tmp
        self.sock = os.path.join(tmp, "mon.sock")
        self.proc = subprocess.Popen(
            ["qemu-system-i386", "-kernel", kernel, "-display", "none", "-no-reboot",
             "-monitor", f"unix:{self.sock},server,nowait"],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        for _ in range(50):
            if os.path.exists(self.sock):
                break
            time.sleep(0.1)
        self.mon = socket.socket(socket.AF_UNIX)
        self.mon.connect(self.sock)
        self.mon.settimeout(0.05)

    def _drain(self):
        try:
            while self.mon.recv(65536):
                pass
        except OSError:
            pass

    def cmd(self, line, wait=0.1):
        self.mon.sendall((line + "\n").encode())
        time.sleep(wait)
        self._drain()

    def type(self, text):
        for ch in text:
            key = KEYMAP.get(ch) or (f"shift-{ch.lower()}" if ch.isupper() else ch)
            self.cmd("sendkey " + key, 0.06)
        time.sleep(0.3)

    def screen(self):
        dump = os.path.join(self.tmp, "vga.bin")
        if os.path.exists(dump):
            os.remove(dump)
        self.cmd(f'pmemsave 0xb8000 4000 "{dump}"', 0.3)
        for _ in range(30):
            if os.path.exists(dump) and os.path.getsize(dump) == 4000:
                break
            time.sleep(0.1)
        raw = open(dump, "rb").read()
        return "\n".join(
            "".join(chr(raw[(r * 80 + c) * 2]) if 32 <= raw[(r * 80 + c) * 2] < 127 else " "
                    for c in range(80)).rstrip()
            for r in range(25))

    def alive(self):
        return self.proc.poll() is None

    def close(self):
        self.proc.kill()
        self.proc.wait()

def main():
    kernel = sys.argv[1] if len(sys.argv) > 1 else "kernel.bin"
    screens = sys.argv[sys.argv.index("--screens") + 1] if "--screens" in sys.argv else None
    failures = []

    def check(name, cond, scr):
        print(("  PASS  " if cond else "  FAIL  ") + name)
        if not cond:
            failures.append(name)
            print("\n".join("        | " + l for l in scr.splitlines() if l.strip()))

    with tempfile.TemporaryDirectory() as tmp:
        vm = VM(kernel, tmp)
        try:
            time.sleep(1.0)
            s = vm.screen()
            check("boots to splash + prompt", "A N T I G R A V I T Y" in s and "[flynn@antigravity]>" in s, s)
            check("all subsystems report OK", s.count("[ OK ]") == 4 and "[ ACTIVE ]" in s, s)

            vm.type("help\n")
            s = vm.screen()
            check("help lists commands", "sysinfo" in s and "derez" in s, s)

            vm.type("clear\n")
            vm.type("echo Hi There!\n")
            s = vm.screen()
            check("shift: uppercase + symbols", "Hi There!" in s, s)

            vm.type("foo\n")
            s = vm.screen()
            check("unknown command handled", "Unknown command: foo" in s, s)

            vm.type("derez\n")
            s = vm.screen()
            check("CPU exception -> panic screen", "D E R E Z Z E D" in s and "Division by zero" in s, s)
            check("VM still running after exception (no triple fault)", vm.alive(), s)
            if screens:
                vm.cmd(f'screendump "{os.path.abspath(os.path.join(screens, "panic.ppm"))}"', 0.5)
        finally:
            vm.close()

    print(f"\n{'OK' if not failures else 'FAILED'}: {7 - len(failures)}/7 checks passed")
    sys.exit(1 if failures else 0)

if __name__ == "__main__":
    main()
