#include "keyboard.h"
#include <OpenKernel/Drivers/Mouse/mouse.h>
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/SystemLib/TimeMng/time.h>

#define KBD_DATA 0x60
#define KBD_STAT 0x64

/* Safety bound for the drain loop below: a stuck status bit must not trap
   the handler in interrupt context. The 8042 output buffer is only a byte
   deep, so one pass always empties it in practice. */
#define KBD_DRAIN_LIMIT 16

static bool_t caps_lock = false;
static bool_t kbd_enabled_driver = false;

static const char kmap[128] __attribute__((section(".rodata"))) = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0
};

/*
 * Written by IRQ1 and read by get_char()/kb_check() in task context, so
 * the indices and the payload are volatile: get_char() sleeps in hlt()
 * and must see the byte the handler stored before it wakes.
 */
static volatile char kbf[KEY_BF_SIZE];
static volatile uint8_t buf_head = 0;
static volatile uint8_t buf_tail = 0;

static char to_upper(char c) {
    if (!kbd_enabled_driver) return c;
    if (caps_lock && c >= 'a' && c <= 'z') {
        return c - 32;
    }
    return c;
}

/* Store a translated character in the ring buffer; drop it when full. */
static void kbd_push(char c) {
    uint8_t next = (uint8_t)((buf_head + 1) % KEY_BF_SIZE);
    if (next == buf_tail) return;
    kbf[buf_head] = c;
    buf_head = next;
}

/*
 * IRQ1 handler. The 8042 output buffer is shared with the mouse, and the
 * auxiliary flag in the status register says which device owns the byte at
 * its head - either IRQ line can therefore surface the other device's data,
 * so both bytes are routed instead of dropped. interrupt_dispatch() sends
 * the EOI after this returns.
 */
static void kbd_irq_handler(registers_t *regs) {
    (void)regs;

    for (int i = 0; i < KBD_DRAIN_LIMIT; i++) {
        uint8_t stat = inb(KBD_STAT);
        uint8_t data;

        if (!(stat & 1)) break;      /* output buffer empty: nothing to do */
        data = inb(KBD_DATA);

        if (stat & 0x20) mouse_handle_byte(data);   /* auxiliary: mouse byte */
        else kbd_hndlr(data);
    }
}

void kbd_init() {
    buf_head = 0;
    buf_tail = 0;
    caps_lock = false;

    /* IRQ1 fills the ring buffer from now on; interrupt_init() must have
       run first and a repeated registration is a no-op. */
    interrupt_register_handler((uint8_t)(PIC_IRQ_BASE + KBD_IRQ_LINE), kbd_irq_handler);
    pic_clear_mask(KBD_IRQ_LINE);

    kbd_enabled_driver = true;
}

uint8_t kb_check() {
    if (!kbd_enabled_driver) return 0;
    return buf_head != buf_tail;
}

char get_char() {
    if (!kbd_enabled_driver) return 0;

    /*
     * IRQ1 owns the controller: sleep until it delivers a byte instead of
     * burning CPU on port 0x64. With interrupts off no IRQ can arrive, so
     * the wait falls back to spinning (same rule as pit_sleep_ms()).
     */
    while (buf_head == buf_tail) {
        if (eflags_r() & EFLAGS_IF) hlt();
    }

    char c = kbf[buf_tail];
    buf_tail = (uint8_t)((buf_tail + 1) % KEY_BF_SIZE);
    return c;
}

void kbd_hndlr(uint8_t scancode) {
    if (!kbd_enabled_driver) return;
    if (scancode & 0x80) return; /* break code: nothing to translate */
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }
    if (scancode >= 128) return;

    char c = kmap[scancode];
    if (c == 0) return;

    kbd_push(to_upper(c));
}

void kbd_close() {
    pic_set_mask(KBD_IRQ_LINE);
    buf_head = 0;
    buf_tail = 0;
    sleep(1);
    caps_lock = false;
    kbd_enabled_driver = false;
}