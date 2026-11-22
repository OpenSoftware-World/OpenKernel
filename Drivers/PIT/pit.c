/* Intel 8253/8254 PIT driver: programs channel 0 and counts IRQ0 ticks. */

#include "pit.h"
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>

#define PIT_DIVISOR_MIN 1        /* divisor is what reaches the counter */
#define PIT_DIVISOR_MAX 65535    /* channel 0 reload value is 16 bits */
#define EFLAGS_IF       (1u << 9) /* interrupt enable flag in EFLAGS */

/* Ticks counted at the current rate: written by IRQ0, read by everyone. */
static volatile uint32_t pit_ticks = 0;

/*
 * Whole milliseconds earned so far, plus the fraction that is not one yet.
 * The remainder is kept in units of 1/PIT_BASE_FREQUENCY ms (the input
 * clock), which is exact: at the default divisor 1193 the rate really is
 * 1193182 / 1193 = 1000.15 Hz, and rounding that to 1000 Hz would let the
 * uptime drift by about 1.3 seconds per day.
 *
 * IRQ0 owns both values. Keeping the accumulation here instead of dividing
 * ticks by a frequency when reading also keeps every access 32-bit - a
 * 64-bit divide would need __udivdi3 from libgcc, which the plain
 * i686-elf-ld link in the Makefile does not provide.
 */
static volatile uint32_t pit_ms = 0;
static volatile uint32_t pit_ms_remainder = 0;

static uint32_t pit_divisor = PIT_BASE_FREQUENCY / PIT_DEFAULT_FREQUENCY;
static uint32_t pit_frequency = PIT_DEFAULT_FREQUENCY;
static bool_t pit_hooked = false;
static bool_t pit_enabled = false;

/*
 * IRQ0 handler; interrupt_dispatch() sends the EOI after it returns.
 * One tick lasts divisor / PIT_BASE_FREQUENCY seconds, so it adds
 * divisor * 1000 units to the remainder. The loop runs at most once at
 * 1 kHz, more only at low rates.
 */
static void pit_irq_handler(registers_t *regs) {
    (void)regs;

    pit_ticks++;
    pit_ms_remainder += pit_divisor * 1000u;
    while (pit_ms_remainder >= PIT_BASE_FREQUENCY) {
        pit_ms_remainder -= PIT_BASE_FREQUENCY;
        pit_ms++;
    }
}

/* Clamp a requested rate to the window the 16-bit divisor can express. */
static uint32_t pit_clamp_frequency(uint32_t hz) {
    if (hz < PIT_MIN_FREQUENCY) return PIT_MIN_FREQUENCY;
    if (hz > PIT_MAX_FREQUENCY) return PIT_MAX_FREQUENCY;
    return hz;
}

/* Reload channel 0 so it interrupts at hz (hz must be clamped first). */
static void pit_program(uint32_t hz) {
    uint32_t divisor = PIT_BASE_FREQUENCY / hz;

    if (divisor < PIT_DIVISOR_MIN) divisor = PIT_DIVISOR_MIN;
    if (divisor > PIT_DIVISOR_MAX) divisor = PIT_DIVISOR_MAX;

    outb(PIT_COMMAND_PORT, PIT_COMMAND_WORD);
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));         /* low byte first */
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));  /* then high byte */

    pit_divisor = divisor;

    /* Nearest whole hertz of PIT_BASE_FREQUENCY / divisor - the exact
       rate is not an integer (1000.15 Hz at the default divisor). */
    pit_frequency = (PIT_BASE_FREQUENCY + divisor / 2) / divisor;
}

/*
 * Hook IRQ0, program channel 0 at hz and unmask the line.
 * interrupt_init() must already have run; interrupts stay whatever the
 * caller left them - ticks only advance while IF is set.
 */
void pit_init(uint32_t hz) {
    /* Hold IRQ0 while the rate and the accumulator are re-armed, so no
       tick is counted against a frequency the hardware does not have. */
    pic_set_mask(PIT_IRQ_LINE);

    hz = pit_clamp_frequency(hz);

    if (pit_hooked) {
        /* pit_ms already holds real milliseconds; the sub-millisecond
           remainder of the old rate is dropped rather than converted. */
        pit_ms_remainder = 0;
        pit_ticks = 0;
    } else {
        interrupt_register_handler((uint8_t)(PIC_IRQ_BASE + PIT_IRQ_LINE), pit_irq_handler);
        pit_hooked = true;
    }

    pit_program(hz);
    pit_enabled = true;
    pic_clear_mask(PIT_IRQ_LINE);
}

/* Mask IRQ0 again: the counter freezes at its current value. */
void pit_stop(void) {
    if (!pit_hooked) return;

    pic_set_mask(PIT_IRQ_LINE);
    pit_enabled = false;
}

uint32_t pit_get_ticks(void) {
    return pit_ticks;
}

uint32_t pit_get_frequency(void) {
    return pit_frequency;
}

uint32_t pit_get_uptime_ms(void) {
    return pit_ms;
}

/*
 * Busy-wait for ms milliseconds. It cannot run inside an IRQ handler:
 * the ticks it waits for would never be delivered while that handler runs.
 * With IF clear no tick can arrive either, so there is nothing to wait for
 * and the call returns immediately (documented behaviour). The wait is
 * driven by pit_ms, so no ms-to-tick conversion (and no 64-bit maths) is
 * needed; the resolution is one tick, 1 ms at the default rate.
 */
void pit_sleep_ms(uint32_t ms) {
    uint32_t start;

    if (!pit_enabled || ms == 0) return;
    if (!(eflags_r() & EFLAGS_IF)) return;

    start = pit_ms;

    /* Unsigned subtraction keeps the comparison valid across a wrap. hlt
       sleeps until the next interrupt; IF is set here, so it always wakes. */
    while ((uint32_t)(pit_ms - start) < ms) {
        hlt();
    }
}
