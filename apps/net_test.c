/*
 * VirtIO-Net send/receive smoke test.
 *
 * Single-node mode (e.g. -netdev user): sends one ARP probe to the
 * slirp gateway; QEMU's user-mode networking responds. Confirms TX
 * and RX both work end-to-end.
 *
 * Two-node mode (-netdev socket between two QEMUs): each instance is
 * told its node id via the MAC last byte (01 or 02). Sends N
 * Hydra-tagged broadcast frames with the local node id; polls for
 * frames matching the OTHER node id. Both nodes will see the other's
 * frames over the host socket bridge.
 *
 * The Hydra tag is a custom EtherType 0x88B5 plus a 4-byte magic
 * 'HYDR' so frames are easy to spot in a pcap.
 */

#include "../kernel/net_driver.h"
#include "../kernel/printk.h"
#include "../kernel/bench.h"

#define HYDRA_ETHERTYPE  0x88b5  /* IEEE local-experimental ethertype */
#define HYDRA_TAG        0x48594452u  /* 'HYDR' */

static uint8_t tx_frame[64];

static int build_hydra_frame(uint8_t *out, const uint8_t *src_mac, uint8_t node_id, uint32_t seq) {
    int o = 0;
    /* dst MAC = broadcast */
    for (int i = 0; i < 6; i++) out[o++] = 0xff;
    /* src MAC */
    for (int i = 0; i < 6; i++) out[o++] = src_mac[i];
    /* ethertype */
    out[o++] = (HYDRA_ETHERTYPE >> 8) & 0xff;
    out[o++] = HYDRA_ETHERTYPE & 0xff;
    /* payload: 'HYDR' + node_id + seq + zero pad to min frame */
    out[o++] = 'H'; out[o++] = 'Y'; out[o++] = 'D'; out[o++] = 'R';
    out[o++] = node_id;
    out[o++] = (seq >> 24) & 0xff;
    out[o++] = (seq >> 16) & 0xff;
    out[o++] = (seq >>  8) & 0xff;
    out[o++] =  seq        & 0xff;
    while (o < 60) out[o++] = 0;  /* min Ethernet frame */
    return o;
}

void net_self_test(void) {
    uint8_t mac[6];
    net_get_mac_address(mac);
    uint8_t my_id = mac[5];  /* 0x01 or 0x02 for two-node tests; 0x56 in slirp mode */

    printk("[nettest] MAC %02x:%02x:%02x:%02x:%02x:%02x  node_id=0x%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], my_id);

    /* Send two passes of 20 sequence numbers. UDP multicast has no join
     * synchronization, so the faster QEMU can lose its first frame before
     * the peer attaches. Count unique peer sequences: the second pass repairs
     * startup loss without treating duplicates as additional success. */
    int peer_seen = 0;
    uint32_t peer_bitmap = 0;
    for (uint32_t round = 0; round < 40; round++) {
        uint32_t seq = round % 20;
        int n = build_hydra_frame(tx_frame, mac, my_id, seq);
        BENCH_TS("net_send_start");
        int rc = net_send(tx_frame, (uint16_t)n);
        BENCH_TS("net_send_done");
        if (rc < 0) {
            printk("[nettest] net_send rc=%d (seq=%d)\n", rc, seq);
        }

        /* Burn cycles and poll for incoming. */
        for (int t = 0; t < 200; t++) {
            for (volatile int i = 0; i < 5000; i++) {}
            struct net_packet *p = net_receive();
            if (!p) continue;
            /* Check whether it's a Hydra-tagged frame from the peer. */
            int looks_hydra =
                p->length >= 21 &&
                p->data[12] == ((HYDRA_ETHERTYPE >> 8) & 0xff) &&
                p->data[13] == (HYDRA_ETHERTYPE & 0xff) &&
                p->data[14] == 'H' && p->data[15] == 'Y' &&
                p->data[16] == 'D' && p->data[17] == 'R';
            if (looks_hydra) {
                uint8_t peer_id = p->data[18];
                uint32_t peer_seq =
                    ((uint32_t)p->data[19] << 24) | ((uint32_t)p->data[20] << 16) |
                    ((uint32_t)p->data[21] <<  8) | (uint32_t)p->data[22];
                if (peer_id != my_id && peer_seq < 20 &&
                    !(peer_bitmap & (1U << peer_seq))) {
                    peer_bitmap |= 1U << peer_seq;
                    printk("[nettest] RX hydra-frame from node 0x%02x seq=%d (%d bytes)\n",
                           peer_id, peer_seq, p->length);
                    peer_seen++;
                }
            } else {
                printk("[nettest] RX %d bytes from %02x:%02x:%02x:%02x:%02x:%02x ethertype=%02x%02x\n",
                       p->length,
                       p->data[6], p->data[7], p->data[8],
                       p->data[9], p->data[10], p->data[11],
                       p->data[12], p->data[13]);
            }
            net_free_packet(p);
        }
    }

    struct net_stats *st = net_get_stats();
    printk("[nettest] DONE  peer_frames_seen=%d  tx=%d/%dB  rx=%d/%dB\n",
           peer_seen, st->tx_packets, st->tx_bytes, st->rx_packets, st->rx_bytes);
}
