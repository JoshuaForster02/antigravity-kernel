# Makefile — Antigravity OS
# Build inside Docker: docker run --rm -v $(pwd):/os antigravity-dev make

AS  = nasm
CC  = i686-elf-gcc
LD  = i686-elf-ld

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

.PHONY: all clean

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

clean:
	rm -rf $(OBJDIR) $(KERNEL_BIN)
