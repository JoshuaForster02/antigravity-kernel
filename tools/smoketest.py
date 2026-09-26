#!/usr/bin/env python3
"""Headless boot smoke test for ENCOM OS-12.

Boots the kernel (Multiboot via -kernel, or the GRUB ISO via -cdrom) in QEMU,
types commands through the QEMU monitor, and checks
  * the serial log (the shell mirrors all output to COM1),
  * the screen (screendump -> the desktop must actually be drawn in ENCOM cyan).
Exit code 0 = all checks passed.

Usage: tools/smoketest.py kernel.bin|encom-os.iso [--screens DIR]
"""
import os, socket, struct, subprocess, sys, tempfile, time

KEYMAP = {" ": "spc", "\n": "ret", "-": "minus", "!": "shift-1", "?": "shift-slash"}


class VM:
    def __init__(self, image, tmp):
        self.sock = os.path.join(tmp, "mon.sock")
        self.log = os.path.join(tmp, "serial.log")
        boot = ["-cdrom", image] if image.endswith(".iso") else ["-kernel", image]
        self.proc = subprocess.Popen(
            ["qemu-system-i386", *boot, "-m", "256", "-display", "none", "-no-reboot",
             "-serial", f"file:{self.log}", "-monitor", f"unix:{self.sock},server,nowait"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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
        time.sleep(0.4)

    def serial(self):
        try:
            return open(self.log, errors="replace").read()
        except FileNotFoundError:
            return ""

    def wait_for(self, text, timeout=15):
        end = time.time() + timeout
        while time.time() < end:
            if text in self.serial():
                return True
            time.sleep(0.2)
        return False

    def screen(self, path):
        """screendump (PPM) -> (width, height, share of ENCOM-cyan pixels)"""
        if os.path.exists(path):
            os.remove(path)
        self.cmd(f'screendump "{path}"', 0.5)
        for _ in range(30):
            if os.path.exists(path) and os.path.getsize(path) > 100:
                break
            time.sleep(0.1)
        data = open(path, "rb").read()
        parts, i = [], 0
        while len(parts) < 4:                       # P6 header: magic, width, height, maxval
            while data[i:i + 1].isspace():
                i += 1
            j = i
            while not data[j:j + 1].isspace():
                j += 1
            parts.append(data[i:j]); i = j
        w, h = int(parts[1]), int(parts[2])
        px = data[i + 1:]
        cyan = sum(1 for k in range(0, len(px) - 2, 3 * 7)
                   if px[k + 2] > 150 and px[k + 1] > 120 and px[k] < 110)
        return w, h, cyan / (len(px) / (3 * 7))

    def alive(self):
        return self.proc.poll() is None

    def close(self):
        self.proc.kill()
        self.proc.wait()


def main():
    image = sys.argv[1] if len(sys.argv) > 1 else "kernel.bin"
    screens = sys.argv[sys.argv.index("--screens") + 1] if "--screens" in sys.argv else None
    failures, total = [], 0

    def check(name, cond, detail=""):
        nonlocal total
        total += 1
        print(("  PASS  " if cond else "  FAIL  ") + name)
        if not cond:
            failures.append(name)
            if detail:
                print("\n".join("        | " + l for l in detail.strip().splitlines()[-15:]))

    print(f"ENCOM OS-12 smoke test: {image}")
    with tempfile.TemporaryDirectory() as tmp:
        vm = VM(image, tmp)
        shot = os.path.join(screens or tmp, "desktop.ppm")
        try:
            check("boots and finds a framebuffer", vm.wait_for("framebuffer:"), vm.serial())
            check("desktop comes up after the boot sequence", vm.wait_for("DESKTOP READY", 20), vm.serial())
            time.sleep(0.5)
            w, h, cyan = vm.screen(shot)
            check(f"screen is 1024x768 and drawn (cyan share {cyan:.2%})", (w, h) == (1024, 768) and cyan > 0.003)

            vm.type("help\n")
            check("help lists commands", "open      open an app" in vm.serial(), vm.serial())
            vm.type("echo Hi There!\n")
            check("keyboard incl. Shift reaches the shell", "  Hi There!" in vm.serial(), vm.serial())
            vm.type("sysinfo\n")
            s = vm.serial()
            check("sysinfo reports memory and display", "MiB" in s and "1024x768x32" in s, s)
            vm.type("foo\n")
            check("unknown command handled", "unknown command: foo" in vm.serial(), vm.serial())
            vm.type("open lightcycle\n")
            check("apps open from the shell", "no such app" not in vm.serial(), vm.serial())
            vm.cmd("sendkey f1", 0.3)

            vm.type("derez\n")
            check("CPU exception -> DEREZZED panic", vm.wait_for("DEREZZED: Division by zero", 5), vm.serial())
            check("no triple fault / reboot after the exception", vm.alive())
            if screens:
                vm.screen(os.path.join(screens, "panic.ppm"))
        finally:
            vm.close()

    print(f"\n{'OK' if not failures else 'FAILED'}: {total - len(failures)}/{total} checks passed")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
