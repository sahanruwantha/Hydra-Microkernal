#include "net_driver.h"
#include "printk.h"
#include <stddef.h>

void net_print_stats(void) {
    struct net_stats *stats = net_get_stats();
    
    printk("[net] Network statistics:\n");
    printk("  TX packets: %u\n", stats->tx_packets);
    printk("  RX packets: %u\n", stats->rx_packets);
    printk("  TX bytes: %u\n", stats->tx_bytes);
    printk("  RX bytes: %u\n", stats->rx_bytes);
    printk("  TX errors: %u\n", stats->tx_errors);
    printk("  RX errors: %u\n", stats->rx_errors);
}

int net_is_packet_available(void) {
    struct net_packet *pkt = net_receive();
    if (pkt != NULL) {
        // Put it back - this is not ideal but works for demo
        // In real implementation, we'd have a peek function
        net_free_packet(pkt);
        return 1;
    }
    return 0;
}
