#ifndef AHCI_H
#define AHCI_H

#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/Drivers/PCIBus/pci.h>

/* ============================================================
   AHCI Register Offsets (from ABAR)
   ============================================================ */

#define AHCI_CAP       0x00  /* Host Capabilities */
#define AHCI_GHC       0x04  /* Global Host Control */
#define AHCI_IS        0x08  /* Interrupt Status */
#define AHCI_PI        0x0C  /* Ports Implemented */
#define AHCI_VS        0x10  /* Version */
#define AHCI_AHCICTL   0x24  /* AHCI Control */

#define AHCI_PORT_SIZE 0x80
#define AHCI_PORT_BASE 0x100

/* Port Registers */
#define PORT_CLB      0x00  /* Command List Base */
#define PORT_CLBU     0x04  /* Command List Base Upper */
#define PORT_FB       0x08  /* FIS Base */
#define PORT_FBU      0x0C  /* FIS Base Upper */
#define PORT_IS       0x10  /* Interrupt Status */
#define PORT_IE       0x14  /* Interrupt Enable */
#define PORT_CMD      0x18  /* Command and Status */
#define PORT_TFD      0x20  /* Task File Data */
#define PORT_SIG      0x24  /* Signature */
#define PORT_SSTS     0x28  /* SATA Status */
#define PORT_SCTL     0x2C  /* SATA Control */
#define PORT_SERR     0x30  /* SATA Error */
#define PORT_SACT     0x34  /* SATA Active */
#define PORT_CI       0x38  /* Command Issue */
#define PORT_SNTF     0x3C  /* SATA Notification */
#define PORT_FBS      0x40  /* FIS-based Switching */

/* PCI IDs */
#define AHCI_PCI_CLASS 0x01
#define AHCI_PCI_SUBCLASS 0x06
#define AHCI_PCI_PROGIF 0x01

#define AHCI_PCI_VID_INTEL 0x8086

/* AHCI Flags */
#define AHCI_GHC_AE   (1 << 31)  /* AHCI Enable */
#define AHCI_GHC_IE   (1 << 1)   /* Interrupt Enable */
#define AHCI_GHC_HR   (1 << 0)   /* HBA Reset */

#define AHCI_PORT_CMD_ST  (1 << 0)  /* Start */
#define AHCI_PORT_CMD_FRE (1 << 4)  /* FIS Receive Enable */
#define AHCI_PORT_CMD_CRA (1 << 31) /* Command List Running */
#define AHCI_PORT_CMD_FR  (1 << 14) /* FIS Receive Running */

#define AHCI_PORT_IS_TFES (1 << 30) /* Task File Error Status */
#define AHCI_PORT_IS_DHRS (1 << 0)  /* Device to Host Register FIS */
#define AHCI_PORT_IS_PSS  (1 << 27) /* Set Task Bits */

#define AHCI_SIG_ATA    0x00000101
#define AHCI_SIG_ATAPI  0xEB140101
#define AHCI_SIG_PM     0x96690101
#define AHCI_SIG_SEMB   0xC33C0101

#define AHCI_CMD_CFIS_LEN 0x10  /* 16 DWORDs */
#define AHCI_CMD_ACMD     0x0040
#define AHCI_CMD_WRITE    0x0040
#define AHCI_CMD_PREF     0x0004
#define AHCI_CMD_CLEAR    0x0001

/* FIS Types */
#define FIS_TYPE_REG_H2D  0x27
#define FIS_TYPE_REG_D2H  0x34
#define FIS_TYPE_DMA_ACT  0x39
#define FIS_TYPE_DMA_SETUP 0x41
#define FIS_TYPE_DATA     0x46
#define FIS_TYPE_BIST     0x58
#define FIS_TYPE_PIO_SETUP 0x5F
#define FIS_TYPE_DEV_BITS 0xA1

#define AHCI_MAX_PORTS    32
#define AHCI_MAX_SLOTS    32
#define AHCI_MAX_PRDT     8
#define AHCI_PRDT_MAX_SZ  (4 * 1024 * 1024) /* 4MB */

/* ============================================================
   AHCI Structures
   ============================================================ */

typedef volatile struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_ports;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  reserved[0x60];
    uint8_t  vendor[0x60];
    uint8_t  ports_data[0x80 * 32];
} __attribute__((packed)) ahci_hba_mem_t;

typedef volatile struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed)) ahci_port_t;

typedef struct {
    uint32_t dw0;
    uint32_t desc_base;
    uint32_t desc_base_upper;
    uint32_t rsv0;
} __attribute__((packed)) ahci_cmd_hdr_t;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv0[48];
    uint8_t  prdt[128 * AHCI_MAX_PRDT];
} __attribute__((packed)) ahci_cmd_table_t;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc: 22;
    uint32_t rsv1: 9;
    uint32_t i: 1;
} __attribute__((packed)) ahci_prdt_entry_t;

typedef struct {
    uint8_t  fis_type;
    uint8_t  pmport: 4;
    uint8_t  rsv0: 3;
    uint8_t  c: 1;
    uint8_t  cmd;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  rsv1[4];
} __attribute__((packed)) ahci_fis_reg_h2d_t;

typedef struct {
    uint8_t  fis_type;
    uint8_t  pmport: 4;
    uint8_t  rsv0: 2;
    uint8_t  i: 1;
    uint8_t  c: 1;
    uint8_t  status;
    uint8_t  error;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  rsv1[2];
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  rsv2[10];
} __attribute__((packed)) ahci_fis_reg_d2h_t;

/* Port state for driver */
typedef struct {
    ahci_port_t* port;
    int          port_num;
    int          type;
    int          present;
    uint64_t     total_sectors;
    uint32_t     sector_size;
    ahci_cmd_table_t* cmd_table;
    void*        rx_fis;
} ahci_port_state_t;

/* Driver public API */
int  ahci_init(void);
int  ahci_scan_ports(void);
void ahci_read_sector(int port, uint32_t lba, uint32_t count, uint8_t* buf);
void ahci_write_sector(int port, uint32_t lba, uint32_t count, const uint8_t* buf);
int  ahci_get_port_count(void);
ahci_port_state_t* ahci_get_port(int index);
void ahci_print_info(void);

#define AHCI_DRIVER_NAME "OpenKernel AHCI SATA Driver"
#define AHCI_DRIVER_VER "1.0"
#define AHCI_DRIVER_DESC "AHCI SATA controller driver for OpenKernel"
#define AHCI_DRIVER_AUTHOR "OpenSoftware-World"
#define AHCI_DRIVER_KRNL_VER "2.0"

#endif
