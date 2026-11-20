#include "serial.h"
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>

#define SERIAL_LINE_STATUS_DATA_READY 0x01
#define SERIAL_LINE_STATUS_THRE 0x20

/* One shared size for both ring buffers; the uint8_t indices wrap for free. */
#define SERIAL_BUF_SIZE 256

/* Upper bound on IIR rounds per interrupt: a cause that never clears must
   not trap the handler in interrupt context. */
#define SERIAL_DRAIN_LIMIT 64

/* Bounded THRE wait for the queue-full fallback path only - a dead UART
   must drop a byte rather than hang the caller. */
#define SERIAL_TX_SPINS 50000

static bool_t serial_status = true;

/*
 * TX is queued by serial_print_char() and drained by IRQ4 (or inline when
 * interrupts are off); RX is filled by IRQ4. Both indices are touched from
 * task and interrupt context, so they are volatile: serial_read_char()
 * sleeps in hlt() and must see the byte the handler stored before it wakes.
 */
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static char tx_buf[SERIAL_BUF_SIZE];
static char rx_buf[SERIAL_BUF_SIZE];

/* Set once IRQ4 is unmasked and the UART's interrupts are enabled: from
   then on the handler, not the caller, owns the data port. */
static bool_t serial_irq_ready = false;

/* Move queued bytes into the UART while it can take them. The next THRE
   interrupt - or the caller, when interrupts are off - continues the rest. */
static void serial_tx_drain(void) {
    while (tx_head != tx_tail && serial_is_transmit_empty()) {
        outb(COM1_PORT, tx_buf[tx_tail]);
        tx_tail = (uint8_t)((tx_tail + 1) % SERIAL_BUF_SIZE);
    }
}

/* Append one received byte; drop it when the ring is full (an IRQ handler
   must never block, and a full ring means nobody is reading anyway). */
static void serial_rx_push(char c) {
    uint8_t next = (uint8_t)((rx_head + 1) % SERIAL_BUF_SIZE);

    if (next == rx_tail) return;
    rx_buf[rx_head] = c;
    rx_head = next;
}

/* Pull bytes out of the UART when IRQ4 is not driving them. Safe only
   while the handler cannot be running: interrupts off, or no handler. */
static void serial_rx_harvest(void) {
    if (serial_irq_ready && (eflags_r() & EFLAGS_IF)) return;

    for (int i = 0; i < SERIAL_DRAIN_LIMIT; i++) {
        if (!(inb(COM1_LSR) & SERIAL_LINE_STATUS_DATA_READY)) break;
        serial_rx_push((char)inb(COM1_PORT));
    }
}

/*
 * IRQ4 handler. Reading the interrupt-identification register both
 * reports the cause and clears it - a THRE interrupt that is never cleared
 * would leave the UART's INTR line high, which on a shared line would
 * starve the other device (and storm a level-triggered PIC). TX hands the
 * UART its next queued byte, RX empties the FIFO into the ring; each pass
 * is bounded by SERIAL_DRAIN_LIMIT. The EOI is sent by interrupt_dispatch().
 */
static void serial_irq_handler(registers_t *regs) {
    (void)regs;

    for (int i = 0; i < SERIAL_DRAIN_LIMIT; i++) {
        uint8_t iir = inb(COM1_IIR);
        uint8_t type;

        if (iir & 0x01) break;                          /* no interrupt pending */

        type = iir & 0x0E;
        if (type == 0x02) {                             /* THR empty */
            serial_tx_drain();
        } else if (type == 0x04 || type == 0x0C) {      /* data ready / FIFO timeout */
            while (inb(COM1_LSR) & SERIAL_LINE_STATUS_DATA_READY)
                serial_rx_push((char)inb(COM1_PORT));
        } else {
            break;                                      /* a cause we never enable */
        }
    }
}

void serial_init() {
    serial_status = true;
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
    outb(COM1_PORT + 4, 0x1E);
    outb(COM1_PORT + 0, 0xAE);
    if (inb(COM1_PORT + 0) != 0xAE) {
        return;
    }
    outb(COM1_PORT + 4, 0x0F);
    outb(COM1_PORT + 1, 0x03);
    interrupt_register_handler((uint8_t)(PIC_IRQ_BASE + SERIAL_IRQ_LINE), serial_irq_handler);
    pic_clear_mask(SERIAL_IRQ_LINE);
    serial_irq_ready = true;
}

