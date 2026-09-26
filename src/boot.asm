; boot.asm - Minimal Multiboot Header
MB_MAGIC     equ 0x1BADB002          ; Multiboot magic number
MB_FLAGS     equ 1 << 0 | 1 << 1     ; Align modules, provide memory map
MB_CHECKSUM  equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
    align 16
    stack_bottom:
        resb 16384 ; 16 KiB stack
    stack_top:

section .text
    global _start:function (_start.end - _start)
    _start:
        ; Set up stack
        mov esp, stack_top

        ; Call the C kernel
        extern kernel_main
        call kernel_main

        ; Infinite loop if kernel returns
        cli
    .hang:
        hlt
        jmp .hang
    .end:

section .note.GNU-stack noalloc noexec nowrite progbits
