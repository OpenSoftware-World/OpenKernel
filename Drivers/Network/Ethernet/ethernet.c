#include "ethernet.h"
#include <OpenKernel/SystemLib/Mem/mem.h>

static net_device_t *device_list = 0;

void ethernet_register_device(net_device_t *dev) {
    dev->next = device_list;
    device_list = dev;
}

void ethernet_send(uint8_t *dest_mac, uint16_t ethertype, void *payload, uint16_t len) {
    uint8_t frame[ETH_MAX_FRAME_SIZE];
    eth_header_t *hdr = (eth_header_t*)frame;
    memset(hdr->src, 0xAA, 6);
    memcpy(hdr->dest, dest_mac, 6);
    hdr->ethertype = ethertype;
    memcpy(frame + sizeof(eth_header_t), payload, len);
    uint16_t frame_len = sizeof(eth_header_t) + len;

    if (device_list && device_list->send) {
        device_list->send(frame, frame_len);
    }
}