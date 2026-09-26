; interrupts.asm — ASM wrappers for GDT load, CPU exceptions and IRQ handlers
[bits 32]

; ── GDT: load table, far-jump to reload CS, reload data segments ────────────
global gdt_flush

gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.reload_cs
.reload_cs:
    ret

; ── CPU exceptions 0-31 ──────────────────────────────────────────────────────
; Some exceptions push an error code, the rest don't. Push a dummy 0 for those
; so every frame has the same layout (struct regs_t in panic.h).
extern exception_handler

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

isr_common:
    pushad
    cld
    push esp                ; struct regs_t*
    call exception_handler  ; never returns
.hang:
    cli
    hlt
    jmp .hang

; Address table so idt.c can install all 32 stubs in a loop
section .rodata
global isr_table
isr_table:
%assign i 0
%rep 32
    dd isr %+ i
%assign i i+1
%endrep
section .text

; ── Keyboard (IRQ1 → INT 0x21) ───────────────────────────────────────────────
global keyboard_handler_wrapper
extern keyboard_handler

keyboard_handler_wrapper:
    pushad
    cld
    call keyboard_handler
    popad
    iretd

; ── PIT timer (IRQ0 → INT 0x20) and PS/2 mouse (IRQ12 → INT 0x2C) ─────────────
global timer_handler_wrapper
global mouse_handler_wrapper
extern timer_handler
extern mouse_handler

timer_handler_wrapper:
    pushad
    cld
    call timer_handler
    popad
    iretd

mouse_handler_wrapper:
    pushad
    cld
    call mouse_handler
    popad
    iretd

; ── Catch-all no-op handler (unused IRQ/software vectors 32-255) ─────────────
global ignore_handler

ignore_handler:
    iretd

section .note.GNU-stack noalloc noexec nowrite progbits
