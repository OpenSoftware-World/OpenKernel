#include "pci.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/Drivers/Network/e1000/e1000.h>
#include <OpenKernel/Drivers/Network/RTL8139/rtl8139.h>

#define MAX_PCI_DEV 64

static pci_dev_t pci_devs[MAX_PCI_DEV];
static int pci_dev_count = 0;

uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t addr = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, val);    
}

static void pci_add_device(pci_dev_t dev) {
    if (pci_dev_count >= MAX_PCI_DEV) {
        return;
    }
    pci_devs[pci_dev_count++] = dev;
}

void pci_scan() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            uint32_t data = pci_read(bus, device, 0, 0);
            uint16_t vendor = data & 0xFFFF;
            if (vendor == 0xFFFF) {
                continue;
            }
            pci_dev_t dev;
            dev.bus = bus;
            dev.dev = device;
            dev.func = 0;
            dev.vendor_id = vendor;
            dev.dev_id = data >> 16;
            uint32_t class_dat = pci_read(bus, device, 0, 8);
            dev.class_mod = (class_dat >> 24) & 0xFF;
            dev.subclass = (class_dat >> 16) & 0xFF;
            dev.prog_if = (class_dat >> 8) & 0xFF;

            for (int i = 0; i < 6; i++) {
                dev.bar[i] = pci_read(bus, device, 0, 0x10 + (i * 4));
            }
            uint32_t irq_dat = pci_read(bus, device, 0, 0x3C);
            dev.interrupt_line = (uint8_t)(irq_dat & 0xFF);
            dev.interrupt_pin = (uint8_t)((irq_dat >> 8) & 0xFF);

            pci_add_device(dev);
        }
    }
}

void pci_init() {
    pci_dev_count = 0;
    pci_scan();
}

int pci_get_dev_count() {
    return pci_dev_count;
}

pci_dev_t* pci_get_device(int index) {
    if (index < 0 || index >= pci_dev_count) {
        return 0;
    } 
    return &pci_devs[index];
}

/*
 * Turn on the address decoders a driver is about to use. A BAR is inert until
 * the matching enable bit in the command register is set, and the e1000 also
 * needs bus mastering to move its descriptor rings.
 *
 * Only the 16-bit command half is written back: the upper half of that dword
 * is the (write-1-to-clear) status register, which must not be touched.
 */
static void pci_enable_device(pci_dev_t *dev) {
    uint32_t cmd = pci_read(dev->bus, dev->dev, dev->func, 0x04) & 0xFFFFu;

    cmd |= 0x0001;  /* I/O space enable      - RTL8139 */
    cmd |= 0x0002;  /* memory space enable   - e1000   */
    cmd |= 0x0004;  /* bus master enable     - e1000   */

    pci_write(dev->bus, dev->dev, dev->func, 0x04, cmd);
}

void pci_dev_init() {
    for (int i = 0; i < pci_get_dev_count(); i++) {
        pci_dev_t *dev = pci_get_device(i);

        if (!dev || dev->class_mod != 0x02) {
            continue;
        }

        if (dev->vendor_id == 0x8086) {
            pci_enable_device(dev);
            e1000_init(dev);
        }
        else if (dev->vendor_id == 0x10EC) {
            pci_enable_device(dev);
            rtl8139_init(dev);
        }
    }
}