int serial_is_transmit_empty() {
    return inb(COM1_PORT + 5) & SERIAL_LINE_STATUS_THRE;
}

/*
 * Queue one byte for transmission. IRQ4 drains the queue while
 * interrupts are on; with them off - or before serial_init() has run -
 * the bytes are pushed out here, exactly as the polled driver did.
 */
void serial_print_char(char c) {
    uint8_t next;

    if (!serial_status) return;

    next = (uint8_t)((tx_head + 1) % SERIAL_BUF_SIZE);
    if (next == tx_tail) {
        uint32_t flags = eflags_r();

        cli();
        while (tx_head != tx_tail) {
            uint32_t spins = SERIAL_TX_SPINS;

            while (!serial_is_transmit_empty() && spins > 0) spins--;
            if (spins == 0) break;              /* UART is not taking data */

            outb(COM1_PORT, tx_buf[tx_tail]);
            tx_tail = (uint8_t)((tx_tail + 1) % SERIAL_BUF_SIZE);
        }
        if (flags & EFLAGS_IF) sti();

        if ((uint8_t)((tx_head + 1) % SERIAL_BUF_SIZE) == tx_tail) return; /* still full: drop */
    }

    tx_buf[tx_head] = c;
    tx_head = next;
    if (serial_irq_ready) {
        uint32_t flags = eflags_r();

        cli();
        serial_tx_drain();
        if (flags & EFLAGS_IF) sti();
    } else {
        while (tx_head != tx_tail) {
            while (!serial_is_transmit_empty()) ;
            outb(COM1_PORT, tx_buf[tx_tail]);
            tx_tail = (uint8_t)((tx_tail + 1) % SERIAL_BUF_SIZE);
        }
    }
}

void serial_print_string(const char *string) {
    if (!serial_status) return;
    while (*string) {
        if (*string == '\n') {
            serial_print_char('\r');
        }
        serial_print_char(*string++);
    }
}

int serial_received() {
    serial_rx_harvest();
    return rx_head != rx_tail;
}

/*
 * Blocking read. When IRQ4 is hooked and interrupts are on the handler
 * fills the ring and this sleeps in hlt(); otherwise the UART is drained
 * here, which is the original polled behaviour.
 */
char serial_read_char() {
    for (;;) {
        serial_rx_harvest();
        if (rx_head != rx_tail) break;
        if (serial_irq_ready && (eflags_r() & EFLAGS_IF)) hlt();
    }

    char c = rx_buf[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1) % SERIAL_BUF_SIZE);
    return c;
}

int serial_try_read_char(char *c) {
    if (!c) return 0;

    serial_rx_harvest();
    if (rx_head == rx_tail) return 0;

    *c = rx_buf[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1) % SERIAL_BUF_SIZE);
    return 1;
}

int serial_has_error() {
    uint8_t stat = inb(COM1_LSR);
    return stat & (0x0E | 0x80);
}

int serial_get_error() {
    return inb(COM1_LSR) & (SERIAL_ERR_OVERRUN | SERIAL_ERR_PARITY | SERIAL_ERR_FRAME | SERIAL_ERROR_FIFO);
}

void serial_clear_errors() {
    inb(COM1_LSR);
}

void serial_log(const char* tag, const char* msg) {
    serial_print_string("[");
    serial_print_string(tag);
    serial_print_string("]");
    serial_print_string(msg);
    serial_print_string("\n");
}

void serial_success(const char* msg) {
    serial_log("SUCCESS", msg);
}

void serial_error(const char* msg) {
    serial_log("ERROR", msg);
}

void serial_warn(const char* msg) {
    serial_log("WARNING", msg);
}

void serial_info(const char* msg) {
    serial_log("INFO", msg);
}

void serial_unknown(const char* msg) {
    serial_log("UNKNOWN", msg);
}

void serial_set_status(bool_t val) {
    serial_status = val;
}

void serial_loopback_test() {
    outb(COM1_MCR, 0x1E);
    outb(COM1_PORT, 0xAE);
    if (inb(COM1_PORT) != 0xAE) {
        serial_error("Loopback test FAILED.");
    }
    else {
        serial_success("Loopback test OK.");
    }
    outb(COM1_MCR, 0x0F);
}