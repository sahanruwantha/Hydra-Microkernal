#include "net_driver.h"
#include "printk.h"
#include <stddef.h>

int net_recv_packet(uint8_t *buffer, uint16_t max_len) {
    struct net_packet *pkt = net_receive();
    if (pkt == NULL) {
        return 0; // No packet available
    }
    
    uint16_t copy_len = (pkt->length < max_len) ? pkt->length : max_len;
    
    // Copy packet data to user buffer
    for (int i = 0; i < copy_len; i++) {
        buffer[i] = pkt->data[i];
    }
    
    uint16_t actual_len = pkt->length;
    net_free_packet(pkt);
    
    return actual_len;
}
