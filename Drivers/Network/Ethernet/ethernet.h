#ifndef ETHERNET_H
#define ETHERNET_H

#include <OpenKernel/SystemLib/Std/types.h>

#define ETH_MAX_FRAME_SIZE 1518

typedef struct {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IP4 0x0800

typedef struct net_device {
    char name[16];
    void (*send)(void *data, uint16_t len);
    void (*receive)(void *data, uint16_t len);
    struct net_device *next;
} net_device_t;

void ethernet_init();
void ethernet_register_device(net_device_t *dev);
void ethernet_send(uint8_t *dest_mac, uint16_t ethertype, void *payload, uint16_t len);

#define ETHERNET_DRIVER_NAME "OpenKernel Ethernet Driver"
#define ETHERNET_DRIVER_VER "0.1"
#define ETHERNET_DRIVER_DESC "A Ethernet driver for OpenKernel"
#define ETHERNET_DRIVER_AUTHOR "OpenSoftware-World"
#define ETHERNET_DRIVER_KRNL_VER "2.0"

#endif