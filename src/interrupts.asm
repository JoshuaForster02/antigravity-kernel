; interrupts.asm — ASM wrappers for IRQ handlers
[bits 32]

; ── Keyboard (IRQ1 → INT 0x21) ───────────────────────────────────────────────
global keyboard_handler_wrapper
extern keyboard_handler

keyboard_handler_wrapper:
    pushad
    cld
    call keyboard_handler
    popad
    iretd

; ── Catch-all no-op handler (used to fill unused IDT slots) ──────────────────
global ignore_handler

ignore_handler:
    iretd
