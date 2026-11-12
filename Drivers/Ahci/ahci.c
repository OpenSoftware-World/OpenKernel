#include "ahci.h"

#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/Memory/mem.h>

/* ============================================================
   Internal State
   ============================================================ */

static ahci_hba_mem_t* hba_mem = NULL;
static ahci_port_state_t ports[AHCI_MAX_PORTS];
static int port_count = 0;

/* ============================================================
   Compile-time layout checks against the AHCI 1.3.1 register map.
   __builtin_offsetof keeps the offsets constant-foldable without
   pulling in <stddef.h>, whose size_t conflicts with types.h.
   ============================================================ */

STATIC_ASSERT(__builtin_offsetof(ahci_hba_mem_t, ports_data) == AHCI_PORT_BASE, hba_ports_at_0x100);
STATIC_ASSERT(sizeof(ahci_port_t) == AHCI_PORT_SIZE, port_block_is_0x80_bytes);
STATIC_ASSERT(sizeof(ahci_cmd_hdr_t) == 0x20, cmd_header_is_32_bytes);
STATIC_ASSERT(__builtin_offsetof(ahci_cmd_table_t, prdt) == 0x80, prdt_starts_at_0x80);
STATIC_ASSERT(sizeof(ahci_prdt_entry_t) == 0x10, prdt_entry_is_16_bytes);
STATIC_ASSERT(sizeof(ahci_fis_reg_h2d_t) == 0x14, h2d_fis_is_20_bytes);
STATIC_ASSERT(sizeof(ahci_fis_reg_d2h_t) == 0x1C, d2h_fis_is_28_bytes);

/* ============================================================
   Timeouts / PCI configuration used by this driver
   ============================================================ */

#define AHCI_ENGINE_SPINS 1000000  /* port start/stop handshake   */
#define AHCI_CMD_SPINS    1000000  /* command completion poll     */

#define PCI_CONF_CMD   0x04
#define PCI_CONF_BAR5  0x24
#define PCI_CMD_MEM    (1u << 1)  /* Memory Space Enable */
#define PCI_CMD_MASTER (1u << 2)  /* Bus Master Enable    */

/* ============================================================
   Helper: print hex value (vga_print_hex renders 8 digits)
   ============================================================ */

static void prt_hex(uint32_t val) {
    vga_print_scr_nw("0x");
    vga_print_hex(val);
}

/* ============================================================
   Helper: print unsigned 64-bit decimal.
   vga_print_dec() takes an int and would wrap on large sector
   counts. The divisor 10 is constant, so GCC keeps the 64-bit
   division inline and the kernel gains no libgcc dependency.
   ============================================================ */

static void prt_dec(uint64_t val) {
    char buf[21];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else {
        char t[21]; int ti = 0;
        while (val > 0) { t[ti++] = (char)((val % 10) + '0'); val /= 10; }
        while (ti > 0) buf[i++] = t[--ti];
    }
    buf[i] = '\0';
    vga_print_scr_nw(buf);
}

/* ============================================================
   Helper: aligned DMA buffer.
   kmalloc() guarantees only 4-byte alignment, but the HBA wants
   1 KiB for the command list, 256 bytes for the receive-FIS area
   and 128 bytes for command tables. Buffers must be reachable as
   physical addresses, which holds because this kernel maps its
   memory 1:1 (identity).
   ============================================================ */

static void* kmalloc_aligned(uint32_t size, uint32_t align) {
    void* raw = kmalloc(size + align);
    if (!raw) return NULL;
    return (void*)ALIGN_UP((uintptr_t)raw, align);
}

/* ============================================================
   HBA Port Engine Control (bounded waits - a dead HBA must not
   hang the kernel)
   ============================================================ */

static int port_wait_idle(ahci_port_t* port, uint32_t mask) {
    uint32_t spins = AHCI_ENGINE_SPINS;
    while (spins-- > 0) {
        if (!(port->cmd & mask)) return 0;
    }
    return -1;
}

