; isr.asm - entry stubs for the CPU exceptions (0-31) and the hardware
; IRQs (32-47, after the PIC remap done by pic_init()). Every stub
; normalises the stack frame so interrupt_dispatch() always sees the
; same registers_t layout, then hands over to the shared isr_common path.
;
; Assemble: nasm -f elf32 isr.asm

; C entry points referenced from this file.
extern interrupt_dispatch

; Vector of IRQ0 after the PIC remap.
IRQ_BASE_VECTOR equ 32

; Shared tail: save the registers, switch to the kernel data segments and
; call the C dispatcher with a pointer to the register frame.
isr_common:
    pusha                       ; eax..edi (pusha order)
    mov ax, ds
    push eax                    ; keep the entry data segment in the frame

    mov ax, 0x10                ; kernel data segment selector (GDT index 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                    ; registers_t *regs
    call interrupt_dispatch
    add esp, 4

    ; Drop the saved entry segment and reload the kernel segments
    ; instead: a fault like #GP may have left ds/es/fs/gs with a
    ; selector the kernel cannot use.
    pop eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                        ; eax..edi
    add esp, 8                  ; drop the vector number and the error code
    iret

; Exception stub: the CPU pushes an error code for 8, 10-14 and 17 only,
; so every other stub pushes a dummy one to keep the frame uniform.
%macro EXCEPTION_STUB 1
global isr%1
isr%1:
%if ((%1 = 8) || (%1 = 10) || (%1 = 11) || (%1 = 12) || (%1 = 13) || (%1 = 14) || (%1 = 17))
    push dword %1               ; the error code is already on the stack
%else
    push dword 0
    push dword %1
%endif
    jmp isr_common
%endmacro

; IRQ stubs never receive an error code; the pushed vector number is the
; remapped one (32-47), which is what the dispatcher expects.
%macro IRQ_STUB 1
global irq%1
irq%1:
    push dword 0
    push dword (IRQ_BASE_VECTOR + %1)
    jmp isr_common
%endmacro

%assign vec 0
%rep 32
    EXCEPTION_STUB vec
    %assign vec vec + 1
%endrep

%assign vec 0
%rep 16
    IRQ_STUB vec
    %assign vec vec + 1
%endrep
