#include "rtl8139.h"
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/Drivers/PCIBus/pci.h>

static rtl8139_t nic_state;
static rtl8139_t *nic = 0;

static uint8_t rx_buffer[RTL_RX_BUF_SIZE];
static uint32_t rx_pos = 0;

/*
 * NIC interrupt: read the ISR first - a zero value means another device on
 * a shared line raised it, so nothing is cleared here and the next handler
 * in the chain gets its turn. The set bits are written back to clear them
 * (write-1-to-clear), then the receive ring is drained. The EOI itself is
 * sent by interrupt_dispatch().
 */
static void rtl8139_irq_handler(registers_t *regs) {
    uint16_t isr;

    (void)regs;
    if (!nic) return;

    isr = inw(nic->io_base + RTL_REG_ISR);
    if (isr == 0) return;

    outw(nic->io_base + RTL_REG_ISR, isr);

    if (isr & (RTL_INT_RXOK | RTL_INT_RXERR | RTL_INT_RXOVW)) {
        /* One interrupt can cover several queued packets while the ISR bit
           is cleared exactly once, and the chip does not re-raise RXOK for
           packets it already wrote - so drain until the ring reports empty
           or the remaining packets would wait forever. The bound stops a
           stuck buffer-empty flag from trapping the handler. */
        for (int i = 0; i < RTL_RX_DRAIN_LIMIT; i++) {
            if ((inb(nic->io_base + RTL_REG_CMD) & 0x01) == 0) break;
            rtl8139_handle_rx();
        }
    }
}

/*
 * Probe the device described by pdev and claim it.
 * Returns false when the entry is not an RTL8139 or when BAR0 does not carry
 * a usable I/O window, in which case nic stays NULL and every other entry
 * point becomes a no-op instead of touching a bogus port.
 */
bool_t rtl8139_init(pci_dev_t *pdev)
{
    uint32_t bar0;

    nic = 0;
    rx_pos = 0;

    if (!pdev) return false;
    if (pdev->vendor_id != RTL8139_VENDOR_ID || pdev->dev_id != RTL8139_DEVICE_ID)
        return false;

    /* BAR0 holds the I/O port window: bit0 marks an I/O region, bits 31:2 the
       base, and an unimplemented BAR reads back as 0xFFFFFFFF. */
    bar0 = pdev->bar[0];
    if (bar0 == 0 || bar0 == 0xFFFFFFFFu) return false;
    if ((bar0 & 0x1u) == 0) return false;
    if ((bar0 & ~0x3u) == 0) return false;

    /* This driver is interrupt driven: without a routed IRQ line there is
       no way to learn that a packet arrived (0xFF means "not connected"). */
    if (pdev->interrupt_line > 15) return false;

    nic_state.io_base = bar0 & ~0x3u;
    nic = &nic_state;

    outb(nic->io_base + RTL_REG_CMD, RTL_CMD_RESET);
    outb(nic->io_base + RTL_REG_CMD, RTL_CMD_RX_ENABLE | RTL_CMD_TX_ENABLE);
    outl(nic->io_base + RTL_REG_RX_BUF, (uint32_t)&rx_buffer);

    /* The receive ring exists now, so RX interrupts can be enabled: clear
       whatever reset left behind, unmask the events we handle, then hook
       the PCI interrupt line. */
    outw(nic->io_base + RTL_REG_ISR, 0xFFFF);
    outw(nic->io_base + RTL_REG_IMR,
         RTL_INT_RXOK | RTL_INT_RXERR | RTL_INT_TXOK | RTL_INT_RXOVW);

    interrupt_register_handler((uint8_t)(PIC_IRQ_BASE + pdev->interrupt_line),
                               rtl8139_irq_handler);
    pic_clear_mask(pdev->interrupt_line);
    return true;
}

void rtl8139_send_packet(void *data, uint16_t len)
{
    static int tx_index = 0;
    uint16_t port;

    if (!nic || !data || len == 0) return;

    port = nic->io_base + RTL_REG_TX_ADDR0 + (tx_index * 4);
    outl(port, (uint32_t)data);
    port = nic->io_base + RTL_REG_TX_STATUS0 + (tx_index * 4);
    outl(port, len);
    tx_index = (tx_index + 1) % 4;
}

/*
 * Drain the receive ring. Runs from the NIC's PCI interrupt whenever the
 * ISR reported RXOK / RXERR / RXOVW - nothing polls it any more. The
 * packet itself is still not handed to a network stack, which does not
 * exist yet in this project.
 */
void rtl8139_handle_rx(void)
{
    uint8_t *packet;
    uint16_t len;

    if (!nic) return;

    if ((inb(nic->io_base + RTL_REG_CMD) & 0x01) == 0) return;

    /* The 4 byte packet header has to fit inside the ring buffer before it is
       read; without this the last 4 bytes of rx_buffer would be overrun. */
    if (rx_pos + 4 > RTL_RX_BUF_SIZE) {
        rx_pos = 0;
        return;
    }

    packet = &rx_buffer[rx_pos];
    len = *(uint16_t*)(packet + 2);

    /* Advance by what the hardware reported; a bogus length only restarts the
       ring, it can no longer walk off the end of rx_buffer. */
    rx_pos += (uint32_t)len + 4;
    if (rx_pos >= RTL_RX_BUF_SIZE)
        rx_pos = 0;
}
