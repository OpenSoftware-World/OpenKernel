#include "ata.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/SystemLib/Memory/mem.h>

/* Polling budget for the status register. Without it a dead or absent drive
   would spin forever inside the wait helpers below. */
#define ATA_POLL_LIMIT 1000000u

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

/* Wait for BSY to clear. Returns false on timeout and on ERR/DF, so a failing
   drive reports an error instead of hanging the kernel. */
static bool_t ata_wait_busy(void) {
    for (uint32_t i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = inb(ATA_STATUS);

        if (status & ATA_STATUS_BSY) {
            continue;
        }
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return false;
        }
        return true;
    }
    return false;
}

/* Wait for DRQ after a command. BSY must be clear before DRQ is meaningful,
   and ERR/DF abort the transfer. */
static bool_t ata_wait_drq(void) {
    for (uint32_t i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = inb(ATA_STATUS);

        if (status & ATA_STATUS_BSY) {
            continue;
        }
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return false;
        }
        if (status & ATA_STATUS_DRQ) {
            return true;
        }
    }
    return false;
}

void ata_sys_disk_init() {

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

    outb(ATA_COMMAND, ATA_IDENTIFY);

    if (!ata_wait_busy() || !ata_wait_drq()) {
        memset(buf, 0, ATA_SECTOR_SIZE);
        return;
    }

    for (int i = 0; i < 256; i++) {
        buf[i] = inw(ATA_DATA);
    }
}
