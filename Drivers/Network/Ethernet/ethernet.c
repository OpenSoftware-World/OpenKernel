#include "ethernet.h"
#include <OpenKernel/SystemLib/Memory/mem.h>

static net_device_t *device_list = 0;

void ethernet_register_device(net_device_t *dev) {
    if (!dev) return;

    dev->next = device_list;
    device_list = dev;
}

void ethernet_send(uint8_t *dest_mac, uint16_t ethertype, void *payload, uint16_t len) {
    uint8_t frame[ETH_MAX_FRAME_SIZE];
    eth_header_t *hdr = (eth_header_t*)frame;
    const uint16_t max_payload = (uint16_t)(ETH_MAX_FRAME_SIZE - sizeof(eth_header_t));

    if (!dest_mac || !payload) return;

    /* frame[] is one MTU wide: an unchecked len would run off the end of the
       stack buffer, and sizeof(eth_header_t) + len would wrap the 16 bit
       frame_len back to a tiny value. Cap the payload instead. */
    if (len > max_payload) len = max_payload;

    memset(hdr->src, 0xAA, 6);
    memcpy(hdr->dest, dest_mac, 6);
    hdr->ethertype = ethertype;
    memcpy(frame + sizeof(eth_header_t), payload, len);
    uint16_t frame_len = (uint16_t)(sizeof(eth_header_t) + len);

    if (device_list && device_list->send) {
        device_list->send(frame, frame_len);
    }
}