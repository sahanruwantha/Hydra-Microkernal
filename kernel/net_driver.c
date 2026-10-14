/*
 * Hydra VirtIO-Net 1.1 driver — modern MMIO transport (version 2).
 *
 * QEMU must be invoked with -global virtio-mmio.force-legacy=false so
 * the device exposes the modern transport. The make targets in
 * Makefile already pass this flag.
 *
 * Init dance (modern):
 *   RESET → ACK → DRIVER → feature negotiation → FEATURES_OK → check →
 *   per-queue setup (desc/driver/device addresses + QueueReady=1) →
 *   DRIVER_OK.
 *
 * We must negotiate VIRTIO_F_VERSION_1 (bit 32). VIRTIO_NET_F_MAC (bit 5)
 * is also accepted if offered. Everything else is declined.
 *
 * Hydra has no MMU, so the "physical" address we hand to the device
 * is the same as the kernel virtual address. We avoid kmalloc and use
 * pre-aligned BSS arrays.
 *
 * Polling only — no interrupts. net_send pushes a TX descriptor and
 * kicks the device; net_receive polls the RX used ring.
 */

#include "net_driver.h"
#include "printk.h"
#include "irq.h"
#include <stddef.h>

typedef unsigned long size_t;

/* --- Modern VirtIO MMIO register offsets --- */
#define VMMIO_MAGIC               0x000
#define VMMIO_VERSION             0x004
#define VMMIO_DEVICE_ID           0x008
#define VMMIO_VENDOR_ID           0x00c
#define VMMIO_DEVICE_FEATURES     0x010
#define VMMIO_DEVICE_FEATURES_SEL 0x014
#define VMMIO_DRIVER_FEATURES     0x020
#define VMMIO_DRIVER_FEATURES_SEL 0x024
#define VMMIO_QUEUE_SEL           0x030
#define VMMIO_QUEUE_NUM_MAX       0x034
#define VMMIO_QUEUE_NUM           0x038
#define VMMIO_QUEUE_READY         0x044
#define VMMIO_QUEUE_NOTIFY        0x050
#define VMMIO_INTERRUPT_STATUS    0x060
#define VMMIO_INTERRUPT_ACK       0x064
#define VMMIO_STATUS              0x070
#define VMMIO_QUEUE_DESC_LOW      0x080
#define VMMIO_QUEUE_DESC_HIGH     0x084
#define VMMIO_QUEUE_DRIVER_LOW    0x090
#define VMMIO_QUEUE_DRIVER_HIGH   0x094
#define VMMIO_QUEUE_DEVICE_LOW    0x0a0
#define VMMIO_QUEUE_DEVICE_HIGH   0x0a4
#define VMMIO_CONFIG_GENERATION   0x0fc
#define VMMIO_CONFIG              0x100

#define VIRTIO_MAGIC_VALUE        0x74726976u  /* 'virt' little-endian */

#define V_STATUS_RESET        0
#define V_STATUS_ACK          1
#define V_STATUS_DRIVER       2
#define V_STATUS_DRIVER_OK    4
#define V_STATUS_FEATURES_OK  8
#define V_STATUS_FAILED       128

#define VIRTIO_NET_F_MAC      (1ULL << 5)
#define VIRTIO_F_VERSION_1    (1ULL << 32)

#define PAGE_SIZE 4096

struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;  /* present when VIRTIO_F_VERSION_1 negotiated */
} __attribute__((packed));
#define NETHDR_SIZE 12

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

#define QUEUE_RX 0
#define QUEUE_TX 1
#define QUEUE_SIZE 16
#define MAX_BUF 1600

struct virtq {
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    uint8_t            *buffers;
    uint16_t            last_used_idx;
};

static volatile uint64_t mmio_base = 0;
static struct virtq      rx_vq;
static struct virtq      tx_vq;
static uint8_t           mac_addr[6];
static struct net_stats  stats = {0};

