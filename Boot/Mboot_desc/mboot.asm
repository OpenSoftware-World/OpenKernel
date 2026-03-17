section .multiboot
align 4

multiboot_magic    equ 0x1BADB002
multiboot_flags    equ 0x00000003
multiboot_checksum equ -(multiboot_magic + multiboot_flags)

dd multiboot_magic
dd multiboot_flags
dd multiboot_checksum