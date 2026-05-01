#ifndef PCI_H
#define PCI_H

#include <OpenKernel/SystemLib/Std/types.h>

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

typedef struct {
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t dev_id;
    uint8_t class_mod;
    uint8_t subclass;
    uint8_t prog_if;
    uint32_t bar[6];
} pci_dev_t;

void pci_init();
uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);
void pci_scan();

int pci_get_dev_count();
pci_dev_t* pci_get_device(int index);

#define PCIBUS_DRIVER_NAME "OpenKernel PCI Bus Driver"
#define PCIBUS_DRIVER_VER "0.1"
#define PCIBUS_DRIVER_DESC "A PCI Bus driver for OpenKernel"
#define PCIBUS_DRIVER_AUTHOR "OpenSoftware-World"
#define PCIBUS_DRIVER_KRNL_VER "2.0"

#endif