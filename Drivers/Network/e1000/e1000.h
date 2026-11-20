#ifndef E1000_H
#define E1000_H

#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/Drivers/PCIBus/pci.h>

#define E1000_VENDOR_ID 0x8086

#define E1000_DEV_82540EM 0x100E
#define E1000_DEV_82545EM 0x100F

#define E1000_TDBAL 0x3800
#define E1000_TDBAH 0x3804
#define E1000_TDLEN 0x3808
#define E1000_TDH 0x3810
#define E1000_TDT 0x3818

#define E1000_RDBAL 0x2800
#define E1000_RDBAH 0x2804
#define E1000_RDLEN 0x2808
#define E1000_RDH   0x2810
#define E1000_RDT   0x2818

#define E1000_TCTL 0x0400
#define E1000_RCTL 0x0100

#define E1000_STATUS 0x0008

/* Interrupt Cause Read (read clears the reported bits), Mask Set and
   Mask Clear: the e1000's interrupt interface. */
#define E1000_ICR 0xC0
#define E1000_IMS 0xD0
#define E1000_IMC 0xD8

/* ICR/IMS cause bits (82540EM). */
#define E1000_ICR_TXDW   (1u << 0)    /* transmit descriptor written */
#define E1000_ICR_TXQE   (1u << 1)    /* transmit queue empty */
#define E1000_ICR_LSC    (1u << 2)    /* link status change */
#define E1000_ICR_RXSEQ  (1u << 3)    /* receive sequence error */
#define E1000_ICR_RXDMT0 (1u << 4)    /* receive descriptor min threshold */
#define E1000_ICR_RXO    (1u << 6)    /* receive FIFO overrun */
#define E1000_ICR_RXT0   (1u << 7)    /* receive timer */
#define E1000_ICR_RXCFG  (1u << 10)   /* receive configuration changed */

/* The causes this driver reacts to: everything that means "descriptors
   were filled or the receive path needs attention". */
#define E1000_RX_INTERRUPTS (E1000_ICR_RXSEQ | E1000_ICR_RXDMT0 | \
                             E1000_ICR_RXO   | E1000_ICR_RXT0   | \
                             E1000_ICR_RXCFG)

#define E1000_TX_DESC_COUNT 32
#define E1000_RX_DESC_COUNT 32

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint8_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint32_t *mmio_base;
    e1000_tx_desc_t *tx_descs;
    e1000_rx_desc_t *rx_desc;
    uint8_t tx_index;
    uint8_t rx_index;
    bool_t link_up;
} e1000_t;

bool_t e1000_init(pci_dev_t *pdev);
void e1000_send_packet(void *data, uint16_t len);
void e1000_handle_rx(void);

#define E1000_DRIVER_NAME "OpenKernel Intel E1000 Driver"
#define E1000_DRIVER_VER "1.0"
#define E1000_DRIVER_DESC "A Intel E1000 for OpenKernel"
#define E1000_DRIVER_AUTHOR "OpenSoftware-World"
#define E1000_DRIVER_KRNL_VER "4.0"

#endif