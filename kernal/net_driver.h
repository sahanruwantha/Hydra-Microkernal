#ifndef NET_DRIVER_H
#define NET_DRIVER_H

#include <stdint.h>

// Packet buffer structure
struct net_packet {
    uint8_t data[1514];  // Maximum Ethernet frame size
    uint16_t length;
    struct net_packet *next;
};

// Network statistics
struct net_stats {
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
    uint32_t tx_errors;
    uint32_t rx_errors;
};

void net_init(void);
int net_send(const uint8_t *data, uint16_t len);
struct net_packet *net_receive(void);
void net_free_packet(struct net_packet *pkt);
struct net_stats *net_get_stats(void);
void net_interrupt_handler(void);

#endif
