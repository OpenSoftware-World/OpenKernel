#include "rtl8139.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>

static rtl8139_t *nic;

static uint8_t rx_buffer[RTL_RX_BUFFER_SIZE];
static uint32_t rx_pos = 0;

bool_t rtl8139_init()
{
    nic = (rtl8139_t*)0xDEADBEEF;
    outb(nic->io_base + RTL_REG_COMMAND, RTL_CMD_RESET);
    outb(nic->io_base + RTL_REG_COMMAND, RTL_CMD_RX_ENABLE | RTL_CMD_TX_ENABLE);
    outl(nic->io_base + RTL_REG_RX_BUF, (uint32_t)&rx_buffer);
    return true;
}

void rtl8139_send_packet(void *data, uint16_t len)
{
    static int tx_index = 0;
    uint16_t port = nic->io_base + RTL_REG_TX_ADDR0 + (tx_index * 4);
    outl(port, (uint32_t)data);
    port = nic->io_base + RTL_REG_TX_STATUS0 + (tx_index * 4);
    outl(port, len);
    tx_index = (tx_index + 1) % 4;
}

void rtl8139_receive_poll()
{
    uint8_t cmd = inb(nic->io_base + RTL_REG_COMMAND);
    if (cmd & 0x01)
    {
        uint8_t *packet = &rx_buffer[rx_pos];
        uint16_t len = *(uint16_t*)(packet + 2);
        rx_pos += len + 4;
        if (rx_pos >= RTL_RX_BUFFER_SIZE)
            rx_pos = 0;
    }
}