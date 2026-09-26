# Makefile — Antigravity OS
# Build natively (needs nasm + i686-elf cross compiler OR gcc-multilib)
# or inside Docker: docker build -t antigravity-dev . && docker run --rm -v $(pwd):/os antigravity-dev make
#
#   make        build kernel.bin
#   make run    boot in QEMU (window)
#   make test   headless boot smoke test in QEMU (used by CI)

AS  = nasm

# Prefer a real i686-elf cross compiler; fall back to the host gcc in 32-bit mode.
ifneq ($(shell command -v i686-elf-gcc 2>/dev/null),)
CC  = i686-elf-gcc
LD  = i686-elf-ld
else
CC  = gcc -m32 -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables
LD  = ld -m elf_i386
endif

QEMU = qemu-system-i386

ASFLAGS = -f elf32
CCFLAGS = -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -T src/linker.ld

SRCDIR = src
OBJDIR = obj

# C sources: all .c files in src/
C_SOURCES   = $(wildcard $(SRCDIR)/*.c)
# ASM sources: all .asm in src/ EXCEPT the standalone bootsector
ASM_SOURCES = $(filter-out $(SRCDIR)/bootsector.asm, $(wildcard $(SRCDIR)/*.asm))

OBJECTS = $(C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o) \
          $(ASM_SOURCES:$(SRCDIR)/%.asm=$(OBJDIR)/%.o)

KERNEL_BIN = kernel.bin

.PHONY: all clean run test

all: $(KERNEL_BIN)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CCFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.asm
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL_BIN): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Standalone legacy bootsector (not part of kernel.bin)
boot.bin: $(SRCDIR)/bootsector.asm
	$(AS) -f bin $< -o $@

# Multiboot kernel: QEMU loads it directly, no GRUB image needed
run: $(KERNEL_BIN)
	$(QEMU) -kernel $(KERNEL_BIN)

test: $(KERNEL_BIN)
	python3 tools/smoketest.py $(KERNEL_BIN)

clean:
	rm -rf $(OBJDIR) $(KERNEL_BIN)
