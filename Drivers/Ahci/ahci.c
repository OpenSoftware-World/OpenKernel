#include "ahci.h"

#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/SystemLib/Std/std.h>

/* ============================================================
   Internal State
   ============================================================ */

static ahci_hba_mem_t* hba_mem = NULL;
static ahci_port_state_t ports[AHCI_MAX_PORTS];
static int port_count = 0;

/* ============================================================
   Helper: print hex value
   ============================================================ */

static void prt_hex(uint32_t val) {
    const char* hx = "0123456789ABCDEF";
    vga_print_scr_nw("0x");
    for (int i = 28; i >= 0; i -= 4) {
        char c[2] = { hx[(val >> i) & 0xF], '\0' };
        vga_print_scr_nw(c);
    }
}

static void prt_dec(uint32_t val) {
    char buf[12];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else {
        char t[12]; int ti = 0;
        while (val > 0) { t[ti++] = (val % 10) + '0'; val /= 10; }
        while (ti > 0) buf[i++] = t[--ti];
    }
    buf[i] = '\0';
    vga_print_scr_nw(buf);
}

/* ============================================================
   HBA Port Start
   ============================================================ */

static void port_start(ahci_port_t* port) {
    while (port->cmd & (AHCI_PORT_CMD_CRA | AHCI_PORT_CMD_FR))
        ;

    port->cmd &= ~AHCI_PORT_CMD_ST;
    port->cmd &= ~AHCI_PORT_CMD_FRE;

    while (port->cmd & AHCI_PORT_CMD_FR)
        ;

    port->cmd |= AHCI_PORT_CMD_FRE;
    port->cmd |= AHCI_PORT_CMD_ST;
}

/* ============================================================
   HBA Port Stop
   ============================================================ */

static void port_stop(ahci_port_t* port) {
    port->cmd &= ~AHCI_PORT_CMD_ST;
    port->cmd &= ~AHCI_PORT_CMD_FRE;
}

/* ============================================================
   Find Command Slot (wait for empty slot)
   ============================================================ */

static int find_cmdslot(ahci_port_t* port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < AHCI_MAX_SLOTS; i++) {
        if (!(slots & (1 << i))) return i;
    }
    return -1;
}

/* ============================================================
   Configure Command Header / PRDT for a port
   ============================================================ */

static int configure_port(ahci_port_state_t* ps) {
    ahci_port_t* port = ps->port;

    /* Stop port before reconfiguration */
    port_stop(port);

    /* Allocate command list (1K aligned, 32 entries * 32 bytes = 1024) */
    uint32_t clb_sz = 1024;
    void* clb = kmalloc(clb_sz);
    if (!clb) return -1;
    memset(clb, 0, clb_sz);

    /* Allocate received FIS (256 bytes, aligned) */
    uint32_t fis_sz = 256;
    void* fis = kmalloc(fis_sz);
    if (!fis) { kfree(clb); return -1; }
    memset(fis, 0, fis_sz);

    /* Allocate command table (aligned, 1024 bytes per entry) */
    ps->cmd_table = (ahci_cmd_table_t*)kmalloc(
        sizeof(ahci_cmd_table_t) * AHCI_MAX_SLOTS);
    if (!ps->cmd_table) {
        kfree(clb); kfree(fis); return -1;
    }
    memset(ps->cmd_table, 0, sizeof(ahci_cmd_table_t) * AHCI_MAX_SLOTS);

    /* Set CLB and FIS addresses in port registers */
    port->clb = (uint32_t)(uint64_t)clb;
    port->clbu = (uint32_t)((uint64_t)clb >> 32);
    port->fb = (uint32_t)(uint64_t)fis;
    port->fbu = (uint32_t)((uint64_t)fis >> 32);

    /* Initialize command headers to point to their tables */
    ahci_cmd_hdr_t* hdr = (ahci_cmd_hdr_t*)clb;
    for (int i = 0; i < AHCI_MAX_SLOTS; i++) {
        hdr[i].dw0 = 0;
        hdr[i].desc_base = (uint32_t)(uint64_t)&ps->cmd_table[i];
        hdr[i].desc_base_upper = (uint32_t)((uint64_t)&ps->cmd_table[i] >> 32);
        hdr[i].rsv0 = 0;
    }

    ps->rx_fis = fis;
    port_start(port);
    return 0;
}

/* ============================================================
   Read/Write a single sector using AHCI
   ============================================================ */

