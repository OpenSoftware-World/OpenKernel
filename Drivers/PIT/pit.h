#ifndef PIT_H
#define PIT_H

#include <OpenKernel/SystemLib/Std/types.h>

/*
Intel 8253/8254 Programmable Interval Timer (PIT).

Channel 0 is programmed as a periodic source and wired to IRQ0, which the
PIC remaps to vector 32. Every interrupt advances one tick counter, so the
kernel gets a millisecond clock (1 kHz) plus a tick-based sleep.

pit_init() only programs the hardware: ticks advance while the interrupt
flag (IF) is set, so the caller has to enable interrupts itself.
*/

/* One data port per channel plus the shared command/status port. */
#define PIT_CHANNEL0_PORT 0x40
#define PIT_CHANNEL1_PORT 0x41
#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT  0x43

/* Input clock fed to channel 0 by the motherboard crystal (1.193182 MHz). */
#define PIT_BASE_FREQUENCY 1193182

/* Frequency the driver starts at: 1 kHz means one tick per millisecond. */
#define PIT_DEFAULT_FREQUENCY 1000

/* The 16-bit divisor cannot express rates outside this window. */
#define PIT_MIN_FREQUENCY 19
#define PIT_MAX_FREQUENCY PIT_BASE_FREQUENCY

/* Command word: channel 0, low/high byte access, rate generator, binary. */
#define PIT_COMMAND_WORD 0x34

/* IRQ line used by channel 0 (remapped to vector PIC_IRQ_BASE + 0). */
#define PIT_IRQ_LINE 0

/* Program channel 0 at hz (clamped to 19..1193182) and start counting. */
void pit_init(uint32_t hz);

/* Mask IRQ0: the tick counter freezes, the IRQ handler stays registered. */
void pit_stop(void);

/* Interrupts handled since the last pit_init(). */
uint32_t pit_get_ticks(void);

/* Nearest whole hertz the hardware is running at (1193182 / divisor). */
uint32_t pit_get_frequency(void);

/* Milliseconds the timer has been running; wraps after about 49.7 days. */
uint32_t pit_get_uptime_ms(void);

/* Busy-wait for ms milliseconds. Not usable from inside an IRQ handler. */
void pit_sleep_ms(uint32_t ms);

#define PIT_DRIVER_NAME "OpenKernel PIT (Programmable Interval Timer) Driver"
#define PIT_DRIVER_VER "1.2"
#define PIT_DRIVER_DESC "An Intel 8253/8254 interval timer driver for OpenKernel"
#define PIT_DRIVER_AUTHOR "OpenSoftware-World"
#define PIT_DRIVER_KRNL_VER "4.0"

#endif
