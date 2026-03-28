bits 32

%include "Boot/Mboot_desc/mboot.asm"

global _start
extern kernel_main

section .text
_start:

    cmp eax, 0x2BADB002
    jne hang
    cli
    mov esp, 0x9FC00

    call kernel_main

hang:
    hlt
    jmp hang