/* Stop order per spec: clear ST and wait for the command list to
   stop, then clear FRE and wait for the FIS receiver to stop. */
static int port_stop(ahci_port_t* port) {
    port->cmd &= ~AHCI_PORT_CMD_ST;
    if (port_wait_idle(port, AHCI_PORT_CMD_CRA) != 0) return -1;
    port->cmd &= ~AHCI_PORT_CMD_FRE;
    if (port_wait_idle(port, AHCI_PORT_CMD_FR) != 0) return -1;
    return 0;
}

/* Start order per spec: enable FIS receive first, then start. */
static int port_start(ahci_port_t* port) {
    if (port_wait_idle(port, AHCI_PORT_CMD_CRA | AHCI_PORT_CMD_FR) != 0)
        return -1;
    port->cmd |= AHCI_PORT_CMD_FRE;
    port->cmd |= AHCI_PORT_CMD_ST;
    return 0;
}

/* ============================================================
   Find Command Slot (first free slot)
   ============================================================ */

static int find_cmdslot(ahci_port_t* port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < AHCI_MAX_SLOTS; i++) {
        if (!(slots & (1u << i))) return i;
    }
    return -1;
}

/* ============================================================
   Configure Command Header / PRDT for a port
   ============================================================ */

static int configure_port(ahci_port_state_t* ps) {
    ahci_port_t* port = ps->port;

    /* Stop the engine before reprogramming the base registers */
    if (port_stop(port) != 0) return -1;

    /* Command list: 32 entries * 32 bytes = 1 KiB, 1 KiB aligned */
    void* clb = kmalloc_aligned(1024, 1024);
    if (!clb) return -1;
    memset(clb, 0, 1024);

    /* Received FIS area: 256 bytes, 256-byte aligned */
    void* fis = kmalloc_aligned(256, 256);
    if (!fis) { kfree(clb); return -1; }
    memset(fis, 0, 256);

    /* Command tables: 128-byte aligned per slot */
    ps->cmd_table = (ahci_cmd_table_t*)kmalloc_aligned(
        sizeof(ahci_cmd_table_t) * AHCI_MAX_SLOTS, 128);
    if (!ps->cmd_table) {
        kfree(clb); kfree(fis); return -1;
    }
    memset(ps->cmd_table, 0, sizeof(ahci_cmd_table_t) * AHCI_MAX_SLOTS);

    /* Program the port base registers */
    port->clb = (uint32_t)(uintptr_t)clb;
    port->clbu = (uint32_t)(((uint64_t)(uintptr_t)clb) >> 32);
    port->fb = (uint32_t)(uintptr_t)fis;
    port->fbu = (uint32_t)(((uint64_t)(uintptr_t)fis) >> 32);

    /* Point every command header at its command table */
    ahci_cmd_hdr_t* hdr = (ahci_cmd_hdr_t*)clb;
    for (int i = 0; i < AHCI_MAX_SLOTS; i++) {
        hdr[i].dw0 = 0;
        hdr[i].desc_base = (uint32_t)(uintptr_t)&ps->cmd_table[i];
        hdr[i].desc_base_upper =
            (uint32_t)(((uint64_t)(uintptr_t)&ps->cmd_table[i]) >> 32);
        hdr[i].rsv0 = 0;
    }

    ps->cmd_list = clb;
    ps->rx_fis = fis;

    /* Acknowledge stale status/errors and keep interrupts masked
       (this driver polls for completion) */
    port->is = (uint32_t)-1;
    port->serr = (uint32_t)-1;
    port->ie = 0;
    port->fbs = 0;

    return port_start(port);
}

/* ============================================================
   Issue one ATA command with a single data buffer (internal core)
   ============================================================ */

