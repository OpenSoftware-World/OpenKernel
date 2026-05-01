#include "e1000.h"
#include <OpenKernel/SystemLib/Mem/mem.h>

#define REG(offset) (*(volatile uint32_t*)((uint8_t*)dev->mmio_base + (offset)))

static e1000_t *dev;

static e1000_tx_desc_t *tx_ring;
static e1000_rx_desc_t *rx_ring;

static uint8_t tx_index = 0;
static uint8_t rx_index = 0;

bool e1000_init()
{
    dev = (e1000_t*)kmalloc(sizeof(e1000_t));

    dev->mmio_base = (uint32_t*)0xF0000000;

    tx_ring = (e1000_tx_desc_t*)kmalloc(sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT);
    rx_ring = (e1000_rx_desc_t*)kmalloc(sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT);

    memset(tx_ring, 0, sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT);
    memset(rx_ring, 0, sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT);

    for (int i = 0; i < E1000_RX_DESC_COUNT; i++)
    {
        rx_ring[i].addr = (uint64_t)kmalloc(2048);
        rx_ring[i].status = 0;
    }

    REG(E1000_TDBAL) = (uint32_t)(uint64_t)tx_ring;
    REG(E1000_TDLEN) = sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT;
    REG(E1000_TDH) = 0;
    REG(E1000_TDT) = 0;

    REG(E1000_RDBAL) = (uint32_t)(uint64_t)rx_ring;
    REG(E1000_RDLEN) = sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT;
    REG(E1000_RDH) = 0;
    REG(E1000_RDT) = E1000_RX_DESC_COUNT - 1;

    REG(E1000_RCTL) = (1 << 1) | (1 << 4) | (1 << 15);
    REG(E1000_TCTL) = (1 << 1) | (1 << 3) | (1 << 4);

    dev->tx_index = 0;
    dev->rx_index = 0;

    return true;
}

void e1000_send_packet(void *data, uint16_t len)
{
    e1000_tx_desc_t *desc = &tx_ring[tx_index];

    desc->addr = (uint64_t)data;
    desc->length = len;
    desc->cmd = 0x09;
    desc->status = 0;

    REG(E1000_TDT) = tx_index;

    tx_index = (tx_index + 1) % E1000_TX_DESC_COUNT;
}

void e1000_receive_poll()
{
    e1000_rx_desc_t *desc = &rx_ring[rx_index];

    if (desc->status & 0x01)
    {
        void *packet = (void*)(uint64_t)desc->addr;
        uint16_t len = desc->length;
        desc->status = 0;
        REG(E1000_RDT) = rx_index;
        rx_index = (rx_index + 1) % E1000_RX_DESC_COUNT;
    }
}