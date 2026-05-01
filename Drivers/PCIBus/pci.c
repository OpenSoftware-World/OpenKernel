#include "pci.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>

#define MAX_PCI_DEV 64

static pci_device_t pci_devs[MAX_PCI_DEV];
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
            pci_device_t dev;
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
            pci_add_device(dev);
        }
    }
}

void pci_init() {
    pci_device_count = 0;
    pci_scan();
}

int pci_get_dev_count() {
    return pci_device_count;
}

pci_device_t* pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count) {
        return 0;
    } 
    return &pci_devs[index];
}

void pci_dev_init() {
    for (int i = 0; i < pci_get_dev_count(); i++) {
        pci_dev_t *dev = pci_get_device(i);

        if (dev->class_mod == 0x02) {
            if (dev->vendor_id == 0x8086) {
                e1000_init();
            }
            if (dev->vendor_id == 0x10EC) {
                rtl8139_init();
            }
        }
    }
}