static int ahci_issue(int port_num, uint8_t ata_cmd, uint8_t device_reg,
                      uint64_t lba, uint16_t count, uint8_t* buf, int write) {
    if (port_num < 0 || port_num >= port_count) return -1;
    ahci_port_state_t* ps = &ports[port_num];
    if (!ps->present || !ps->cmd_table || !ps->cmd_list ||
        !buf || count == 0 || ps->sector_size == 0)
        return -1;

    /* Stay inside the disk. total_sectors == 0 means "unknown"
       (during IDENTIFY) and skips this check. */
    if (ps->total_sectors && (lba + count) > ps->total_sectors) return -1;

    uint32_t bytes = (uint32_t)count * ps->sector_size;
    if (bytes < count) return -1;  /* multiplication overflow */
    if (bytes > (uint32_t)(AHCI_MAX_PRDT * AHCI_PRDT_MAX_SZ)) return -1;

    ahci_port_t* port = ps->port;

    /* The task file must be idle before a command is issued */
    if (port->tfd & (AHCI_TFD_BSY | AHCI_TFD_DRQ)) return -1;

    int slot = find_cmdslot(port);
    if (slot < 0) return -1;

    ahci_cmd_hdr_t* cmd_hdr = (ahci_cmd_hdr_t*)ps->cmd_list;
    ahci_cmd_table_t* cmd_tbl = &ps->cmd_table[slot];

    /* Build the PRD list, splitting at 4 MiB boundaries - no PRD
       entry may cross one. */
    ahci_prdt_entry_t* prdt = (ahci_prdt_entry_t*)cmd_tbl->prdt;
    uint32_t remaining = bytes;
    uintptr_t addr = (uintptr_t)buf;
    int nprdt = 0;

    while (remaining > 0 && nprdt < AHCI_MAX_PRDT) {
        uint32_t chunk = remaining;
        uint32_t to_boundary = 0x400000 - (uint32_t)(addr & 0x3FFFFF);
        if (chunk > to_boundary) chunk = to_boundary;

        prdt[nprdt].dba = (uint32_t)addr;
        prdt[nprdt].dbau = (uint32_t)(((uint64_t)addr) >> 32);
        prdt[nprdt].dbc = chunk - 1;
        prdt[nprdt].i = 1;

        addr += chunk;
        remaining -= chunk;
        nprdt++;
    }
    if (remaining > 0) return -1;  /* needs more entries than AHCI_MAX_PRDT */

    /* Command table: Register H2D FIS */
    memset(cmd_tbl->cfis, 0, sizeof(cmd_tbl->cfis));
    ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)cmd_tbl->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->cmd = ata_cmd;
    fis->device = device_reg;
    fis->lba0 = (uint8_t)(lba & 0xFF);
    fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
    fis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    fis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    fis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    fis->lba5 = (uint8_t)((lba >> 40) & 0xFF);
    fis->countl = (uint8_t)(count & 0xFF);
    fis->counth = (uint8_t)((count >> 8) & 0xFF);

    /* Command header DW0: CFL | W | P | PRDTL */
    cmd_hdr[slot].dw0 = (AHCI_CMD_CFIS_LEN & 0x1F)
                      | (write ? AHCI_CMD_WRITE : 0)
                      | AHCI_CMD_PREF
                      | AHCI_CMD_PRDTL(nprdt);

    /* Acknowledge old status/errors, then issue */
    port->is = (uint32_t)-1;
    port->serr = (uint32_t)-1;
    port->ci = (1u << slot);

    /* Poll for completion; the error bit is checked first so a
       command that finished with an error is never reported as a
       success. */
    uint32_t spins = AHCI_CMD_SPINS;
    int done = 0;
    while (spins-- > 0) {
        if (port->is & AHCI_PORT_IS_TFES) break;
        if (!(port->ci & (1u << slot))) { done = 1; break; }
        for (volatile int j = 0; j < 100; j++) ;
    }

    if (port->is & AHCI_PORT_IS_TFES) {
        /* Task-file error: clear it and fail */
        port->serr = (uint32_t)-1;
        port->is = (uint32_t)-1;
        return -1;
    }

    if (!done) {
        /* Never completed: reset the port engine to recover */
        port_stop(port);
        port->is = (uint32_t)-1;
        port->serr = (uint32_t)-1;
        port_start(port);
        return -1;
    }

    return 0;
}

