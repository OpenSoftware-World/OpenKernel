#ifndef RTL8139_H
#define RTL8139_H

#include <OpenKernel/SystemLib/Std/types.h>

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

typedef struct {
    uint32_t io_base;
    uint8_t mac[6];
    uint8_t *rx_buffer;
    uint32_t rx_offset;
} rtl8139_t;

bool_t rtl8139_init();
void rtl8139_send_packet(void *data, uint16_t len);

#define RTL8139_DRIVER_NAME "OpenKernel Realtek 8139 Driver"
#define RTL8139_DRIVER_VER "0.1"
#define RTL8139_DRIVER_DESC "A Realtek 8139 driver for OpenKernel"
#define RTL8139_DRIVER_AUTHOR "OpenSoftware-World"
#define RTL8139_DRIVER_KRNL_VER "2.0"

#endif