#ifndef RTL8139_H
#define RTL8139_H

#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/Drivers/PCIBus/pci.h>

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

#define RTL_REG_MAC0 0x00
#define RTL_REG_CMD 0x37
#define RTL_REG_RX_BUF 0x30
#define RTL_REG_IMR 0x3C
#define RTL_REG_ISR 0x3E

#define RTL_REG_TX_STATUS0 0x10
#define RTL_REG_TX_ADDR0 0x20

#define RTL_CMD_RESET 0x10
#define RTL_CMD_RX_ENABLE 0x08
#define RTL_CMD_TX_ENABLE 0x04
#define RTL_RX_BUF_SIZE 8192

/* IMR (0x3C) / ISR (0x3E) bits: the events this driver lets interrupt. */
#define RTL_INT_RXOK   0x0001   /* a good packet is in the receive ring */
#define RTL_INT_RXERR  0x0002   /* a receive error was reported */
#define RTL_INT_TXOK   0x0004   /* a transmit completed */
#define RTL_INT_RXOVW  0x0010   /* the receive ring overflowed */

/* Upper bound on ring-drain rounds inside the IRQ handler: a stuck
   buffer-empty flag must not trap the handler in interrupt context. */
#define RTL_RX_DRAIN_LIMIT 256

typedef struct {
    uint32_t io_base;
    uint8_t mac[6];
    uint8_t *rx_buffer;
    uint32_t rx_offset;
} rtl8139_t;

bool_t rtl8139_init(pci_dev_t *pdev);
void rtl8139_send_packet(void *data, uint16_t len);
void rtl8139_handle_rx(void);

#define RTL8139_DRIVER_NAME "OpenKernel Realtek 8139 Driver"
#define RTL8139_DRIVER_VER "1.0"
#define RTL8139_DRIVER_DESC "A Realtek 8139 driver for OpenKernel"
#define RTL8139_DRIVER_AUTHOR "OpenSoftware-World"
#define RTL8139_DRIVER_KRNL_VER "4.0"

#endif