/* ============================================================
   IDENTIFY DEVICE (internal): fills total_sectors from words
   100-103 (LBA48), falling back to words 60-61 (LBA28)
   ============================================================ */

static int ahci_identify(int port_num) {
    uint8_t id_buf[512];

    /* IDENTIFY is not LBA-addressed: device register 0, lba 0 */
    if (ahci_issue(port_num, AHCI_ATA_IDENTIFY_DEVICE, 0x00, 0, 1, id_buf, 0) != 0)
        return -1;

    uint16_t* id = (uint16_t*)id_buf;
    uint64_t lba48 = (uint64_t)id[100] | ((uint64_t)id[101] << 16)
                   | ((uint64_t)id[102] << 32) | ((uint64_t)id[103] << 48);
    uint32_t lba28 = (uint32_t)id[60] | ((uint32_t)id[61] << 16);

    uint64_t sectors = lba48;
    if (sectors == 0 || sectors == UINT64_MAX) sectors = lba28;

    ports[port_num].total_sectors = sectors;
    return 0;
}

/* ============================================================
   Read/Write sectors with READ/WRITE DMA EXT (48-bit LBA)
   ============================================================ */

static int ahci_rw(int port_num, uint32_t lba, uint32_t count,
                   uint8_t* buf, int write) {
    if (count == 0 || count > 0xFFFF) return -1;  /* 16-bit sector count */
    return ahci_issue(port_num,
                      write ? AHCI_ATA_WRITE_DMA_EXT : AHCI_ATA_READ_DMA_EXT,
                      AHCI_DEV_LBA, (uint64_t)lba, (uint16_t)count, buf, write);
}

/* ============================================================
   Public: Scan for attached SATA devices
   ============================================================ */

int ahci_scan_ports(void) {
    port_count = 0;

    if (!hba_mem) return -1;

    uint32_t pi = hba_mem->pi;
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (!(pi & (1u << i))) continue;

        ahci_port_t* p = (ahci_port_t*)&hba_mem->ports_data[i * AHCI_PORT_SIZE];

        /* SSTS.DET must report "device present, comms established" */
        if ((p->ssts & 0x0F) != 0x3) continue;

        uint32_t sig = p->sig;
        int type = 0;

        if (sig == AHCI_SIG_ATA)        type = 1; /* SATA */
        else if (sig == AHCI_SIG_ATAPI) type = 2; /* ATAPI */
        else if (sig == AHCI_SIG_PM)    type = 3; /* Port Multiplier */
        else if (sig == AHCI_SIG_SEMB)  type = 4; /* SEMB */
        else continue;

        if (type != 1) continue; /* Only SATA disks for now */

        ahci_port_state_t* ps = &ports[port_count];
        ps->port = p;
        ps->port_num = i;
        ps->type = type;
        ps->present = 1;
        ps->sector_size = 512;
        ps->total_sectors = 0;

        /* The port must be configured BEFORE any command is issued */
        if (configure_port(ps) != 0) {
            ps->present = 0;
            continue;
        }

        /* Identify the drive to learn its size */
        if (ahci_identify(port_count) != 0) {
            vga_print_scr_nw("[AHCI] IDENTIFY failed on port ");
            prt_dec((uint64_t)i);
            vga_print_scr_nw("\n");
        }

        port_count++;
    }

    return port_count;
}

/* ============================================================
   Public: Initialize AHCI driver
   ============================================================ */

