#include "ata.h"
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/SystemLib/Memory/mem.h>

/* Polling budget for the status register. Without it a dead or absent drive
   would spin forever inside the wait helpers below. */
#define ATA_POLL_LIMIT 1000000u

/*
 * HLT budget used only while waiting on IRQ14: counted in wake-ups, so at
 * the default 1 kHz PIT rate it is roughly 10 seconds. A dead drive then
 * still times out instead of sleeping forever - but only while some other
 * interrupt (normally IRQ0) keeps arriving; with IRQ14 hooked the kernel
 * should be running pit_init() too.
 */
#define ATA_IRQ_HLT_LIMIT 10000u

/* Drive select word for LBA mode, master drive. It has to be 0xE0, not 0x0E:
   bits 7-5 carry the fixed pattern the drive expects (and bit 4 selects the
   drive) before it will accept an LBA command. The low nibble is overwritten
   with bits 24-27 of the LBA. */
#define ATA_DRIVE_LBA 0xE0

/* The spec wants about 400 ns between a drive select write and the next status
   read; four status reads are the usual portable way to burn that time. */
static void ata_delay(void) {
    for (int i = 0; i < 4; i++) {
        (void)inb(ATA_STATUS);
    }
}

/* Set by IRQ14 when the drive reports completion; cleared before every
   command so a stale flag cannot satisfy the next wait. */
static volatile bool_t ata_irq_done = false;

/* True once ata_sys_disk_init() has hooked IRQ14: only then do the wait
   helpers sleep instead of polling. */
static bool_t ata_irq_hooked = false;

static void ata_irq_handler(registers_t *regs) {
    (void)regs;
    ata_irq_done = true;
}

/*
 * Sleep until IRQ14 (or any other interrupt) wakes the CPU, when the IRQ
 * is hooked and interrupts are on - otherwise return at once so the caller
 * keeps polling. The flag is tested with interrupts off, and STI keeps
 * them masked until after the following HLT, so an IRQ that lands just
 * before the halt cannot be lost.
 */
static void ata_sleep_until_irq(void) {
    if (!ata_irq_hooked || !(eflags_r() & EFLAGS_IF)) return;

    cli();
    if (ata_irq_done) {
        ata_irq_done = false;
        sti();
        return;
    }
    sti();      /* shadowed: no IRQ can slip between STI and HLT */
    hlt();
    ata_irq_done = false;
}

/*
 * Shared wait core. need_drq == false waits for BSY to clear (command
 * accepted), need_drq == true additionally waits for the data phase.
 * ERR/DF abort either wait, and both budgets are bounded, so a failing
 * drive reports an error instead of hanging the kernel.
 */
static bool_t ata_wait_ready(bool_t need_drq) {
    uint32_t sleeps = ATA_IRQ_HLT_LIMIT;

    for (uint32_t i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = inb(ATA_STATUS);

        if (status & ATA_STATUS_BSY) goto wait;
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) return false;
        if (!need_drq || (status & ATA_STATUS_DRQ)) return true;

    wait:
        /* IRQ14 hooked and interrupts on: sleep between status reads.
           Without them this is the original bounded busy-wait. */
        if (!ata_irq_hooked || !(eflags_r() & EFLAGS_IF)) continue;
        if (sleeps-- == 0) return false;
        ata_sleep_until_irq();
    }
    return false;
}

/* Wait for BSY to clear; returns false on timeout and on ERR/DF. */
static bool_t ata_wait_busy(void) {
    return ata_wait_ready(false);
}

/* Wait for DRQ after a command; BSY must be clear before DRQ is
   meaningful, and ERR/DF abort the transfer. */
static bool_t ata_wait_drq(void) {
    return ata_wait_ready(true);
}

/*
 * Hook IRQ14 and let the drive interrupt: nIEN (bit 1 of the device
 * control register) is cleared so the primary channel asserts the line on
 * command completion. From here the wait helpers sleep on the IRQ instead
 * of polling; interrupt_init() must have run first.
 */
void ata_sys_disk_init() {
    outb(ATA_CONTROL, 0x00);        /* nIEN = 0: device interrupts enabled */

    interrupt_register_handler((uint8_t)(PIC_IRQ_BASE + ATA_IRQ_LINE), ata_irq_handler);
    ata_irq_done = false;
    ata_irq_hooked = true;

    pic_clear_mask(ATA_IRQ_LINE);
}

void ata_sys_disk_read(uint32_t lba, uint8_t* buffer) {
    bool_t ready;

    if (!buffer) return;

    ata_delay();
    ready = ata_wait_busy();

    if (ready) {
        outb(ATA_DRIVE_SELECT, ATA_DRIVE_LBA | ((lba >> 24) & 0x0F));
        outb(ATA_SECTOR_COUNT, 1);

        outb(ATA_LBA_LOW, (uint8_t)(lba));
        outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
        outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

        ata_irq_done = false;   /* only IRQs of this command may satisfy it */
        outb(ATA_COMMAND, ATA_READ);

        ready = ata_wait_busy() && ata_wait_drq();
    }

    if (!ready) {
        /* Leave a defined sector behind rather than whatever was on the
           stack, so the caller never parses uninitialised bytes. */
        memset(buffer, 0, ATA_SECTOR_SIZE);
        return;
    }

    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buffer)[i] = inw(ATA_DATA);
    }
}

void ata_sys_disk_write(uint32_t lba, const uint8_t* buffer) {
    bool_t ready;

    if (!buffer) return;

    ata_delay();
    ready = ata_wait_busy();

    if (ready) {
        outb(ATA_DRIVE_SELECT, ATA_DRIVE_LBA | ((lba >> 24) & 0x0F));
        outb(ATA_SECTOR_COUNT, 1);

        outb(ATA_LBA_LOW, (uint8_t)(lba));
        outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
        outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

        ata_irq_done = false;   /* only IRQs of this command may satisfy it */
        outb(ATA_COMMAND, ATA_WRITE);

        ready = ata_wait_busy() && ata_wait_drq();
    }

    if (!ready) {
        /* The command never reached the data phase, so there is nothing to
           drain. The sector is left unmodified. */
        return;
    }

    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, ((const uint16_t*)buffer)[i]);
    }
}

void ata_soft_reset() {
    outb(ATA_CONTROL, ATA_SRST);

    for (int i = 0; i < 1000; i++) {
        asm volatile ("nop");
    }
    outb(ATA_CONTROL, 0x00);

    for (int i = 0; i < 10000; i++) {
        asm volatile("nop");
    }

    /* Bounded: an absent drive would otherwise hang here forever. */
    (void)ata_wait_busy();
}

void ata_reinit(uint8_t drive, uint16_t* identify_buf) {
    if (!ata_wait_busy()) return;

    outb(ATA_DRIVE_SELECT, drive);
    ata_delay();

    for (volatile int i = 0; i < 10000; i++) {
        asm volatile ("nop");
    }
    ata_identify(drive, identify_buf);
}

void ata_identify(uint8_t drive, uint16_t* buf) {
    if (!buf) return;

    if (!ata_wait_busy()) {
        memset(buf, 0, ATA_SECTOR_SIZE);
        return;
    }

    outb(ATA_DRIVE_SELECT, drive);
    ata_delay();
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);

    ata_irq_done = false;   /* only IRQs of this command may satisfy it */
    outb(ATA_COMMAND, ATA_IDENTIFY);

    if (!ata_wait_busy() || !ata_wait_drq()) {
        memset(buf, 0, ATA_SECTOR_SIZE);
        return;
    }

    for (int i = 0; i < 256; i++) {
        buf[i] = inw(ATA_DATA);
    }
}
