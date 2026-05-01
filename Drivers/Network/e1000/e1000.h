#ifndef E1000_H
#define E1000_H

#include <OpenKernel/SystemLib/Std/types.h>

#define E1000_VENDOR_ID 0x8086

#define E1000_DEV_82540EM 0x100E
#define E1000_DEV_82545EM 0x100F

#define E1000_TDBAL 0x3800
#define E1000_TDBAH 0x3804
#define E1000_TDLEN 0x3808
#define E1000_TDH 0x3810
#define E1000_TDT 0x3818

#define E1000_RDBAL 0x2800
#define E1000_RDBAH 0x2804
#define E1000_RDLEN 0x2808
#define E1000_RDH   0x2810
#define E1000_RDT   0x2818

#define E1000_TCTL 0x0400
#define E1000_RCTL 0x0100

#define E1000_STATUS 0x0008

#define E1000_TX_DESC_COUNT 32
#define E1000_RX_DESC_COUNT 32

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint8_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint32_t *mmio_base;
    e1000_tx_desc_t *tx_descs;
    e1000_rx_desc_t *rx_desc;
    uint8_t tx_index;
    uint8_t rx_index;
    bool_t link_up;
} e1000_t;

bool_t e1000_init();
void e1000_send_packet(void *data, uint16_t len);

#define E1000_DRIVER_NAME "OpenKernel Intel E1000 Driver"
#define E1000_DRIVER_VER "0.1"
#define E1000_DRIVER_DESC "A Intel E1000 for OpenKernel"
#define E1000_DRIVER_AUTHOR "OpenSoftware-World"
#define E1000_DRIVER_KRNL_VER "2.0"

#endif