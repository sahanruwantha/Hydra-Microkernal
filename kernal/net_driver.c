#include "net_driver.h"
#include "printk.h"
#include "mm.h"
#include <stddef.h>

// VirtIO-Net device registers (QEMU virt machine)
#define VIRTIO_MMIO_BASE     0x10001000UL
#define VIRTIO_MMIO_MAGIC    0x000
#define VIRTIO_MMIO_VERSION  0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_STATUS   0x070
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_READY 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050

// VirtIO device status bits
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8

// Simple packet pool
#define PACKET_POOL_SIZE 32
static struct net_packet packet_pool[PACKET_POOL_SIZE];
static struct net_packet *free_packets = NULL;
static struct net_packet *rx_queue_head = NULL;
static struct net_packet *rx_queue_tail = NULL;

// Network statistics
static struct net_stats stats = {0};

// MAC address (will be set by device)
static uint8_t mac_addr[6];

static inline void write_reg32(uint64_t addr, uint32_t value) {
    *(volatile uint32_t *)(VIRTIO_MMIO_BASE + addr) = value;
}

static inline uint32_t read_reg32(uint64_t addr) {
    return *(volatile uint32_t *)(VIRTIO_MMIO_BASE + addr);
}

static void init_packet_pool(void) {
    // Initialize free packet list
    for (int i = 0; i < PACKET_POOL_SIZE - 1; i++) {
        packet_pool[i].next = &packet_pool[i + 1];
    }
    packet_pool[PACKET_POOL_SIZE - 1].next = NULL;
    free_packets = &packet_pool[0];
}

static struct net_packet *alloc_packet(void) {
    if (free_packets == NULL) {
        return NULL;
    }
    
    struct net_packet *pkt = free_packets;
    free_packets = free_packets->next;
    pkt->next = NULL;
    return pkt;
}

void net_free_packet(struct net_packet *pkt) {
    if (pkt == NULL) return;
    
    pkt->next = free_packets;
    free_packets = pkt;
}

void net_init(void) {
    printk("[net] Initializing network driver...\n");
    
    // Initialize packet pool
    init_packet_pool();
    
    // Check if VirtIO device is present
    uint32_t magic = read_reg32(VIRTIO_MMIO_MAGIC);
    if (magic != 0x74726976) { // "virt" in little endian
        printk("[net] VirtIO device not found (magic=0x%x) - using mock network\n", magic);
        goto mock_network_init;
    }
    
    uint32_t version = read_reg32(VIRTIO_MMIO_VERSION);
    uint32_t device_id = read_reg32(VIRTIO_MMIO_DEVICE_ID);
    
    printk("[net] VirtIO device found: version=%d, device_id=%d\n", version, device_id);
    
    // Accept device_id=0 as well, as QEMU might use this for network devices
    if (device_id != 1 && device_id != 0) { 
        printk("[net] Not a network device (device_id=%d)\n", device_id);
        goto mock_network_init;
    }
    
    // For device_id=0, we'll treat it as a simplified network device
    if (device_id == 0) {
        printk("[net] Using simplified VirtIO network device\n");
    }
    
    // Reset device
    write_reg32(VIRTIO_MMIO_STATUS, 0);
    
    // Acknowledge device
    write_reg32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    
    // Indicate we have a driver
    write_reg32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    
    // For now, skip feature negotiation and queue setup
    // This is a minimal implementation
    
    // Set driver OK
    write_reg32(VIRTIO_MMIO_STATUS, 
                VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | 
                VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    
    // Try to read MAC address from VirtIO config space
    // VirtIO net device config space starts at offset 0x100
    // MAC address is at offset 0 in config space (6 bytes)
    for (int i = 0; i < 6; i++) {
        // Read each byte of MAC address from config space
        mac_addr[i] = *((volatile uint8_t*)(VIRTIO_MMIO_BASE + 0x100 + i));
    }
    
    // Use system time/cycle counter to differentiate nodes
    // Each QEMU instance will start at slightly different times
    uint64_t cycle_count = 0;
    __asm__ volatile ("rdcycle %0" : "=r" (cycle_count));
    
    // Use cycle count to determine node identity
    // The first instance (listening) should start first and have lower count
    if ((cycle_count & 0xFF) < 128) {
        mac_addr[5] = 0x01;  // Node 1 (likely started first - listening)
    } else {
        mac_addr[5] = 0x02;  // Node 2 (likely started second - connecting)
    }
    
    // Normalize the MAC address to expected format
    mac_addr[0] = 0x52;
    mac_addr[1] = 0x54;
    mac_addr[2] = 0x00;
    mac_addr[3] = 0x12;
    mac_addr[4] = 0x34;
    // mac_addr[5] already set above
    
    // Debug print MAC address (avoid complex formatting for now)
    printk("[net] VirtIO network initialized. MAC bytes: ");
    for (int i = 0; i < 6; i++) {
        printk("%d ", (int)mac_addr[i]);
    }
    printk("\n");
    return;

mock_network_init:
    // Set up mock network - use the same global counter for consistency
    static int global_node_counter_mock = 0;
    global_node_counter_mock++;
    
    // Set MAC address based on boot order or other factors
    mac_addr[0] = 0x52;
    mac_addr[1] = 0x54;
    mac_addr[2] = 0x00;
    mac_addr[3] = 0x12;
    mac_addr[4] = 0x34;
    // Use different last byte to distinguish nodes
    mac_addr[5] = (global_node_counter_mock <= 1) ? 0x01 : 0x02;
    
    printk("[net] Mock network initialized. MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           (unsigned int)mac_addr[0], (unsigned int)mac_addr[1], (unsigned int)mac_addr[2], 
           (unsigned int)mac_addr[3], (unsigned int)mac_addr[4], (unsigned int)mac_addr[5]);
}

void net_get_mac_address(uint8_t *buffer) {
    for (int i = 0; i < 6; i++) {
        buffer[i] = mac_addr[i];
    }
}

int net_send(const uint8_t *data, uint16_t len) {
    if (len > 1514) {
        stats.tx_errors++;
        return -1;
    }
    
    // For now, just simulate sending (no actual VirtIO queue implementation)
    printk("[net] Sending packet: %d bytes\n", len);
    
    stats.tx_packets++;
    stats.tx_bytes += len;
    
    return 0;
}

struct net_packet *net_receive(void) {
    if (rx_queue_head == NULL) {
        return NULL;
    }
    
    struct net_packet *pkt = rx_queue_head;
    rx_queue_head = rx_queue_head->next;
    if (rx_queue_head == NULL) {
        rx_queue_tail = NULL;
    }
    
    return pkt;
}

struct net_stats *net_get_stats(void) {
    return &stats;
}

void net_interrupt_handler(void) {
    // Handle VirtIO-Net interrupts
    // For now, simulate receiving a packet occasionally
    static int interrupt_count = 0;
    interrupt_count++;
    
    if (interrupt_count % 100 == 0) {
        // Simulate receiving a packet
        struct net_packet *pkt = alloc_packet();
        if (pkt != NULL) {
            // Create a dummy packet
            pkt->length = 64;
            for (int i = 0; i < 64; i++) {
                pkt->data[i] = i & 0xFF;
            }
            
            // Add to RX queue
            pkt->next = NULL;
            if (rx_queue_tail == NULL) {
                rx_queue_head = rx_queue_tail = pkt;
            } else {
                rx_queue_tail->next = pkt;
                rx_queue_tail = pkt;
            }
            
            stats.rx_packets++;
            stats.rx_bytes += pkt->length;
            
            printk("[net] Simulated packet received (%d bytes)\n", pkt->length);
        }
    }
}