/* Page-aligned ring memory. Each component (desc/avail/used) is a
 * separate static array — modern MMIO accepts independent addresses
 * for each, so no contiguous-layout constraint. */
static struct vring_desc  rx_desc[QUEUE_SIZE]              __attribute__((aligned(16)));
static struct vring_desc  tx_desc[QUEUE_SIZE]              __attribute__((aligned(16)));
static uint8_t            rx_avail_mem[6 + 2 * QUEUE_SIZE] __attribute__((aligned(16)));
static uint8_t            rx_used_mem [6 + 8 * QUEUE_SIZE] __attribute__((aligned(16)));
static uint8_t            tx_avail_mem[6 + 2 * QUEUE_SIZE] __attribute__((aligned(16)));
static uint8_t            tx_used_mem [6 + 8 * QUEUE_SIZE] __attribute__((aligned(16)));
static uint8_t            rx_buffers[QUEUE_SIZE * MAX_BUF] __attribute__((aligned(16)));
static uint8_t            tx_buffers[QUEUE_SIZE * MAX_BUF] __attribute__((aligned(16)));

#define PACKET_POOL_SIZE 64
static struct net_packet packet_pool[PACKET_POOL_SIZE];
static struct net_packet *free_packets;
static struct net_packet *rx_q_head;
static struct net_packet *rx_q_tail;

static inline void wreg(uint32_t off, uint32_t val) {
    *(volatile uint32_t *)(mmio_base + off) = val;
}
static inline uint32_t rreg(uint32_t off) {
    return *(volatile uint32_t *)(mmio_base + off);
}
static inline void mb(void) {
    __asm__ volatile("fence rw,rw" ::: "memory");
}
static void zmem(void *p, size_t n) {
    unsigned char *q = p; while (n--) *q++ = 0;
}
static void cmem(void *d, const void *s, size_t n) {
    unsigned char *a = d; const unsigned char *b = s;
    while (n--) *a++ = *b++;
}

static void init_packet_pool(void) {
    for (int i = 0; i < PACKET_POOL_SIZE - 1; i++) {
        packet_pool[i].next = &packet_pool[i + 1];
    }
    packet_pool[PACKET_POOL_SIZE - 1].next = NULL;
    free_packets = &packet_pool[0];
    rx_q_head = rx_q_tail = NULL;
}

static struct net_packet *alloc_packet(void) {
    struct net_packet *p = free_packets;
    if (p) { free_packets = p->next; p->next = NULL; }
    return p;
}

static void net_free_packet_locked(struct net_packet *p) {
    if (!p) return;
    p->next = free_packets;
    free_packets = p;
}

void net_free_packet(struct net_packet *p) {
    irq_state_t irq = irq_save();
    net_free_packet_locked(p);
    irq_restore(irq);
}

/* Scan QEMU virt MMIO slots looking for VirtIO device-id 1 (network). */
static uint64_t scan_virtio_net(void) {
    for (int slot = 0; slot < 8; slot++) {
        uint64_t base = 0x10001000UL + slot * 0x1000UL;
        uint32_t magic = *(volatile uint32_t *)(base + VMMIO_MAGIC);
        if (magic != VIRTIO_MAGIC_VALUE) continue;
        uint32_t did = *(volatile uint32_t *)(base + VMMIO_DEVICE_ID);
        if (did == 1) {
            uint32_t ver = *(volatile uint32_t *)(base + VMMIO_VERSION);
            printk("[net] VirtIO-Net found at 0x%lx (mmio version %d)\n", base, ver);
            return base;
        }
    }
    return 0;
}