static int ahci_rw(int port_num, uint32_t lba, uint32_t count,
                   uint8_t* buf, int write) {
    if (port_num < 0 || port_num >= port_count) return -1;
    ahci_port_state_t* ps = &ports[port_num];
    ahci_port_t* port = ps->port;

    int slot = find_cmdslot(port);
    if (slot < 0) return -1;

    ahci_cmd_hdr_t* cmd_hdr = (ahci_cmd_hdr_t*)(uint64_t)port->clb;
    cmd_hdr[slot].dw0 = 0;

    /* PRD table: single entry for this buffer */
    ahci_cmd_table_t* cmd_tbl = &ps->cmd_table[slot];
    ahci_prdt_entry_t* prdt = &cmd_tbl->prdt[0];
    prdt->dba = (uint32_t)(uint64_t)buf;
    prdt->dbau = (uint32_t)((uint64_t)buf >> 32);
    prdt->dbc = (count * ps->sector_size) - 1;
    prdt->i = 1;

    /* FIS: Register H2D */
    ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)cmd_tbl->cfis;
    memset(cmd_tbl->cfis, 0, 64);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->cmd = write ? 0x35 : 0x25; /* WRITE DMA EXT / READ DMA EXT */
    fis->lba0 = lba & 0xFF;
    fis->lba1 = (lba >> 8) & 0xFF;
    fis->lba2 = (lba >> 16) & 0xFF;
    fis->lba3 = (lba >> 24) & 0xFF;
    fis->device = 1 << 6; /* LBA mode */
    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    /* Command header setup */
    cmd_hdr[slot].dw0 = (AHCI_CMD_CFIS_LEN & 0x1F);
    cmd_hdr[slot].dw0 |= (write ? AHCI_CMD_WRITE : 0);
    cmd_hdr[slot].dw0 |= AHCI_CMD_PREF;
    cmd_hdr[slot].dw0 |= 1; /* 1 PRDT entry */

    /* Issue command */
    port->is = (uint32_t)-1;
    port->ci = (1 << slot);

    /* Wait for completion */
    uint32_t timeout = 1000000;
    while (timeout--) {
        if (!(port->ci & (1 << slot))) break;
        if (port->is & AHCI_PORT_IS_TFES) {
            return -1; /* Task File Error */
        }
        for (volatile int j = 0; j < 100; j++) ;
    }
    if (timeout == 0) return -1;

    return 0;
}

/* ============================================================
   Public: Scan for AHCI controllers via PCI
   ============================================================ */

int ahci_scan_ports(void) {
    port_count = 0;

    if (!hba_mem) return -1;

    uint32_t pi = hba_mem->pi;
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (!(pi & (1 << i))) continue;

        ahci_port_t* p = (ahci_port_t*)&hba_mem->ports_data[i * AHCI_PORT_SIZE];

        uint32_t sig = p->sig;
        int type = 0;

        if (sig == AHCI_SIG_ATA)      type = 1; /* SATA */
        else if (sig == AHCI_SIG_ATAPI) type = 2; /* ATAPI */
        else if (sig == AHCI_SIG_PM)    type = 3; /* Port Multiplier */
        else if (sig == AHCI_SIG_SEMB)  type = 4; /* SEMB */
        else continue;

        if (type != 1) continue; /* Only SATA disks for now */

        ports[port_count].port = p;
        ports[port_count].port_num = i;
        ports[port_count].type = type;
        ports[port_count].present = 1;
        ports[port_count].sector_size = 512;

        /* Identify device to get total sectors */
        uint8_t id_buf[512];
        if (ahci_rw(port_count, 0, 1, id_buf, 0) == 0) {
            uint16_t* id = (uint16_t*)id_buf;
            uint32_t lba28 = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
            ports[port_count].total_sectors = lba28;
        } else {
            ports[port_count].total_sectors = 0;
        }

        if (configure_port(&ports[port_count]) != 0) {
            ports[port_count].present = 0;
            continue;
        }

        port_count++;
    }

    return port_count;
}

/* ============================================================
   Public: Initialize AHCI driver
   ============================================================ */

int ahci_init(void) {
    /* Find AHCI controller via PCI */
    pci_dev_t* dev = NULL;
    uint32_t abar = 0;

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

    /* Read BAR5 (ABAR) */
    abar = pci_read(dev->bus, dev->dev, dev->func, 0x24);
    abar &= 0xFFFFFFF0; /* Lower 4 bits are flags */

    if (abar == 0) {
        vga_print_scr_nw("[AHCI] Invalid ABAR address\n");
        return -1;
    }

    hba_mem = (ahci_hba_mem_t*)(uint64_t)abar;

    /* Enable AHCI mode */
    hba_mem->ghc |= AHCI_GHC_AE;

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
   Public: Read sectors
   ============================================================ */

void ahci_read_sector(int port, uint32_t lba, uint32_t count, uint8_t* buf) {
    ahci_rw(port, lba, count, buf, 0);
}

/* ============================================================
   Public: Write sectors
   ============================================================ */

void ahci_write_sector(int port, uint32_t lba, uint32_t count, const uint8_t* buf) {
    ahci_rw(port, lba, count, (uint8_t*)buf, 1);
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
    prt_dec(port_count);
    vga_print_scr_nw("\n");

    for (int i = 0; i < port_count; i++) {
        vga_print_scr_nw("  Port ");
        prt_dec(ports[i].port_num);
        vga_print_scr_nw(": ");
        prt_dec(ports[i].total_sectors);
        vga_print_scr_nw(" sectors (");
        prt_dec(ports[i].total_sectors / 2048);
        vga_print_scr_nw(" MB)\n");
    }
}
