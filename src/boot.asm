; boot.asm - Multiboot header (with video mode request) + entry
MB_MAGIC     equ 0x1BADB002
MB_FLAGS     equ 1 << 0 | 1 << 1 | 1 << 2   ; align modules, memory map, video mode
MB_CHECKSUM  equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
    align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
    dd 0, 0, 0, 0, 0        ; address fields (unused: ELF kernel, flag 16 not set)
    dd 0                    ; mode_type: 0 = linear framebuffer
    dd 1024, 768, 32        ; preferred width, height, depth

section .bss
    align 16
    stack_bottom:
        resb 32768 ; 32 KiB stack
    stack_top:

section .text
    global _start:function (_start.end - _start)
    _start:
        mov esp, stack_top
        push ebx                ; multiboot_info*
        push eax                ; multiboot magic
        extern kernel_main
        call kernel_main
        cli
    .hang:
        hlt
        jmp .hang
    .end:

section .note.GNU-stack noalloc noexec nowrite progbits
