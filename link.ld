OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot : ALIGN(4K)
    {
        KEEP(*(.multiboot .multiboot.*))
    }

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        __ro_start = .;
        *(.rodata*)
        __ro_end = .;
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        _bss_start = .;
        *(COMMON)
        *(.bss*)
        _bss_end = .;
    }

    .stack (NOLOAD) : ALIGN(4K)
    {
        _stack_start = .;
        . = . + 0x4000;
        _stack_end = .;
    }

    .heap (NOLOAD) : ALIGN(4K)
    {
        _heap_start = .;
        . = . + 0x100000;
        _heap_end = .;
    }

    . = ALIGN(4K);

    _end = .;
    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}