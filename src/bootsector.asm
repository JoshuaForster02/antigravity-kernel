; boot.asm - Minimal 16-bit real mode bootsector
[org 0x7c00]

mov ah, 0x0e ; BIOS teletype output

mov al, 'A'
int 0x10
mov al, 'n'
int 0x10
mov al, 't'
int 0x10
mov al, 'i'
int 0x10
mov al, 'g'
int 0x10
mov al, 'r'
int 0x10
mov al, 'a'
int 0x10
mov al, 'v'
int 0x10
mov al, 'i'
int 0x10
mov al, 't'
int 0x10
mov al, 'y'
int 0x10
mov al, ' '
int 0x10
mov al, 'O'
int 0x10
mov al, 'S'
int 0x10
mov al, '!'
int 0x10

jmp $ ; Infinite loop

times 510-($-$$) db 0 ; Pad to 510 bytes
dw 0xaa55 ; Boot signature
