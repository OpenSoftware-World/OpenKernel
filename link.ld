ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot : ALIGN(4K)
    {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    __ro_start = .;
    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }
    __ro_end = .;

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

    .stack : ALIGN(16)
    {
        _stack_start = .;
        . = . + 0x4000;
        _stack_end = .;
    }

    .heap : ALIGN(16)
    {
    _heap_start = .;
    . = . + 0x100000;
    _heap_end = .;
    }

    _end = .;
    _kernel_end = .;
}