int ahci_init(void) {
    /* Find AHCI controller via PCI (class 01:06:01) */
    pci_dev_t* dev = NULL;

    for (int i = 0; i < pci_get_dev_count(); i++) {
        dev = pci_get_device(i);
        if (!dev) continue;
        if (dev->class_mod == AHCI_PCI_CLASS &&
            dev->subclass == AHCI_PCI_SUBCLASS &&
            dev->prog_if == AHCI_PCI_PROGIF) {
            break;
        }
        dev = NULL;
    }

    if (!dev) {
        vga_print_scr_nw("[AHCI] No AHCI controller found via PCI\n");
        return -1;
    }

    /* BAR5 (ABAR) must be a memory BAR */
    uint32_t bar5 = pci_read(dev->bus, dev->dev, dev->func, PCI_CONF_BAR5);
    if (bar5 & 0x1) {
        vga_print_scr_nw("[AHCI] BAR5 is not a memory BAR\n");
        return -1;
    }
    uint32_t abar = bar5 & 0xFFFFFFF0; /* Lower 4 bits are flags */

    if (abar == 0) {
        vga_print_scr_nw("[AHCI] Invalid ABAR address\n");
        return -1;
    }

    /* Make sure the HBA is reachable: enable memory decoding and
       bus mastering (same policy Linux uses for AHCI devices) */
    uint32_t pci_cmd = pci_read(dev->bus, dev->dev, dev->func, PCI_CONF_CMD);
    if ((pci_cmd & (PCI_CMD_MEM | PCI_CMD_MASTER)) !=
        (PCI_CMD_MEM | PCI_CMD_MASTER)) {
        pci_write(dev->bus, dev->dev, dev->func, PCI_CONF_CMD,
                  pci_cmd | PCI_CMD_MEM | PCI_CMD_MASTER);
    }

    hba_mem = (ahci_hba_mem_t*)(uintptr_t)abar;

    /* Enable AHCI mode and keep global interrupts masked - this
       driver polls for completion. BIOS handoff (CAP2.BOH) is not
       implemented; the BIOS is assumed to have released the HBA. */
    hba_mem->ghc |= AHCI_GHC_AE;
    hba_mem->ghc &= ~AHCI_GHC_IE;

    vga_print_scr_nw("[AHCI] Controller found at ABAR: ");
    prt_hex(abar);
    vga_print_scr_nw(" | Cap: ");
    prt_hex(hba_mem->cap);
    vga_print_scr_nw(" | Version: ");
    prt_hex(hba_mem->vs);
    vga_print_scr_nw("\n");

    return 0;
}

/* ============================================================
   Public: Read sectors (0 on success, -1 on failure)
   ============================================================ */

int ahci_read_sector(int port, uint32_t lba, uint32_t count, uint8_t* buf) {
    return ahci_rw(port, lba, count, buf, 0);
}

/* ============================================================
   Public: Write sectors (0 on success, -1 on failure)
   ============================================================ */

int ahci_write_sector(int port, uint32_t lba, uint32_t count, const uint8_t* buf) {
    return ahci_rw(port, lba, count, (uint8_t*)buf, 1);
}

/* ============================================================
   Public: Get port info
   ============================================================ */

int ahci_get_port_count(void) {
    return port_count;
}

ahci_port_state_t* ahci_get_port(int index) {
    if (index < 0 || index >= port_count) return NULL;
    return &ports[index];
}

/* ============================================================
   Public: Print AHCI info
   ============================================================ */

void ahci_print_info(void) {
    vga_print_scr_nw("\n[AHCI] Ports found: ");
    prt_dec((uint64_t)port_count);
    vga_print_scr_nw("\n");

    for (int i = 0; i < port_count; i++) {
        vga_print_scr_nw("  Port ");
        prt_dec((uint64_t)ports[i].port_num);
        vga_print_scr_nw(": ");
        prt_dec(ports[i].total_sectors);
        vga_print_scr_nw(" sectors (");
        prt_dec(ports[i].total_sectors / 2048);
        vga_print_scr_nw(" MB)\n");
    }
}
