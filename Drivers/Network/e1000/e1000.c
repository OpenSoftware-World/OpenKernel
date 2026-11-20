#include "e1000.h"
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>
#include <OpenKernel/SystemLib/Memory/mem.h>

#define REG(offset) (*(volatile uint32_t*)((uint8_t*)dev->mmio_base + (offset)))

static e1000_t *dev;

static e1000_tx_desc_t *tx_ring;
static e1000_rx_desc_t *rx_ring;

static uint8_t tx_index = 0;
static uint8_t rx_index = 0;

/* Drop every handle this driver holds; the heap never reclaims anyway. */
static void e1000_reset_state(void) {
    dev = 0;
    tx_ring = 0;
    rx_ring = 0;
    tx_index = 0;
    rx_index = 0;
}

/*
 * NIC interrupt: reading ICR clears the reported causes, so a zero value
 * means another device on a shared line raised the line and nothing is
 * touched here. Receive causes drain the descriptor ring; the other
 * causes (link change, transmit) need no work in this driver yet.
 * The EOI is sent by interrupt_dispatch().
 */
static void e1000_irq_handler(registers_t *regs) {
    uint32_t icr;

    (void)regs;
    if (!dev) return;

    icr = REG(E1000_ICR);
    if (icr == 0) return;

    if (icr & E1000_RX_INTERRUPTS)
        e1000_handle_rx();
}

/*
 * Probe the device described by pdev and claim it.
 * Returns false when the entry is not a supported e1000 or when BAR0 does not
 * carry a usable MMIO window. Every allocation failure also bails out here,
 * because a NULL descriptor ring or a NULL receive buffer would otherwise be
 * dereferenced - and a NULL buffer handed to the NIC means DMA to address 0.
 */
bool_t e1000_init(pci_dev_t *pdev)
{
    uint32_t bar0;
    uint32_t mmio;

    e1000_reset_state();

    if (!pdev) return false;

    /* Only the device IDs this driver is written for. */
    if (pdev->vendor_id != E1000_VENDOR_ID) return false;
    if (pdev->dev_id != E1000_DEV_82540EM && pdev->dev_id != E1000_DEV_82545EM)
        return false;

    /* BAR0 is the register window: bit0 marks an I/O region, which this
       driver cannot use, and 0xFFFFFFFF means no BAR is implemented. */
    bar0 = pdev->bar[0];
    if (bar0 == 0 || bar0 == 0xFFFFFFFFu) return false;
    if (bar0 & 0x1u) return false;

    /* A 64-bit memory BAR keeps its upper half in BAR1. Anything above 4 GiB
       cannot be reached through a 32-bit pointer on this target. */
    if ((bar0 & 0x6u) == 0x4u && pdev->bar[1] != 0 && pdev->bar[1] != 0xFFFFFFFFu)
        return false;

    /* This driver is interrupt driven: without a routed IRQ line there is
       no way to learn that a descriptor was filled (0xFF = not connected). */
    if (pdev->interrupt_line > 15) return false;

    mmio = bar0 & ~0xFu;
    if (mmio == 0) return false;

    dev = (e1000_t*)kmalloc(sizeof(e1000_t));
    if (!dev) { e1000_reset_state(); return false; }
    memset(dev, 0, sizeof(e1000_t));

    dev->mmio_base = (uint32_t*)mmio;

    tx_ring = (e1000_tx_desc_t*)kmalloc(sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT);
    if (!tx_ring) { e1000_reset_state(); return false; }

    rx_ring = (e1000_rx_desc_t*)kmalloc(sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT);
    if (!rx_ring) { e1000_reset_state(); return false; }

    memset(tx_ring, 0, sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT);
    memset(rx_ring, 0, sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT);

    for (int i = 0; i < E1000_RX_DESC_COUNT; i++)
    {
        void *buf = kmalloc(2048);

        /* The NIC DMAs into this address: a NULL here would make it write to
           physical address 0 (IVT and BIOS data area). */
        if (!buf) { e1000_reset_state(); return false; }

        rx_ring[i].addr = (uint64_t)(uint32_t)buf;
        rx_ring[i].status = 0;
    }

    REG(E1000_TDBAL) = (uint32_t)tx_ring;
    REG(E1000_TDLEN) = sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT;
    REG(E1000_TDH) = 0;
    REG(E1000_TDT) = 0;

    REG(E1000_RDBAL) = (uint32_t)rx_ring;
    REG(E1000_RDLEN) = sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT;
    REG(E1000_RDH) = 0;
    REG(E1000_RDT) = E1000_RX_DESC_COUNT - 1;

    REG(E1000_RCTL) = (1 << 1) | (1 << 4) | (1 << 15);
    REG(E1000_TCTL) = (1 << 1) | (1 << 3) | (1 << 4);

    dev->tx_index = 0;
    dev->rx_index = 0;

    /* The rings are programmed, so the interrupt path can be armed: drop
       stale causes, unmask the receive ones and hook the PCI line.
       interrupt_init() must have run first. */
    REG(E1000_IMC) = 0xFFFFFFFFu;
    REG(E1000_IMS) = E1000_RX_INTERRUPTS;

    interrupt_register_handler((uint8_t)(PIC_IRQ_BASE + pdev->interrupt_line),
                               e1000_irq_handler);
    pic_clear_mask(pdev->interrupt_line);

    return true;
}

void e1000_send_packet(void *data, uint16_t len)
{
    e1000_tx_desc_t *desc;

    if (!dev || !tx_ring || !data || len == 0) return;

    desc = &tx_ring[tx_index];

    desc->addr = (uint64_t)(uint32_t)data;
    desc->length = len;
    desc->cmd = 0x09;
    desc->status = 0;

    REG(E1000_TDT) = tx_index;

    tx_index = (uint8_t)((tx_index + 1) % E1000_TX_DESC_COUNT);
}

/*
 * Consume every receive descriptor the NIC has filled. Runs from the
 * NIC's PCI interrupt now; nothing polls it any more. The packet is still
 * discarded afterwards, because this project has no network stack yet.
 */
void e1000_handle_rx(void)
{
    e1000_rx_desc_t *desc;

    if (!dev || !rx_ring) return;

    /* Bounded by the ring size: a status bit that never clears cannot
       turn the interrupt handler into a spin loop. */
    for (int budget = 0; budget < E1000_RX_DESC_COUNT; budget++) {
        desc = &rx_ring[rx_index];

        if (!(desc->status & 0x01))
            break;

        desc->status = 0;
        REG(E1000_RDT) = rx_index;
        rx_index = (uint8_t)((rx_index + 1) % E1000_RX_DESC_COUNT);
    }
}