static int setup_queue_modern(int qidx, struct virtq *vq,
                              struct vring_desc *desc,
                              uint8_t *avail_mem, uint8_t *used_mem,
                              uint8_t *buffers)
{
    wreg(VMMIO_QUEUE_SEL, qidx);
    if (rreg(VMMIO_QUEUE_READY) != 0) {
        printk("[net] queue %d already ready\n", qidx);
        return -1;
    }
    uint32_t max = rreg(VMMIO_QUEUE_NUM_MAX);
    if (max == 0 || QUEUE_SIZE > max) {
        printk("[net] queue %d max=%d, want %d — bad\n", qidx, max, QUEUE_SIZE);
        return -1;
    }
    wreg(VMMIO_QUEUE_NUM, QUEUE_SIZE);

    vq->desc    = desc;
    vq->avail   = (struct vring_avail *)avail_mem;
    vq->used    = (struct vring_used  *)used_mem;
    vq->buffers = buffers;
    vq->last_used_idx = 0;

    zmem(desc,      sizeof(struct vring_desc) * QUEUE_SIZE);
    zmem(avail_mem, 6 + 2 * QUEUE_SIZE);
    zmem(used_mem,  6 + 8 * QUEUE_SIZE);
    zmem(buffers,   QUEUE_SIZE * MAX_BUF);

    uint64_t desc_pa  = (uint64_t)desc;
    uint64_t avail_pa = (uint64_t)avail_mem;
    uint64_t used_pa  = (uint64_t)used_mem;

    wreg(VMMIO_QUEUE_DESC_LOW,    (uint32_t)(desc_pa  & 0xffffffff));
    wreg(VMMIO_QUEUE_DESC_HIGH,   (uint32_t)(desc_pa  >> 32));
    wreg(VMMIO_QUEUE_DRIVER_LOW,  (uint32_t)(avail_pa & 0xffffffff));
    wreg(VMMIO_QUEUE_DRIVER_HIGH, (uint32_t)(avail_pa >> 32));
    wreg(VMMIO_QUEUE_DEVICE_LOW,  (uint32_t)(used_pa  & 0xffffffff));
    wreg(VMMIO_QUEUE_DEVICE_HIGH, (uint32_t)(used_pa  >> 32));

    wreg(VMMIO_QUEUE_READY, 1);
    return 0;
}

static void prime_rx_queue(void) {
    for (int i = 0; i < QUEUE_SIZE; i++) {
        rx_vq.desc[i].addr  = (uint64_t)&rx_vq.buffers[i * MAX_BUF];
        rx_vq.desc[i].len   = MAX_BUF;
        rx_vq.desc[i].flags = VRING_DESC_F_WRITE;
        rx_vq.desc[i].next  = 0;
        rx_vq.avail->ring[i] = i;
    }
    mb();
    rx_vq.avail->idx = QUEUE_SIZE;
    mb();
    wreg(VMMIO_QUEUE_NOTIFY, QUEUE_RX);
}

static int virtio_net_init(uint64_t base) {
    mmio_base = base;
    uint32_t ver = rreg(VMMIO_VERSION);
    if (ver != 2) {
        printk("[net] need modern MMIO (v2); device reports v%d. "
               "Use -global virtio-mmio.force-legacy=false in QEMU.\n", ver);
        return -1;
    }

    wreg(VMMIO_STATUS, V_STATUS_RESET);
    wreg(VMMIO_STATUS, V_STATUS_ACK);
    wreg(VMMIO_STATUS, V_STATUS_ACK | V_STATUS_DRIVER);

    /* 64-bit feature negotiation. */
    wreg(VMMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t flo = rreg(VMMIO_DEVICE_FEATURES);
    wreg(VMMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t fhi = rreg(VMMIO_DEVICE_FEATURES);
    uint64_t device_feats = ((uint64_t)fhi << 32) | flo;
    printk("[net] device features = 0x%lx\n", device_feats);

    uint64_t want = VIRTIO_F_VERSION_1;
    if (device_feats & VIRTIO_NET_F_MAC) want |= VIRTIO_NET_F_MAC;

    wreg(VMMIO_DRIVER_FEATURES_SEL, 0);
    wreg(VMMIO_DRIVER_FEATURES, (uint32_t)(want & 0xffffffff));
    wreg(VMMIO_DRIVER_FEATURES_SEL, 1);
    wreg(VMMIO_DRIVER_FEATURES, (uint32_t)(want >> 32));

    wreg(VMMIO_STATUS, V_STATUS_ACK | V_STATUS_DRIVER | V_STATUS_FEATURES_OK);
    uint32_t st = rreg(VMMIO_STATUS);
    if (!(st & V_STATUS_FEATURES_OK)) {
        printk("[net] device rejected feature set, status=0x%x\n", st);
        return -1;
    }

    if (setup_queue_modern(QUEUE_RX, &rx_vq, rx_desc, rx_avail_mem, rx_used_mem, rx_buffers) < 0) return -1;
    if (setup_queue_modern(QUEUE_TX, &tx_vq, tx_desc, tx_avail_mem, tx_used_mem, tx_buffers) < 0) return -1;

    if (want & VIRTIO_NET_F_MAC) {
        for (int i = 0; i < 6; i++) {
            mac_addr[i] = *(volatile uint8_t *)(mmio_base + VMMIO_CONFIG + i);
        }
    } else {
        mac_addr[0]=0x52; mac_addr[1]=0x54; mac_addr[2]=0x00;
        mac_addr[3]=0x12; mac_addr[4]=0x34; mac_addr[5]=0x01;
    }

    wreg(VMMIO_STATUS, V_STATUS_ACK | V_STATUS_DRIVER | V_STATUS_FEATURES_OK | V_STATUS_DRIVER_OK);
    prime_rx_queue();
    return 0;
}

static void mock_init(void) {
    static int boot_counter = 0;
    boot_counter++;
    mac_addr[0]=0x52; mac_addr[1]=0x54; mac_addr[2]=0x00;
    mac_addr[3]=0x12; mac_addr[4]=0x34;
    mac_addr[5] = (boot_counter <= 1) ? 0x01 : 0x02;
    printk("[net] mock network. MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5]);
}

void net_init(void) {
    printk("[net] Initializing network driver...\n");
    init_packet_pool();

    uint64_t base = scan_virtio_net();
    if (base == 0) {
        printk("[net] No VirtIO-Net device found in MMIO slots\n");
        mock_init();
        return;
    }
    if (virtio_net_init(base) < 0) {
        printk("[net] VirtIO-Net init failed; falling back to mock\n");
        mmio_base = 0;
        mock_init();
        return;
    }
    printk("[net] VirtIO-Net ready. MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5]);
}

void net_get_mac_address(uint8_t *buffer) {
    for (int i = 0; i < 6; i++) buffer[i] = mac_addr[i];
}

static int next_tx_slot = 0;
static uint8_t tx_inflight[QUEUE_SIZE];

static void poll_tx_completions(void) {
    if (mmio_base == 0) return;
    mb();
    while (tx_vq.last_used_idx != tx_vq.used->idx) {
        uint16_t used_slot = tx_vq.last_used_idx % QUEUE_SIZE;
        uint32_t desc_id = tx_vq.used->ring[used_slot].id;
        if (desc_id < QUEUE_SIZE) {
            tx_inflight[desc_id] = 0;
        } else {
            stats.tx_errors++;
        }
        tx_vq.last_used_idx++;
        mb();
    }
}

static int net_send_locked(const uint8_t *data, uint16_t len) {
    if (len == 0 || len > 1514) {
        stats.tx_errors++;
        return -1;
    }
    if (mmio_base == 0) {
        stats.tx_packets++;
        stats.tx_bytes += len;
        return 0;
    }

    int slot = next_tx_slot;
    next_tx_slot = (next_tx_slot + 1) % QUEUE_SIZE;

    /* Descriptors may only be reused after the device returns them on the
     * used ring. The old driver wrapped every eight sends regardless of
     * completion, which could overwrite an in-flight Ethernet frame. */
    poll_tx_completions();
    for (unsigned int spin = 0; tx_inflight[slot] && spin < 1000000; spin++) {
        poll_tx_completions();
    }
    if (tx_inflight[slot]) {
        stats.tx_errors++;
        return -1;
    }

    uint8_t *buf = &tx_vq.buffers[slot * MAX_BUF];
    struct virtio_net_hdr *hdr = (struct virtio_net_hdr *)buf;
    zmem(hdr, NETHDR_SIZE);
    cmem(buf + NETHDR_SIZE, data, len);

    tx_vq.desc[slot].addr  = (uint64_t)buf;
    tx_vq.desc[slot].len   = NETHDR_SIZE + len;
    tx_vq.desc[slot].flags = 0;
    tx_vq.desc[slot].next  = 0;
    tx_inflight[slot] = 1;

    uint16_t ai = tx_vq.avail->idx % QUEUE_SIZE;
    tx_vq.avail->ring[ai] = slot;
    mb();
    tx_vq.avail->idx++;
    mb();

    wreg(VMMIO_QUEUE_NOTIFY, QUEUE_TX);

    stats.tx_packets++;
    stats.tx_bytes += len;
    return 0;
}

int net_send(const uint8_t *data, uint16_t len) {
    irq_state_t irq = irq_save();
    int result = net_send_locked(data, len);
    irq_restore(irq);
    return result;
}

static void poll_rx(void) {
    if (mmio_base == 0) return;

    int requeued = 0;
    mb();
    while (rx_vq.last_used_idx != rx_vq.used->idx) {
        uint16_t slot_idx = rx_vq.last_used_idx % QUEUE_SIZE;
        struct vring_used_elem *e = &rx_vq.used->ring[slot_idx];
        uint32_t desc_idx = e->id;
        uint32_t total_len = e->len;

        if (desc_idx >= QUEUE_SIZE) {
            stats.rx_errors++;
            rx_vq.last_used_idx++;
            mb();
            continue;
        }

        if (total_len > NETHDR_SIZE && total_len <= MAX_BUF) {
            uint8_t *src = &rx_vq.buffers[desc_idx * MAX_BUF] + NETHDR_SIZE;
            uint32_t pkt_len = total_len - NETHDR_SIZE;
            if (pkt_len > sizeof(((struct net_packet *)0)->data)) {
                pkt_len = sizeof(((struct net_packet *)0)->data);
            }
            struct net_packet *p = alloc_packet();
            if (p) {
                cmem(p->data, src, pkt_len);
                p->length = (uint16_t)pkt_len;
                p->next = NULL;
                if (rx_q_tail) rx_q_tail->next = p;
                else            rx_q_head = p;
                rx_q_tail = p;
                stats.rx_packets++;
                stats.rx_bytes += pkt_len;
            }
        }

        rx_vq.desc[desc_idx].addr  = (uint64_t)&rx_vq.buffers[desc_idx * MAX_BUF];
        rx_vq.desc[desc_idx].len   = MAX_BUF;
        rx_vq.desc[desc_idx].flags = VRING_DESC_F_WRITE;
        rx_vq.desc[desc_idx].next  = 0;
        uint16_t ai = rx_vq.avail->idx % QUEUE_SIZE;
        rx_vq.avail->ring[ai] = desc_idx;
        mb();
        rx_vq.avail->idx++;
        rx_vq.last_used_idx++;
        requeued++;
        mb();
    }
    if (requeued) {
        mb();
        wreg(VMMIO_QUEUE_NOTIFY, QUEUE_RX);
    }
}

static struct net_packet *net_receive_locked(void) {
    poll_rx();
    if (!rx_q_head) return NULL;
    struct net_packet *p = rx_q_head;
    rx_q_head = p->next;
    if (!rx_q_head) rx_q_tail = NULL;
    p->next = NULL;
    return p;
}

struct net_packet *net_receive(void) {
    irq_state_t irq = irq_save();
    struct net_packet *packet = net_receive_locked();
    irq_restore(irq);
    return packet;
}

struct net_stats *net_get_stats(void) { return &stats; }

void net_interrupt_handler(void) {
    if (mmio_base == 0) return;
    uint32_t st = rreg(VMMIO_INTERRUPT_STATUS);
    wreg(VMMIO_INTERRUPT_ACK, st);
}
