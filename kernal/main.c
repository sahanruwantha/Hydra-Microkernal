#include "uart.h"
#include "printk.h"
#include "mm.h"
#include "sched.h"
#include "ipc.h"
#include "net_driver.h"
#include "timer.h"
#include "syscall.h"
#include "distributed.h"
#include "process_migration.h"

void kmain(void) {
    uart_init();
    printk("\n==============================\n");
    printk(" Hydra Distributed Microkernel\n");
    printk(" Booting on QEMU virt...      \n");
    printk("==============================\n\n");

    printk("[main] Starting memory manager...\n");
    mm_init();
    
    printk("[main] Starting IPC system...\n");
    ipc_init();
    
    printk("[main] Starting scheduler...\n");
    sched_init();
    
    printk("[main] Starting timer...\n");
    // Timer requires interrupts - keep disabled until trap vectors work
    // timer_init();
    printk("[main] Timer disabled until trap vectors are stable\n");
    
    printk("[main] Starting network...\n");
    net_init();
    
    // Initialize distributed system - node ID based on MAC address or detection
    uint32_t node_id = 1; // Default node ID
    node_type_t node_type = NODE_TYPE_COMPUTE; // Default node type
    
    // In a real implementation, we could detect node ID from:
    // - MAC address (different for each QEMU instance)
    // - NVRAM settings
    // - Command line arguments
    // For now, we'll use a simple detection scheme
    
    // Try to detect if this is node 1 or 2 based on some system characteristic
    // This is a simplified approach for the demo
    
    char node_name[32];
    // Simple string copy for node name
    const char *name_template = "hydra-node-1";
    int i = 0;
    while (name_template[i] && i < 31) {
        node_name[i] = name_template[i];
        i++;
    }
    node_name[i] = '\0';
    
    printk("[main] Starting distributed system...\n");
    distributed_init(node_id, node_type, node_name);
    
    printk("[main] Starting server processes...\n");
    // Create server processes
    extern void bullet_server_main(void);
    extern void net_server_main(void);
    extern void test_client_main(void);
    extern void phase5_test(void);
    extern void phase6_demo(void);
    
    create_task(bullet_server_main);
    create_task(net_server_main);
    // Temporarily disable test client to prevent message flood
    // create_task(test_client_main);
    
    printk("[main] Running Phase 5 library tests...\n");
    phase5_test();
    
    printk("[main] Running Phase 6 unit tests...\n");
    extern void phase6_unit_tests(void);
    phase6_unit_tests();
    
    printk("[main] All subsystems initialized\n");
    
    // Enable interrupts only after everything is set up
    printk("[main] Enabling interrupts...\n");
    // Interrupts disabled until trap vectors work
    // enable_interrupts();
    printk("[main] Interrupts disabled until trap vectors are stable\n");
    
    printk("[main] Entering main loop...\n");
    printk("[main] Commands: 'n'=send, 's'=stats, 'r'=RX, 'b'=bullet test, 't'=net test, 'l'=lib test, '6'=Phase 6 apps\n");
    printk("[main] Distributed: 'd'=status, 'm'=migrate raytracer, 'h'=heartbeat, 'f'=find nodes\n");
    
    // Main kernel loop
    while (1) {
        // Process distributed system updates
        distributed_tick();
        
        char c = uart_getc();
        if (c != 0) {  // Only process if we actually got a character
            uart_putc(c); // echo test
            
            // Handle some simple commands
            if (c == 'n') {
                printk("\n[main] Testing network send...\n");
                uint8_t test_data[] = "Hello, Network!";
                net_send(test_data, sizeof(test_data) - 1);
            } else if (c == 's') {
                printk("\n[main] Network statistics:\n");
                struct net_stats *stats = net_get_stats();
                printk("  TX: %u packets, %u bytes\n", stats->tx_packets, stats->tx_bytes);
                printk("  RX: %u packets, %u bytes\n", stats->rx_packets, stats->rx_bytes);
            } else if (c == 'r') {
                printk("\n[main] Checking for received packets...\n");
                struct net_packet *pkt = net_receive();
                if (pkt) {
                    printk("  Received packet: %d bytes\n", pkt->length);
                    net_free_packet(pkt);
                } else {
                    printk("  No packets available\n");
                }
            } else if (c == 'b') {
                printk("\n[main] Testing bullet server...\n");
                extern long handle_syscall(long, long, long, long, long);
                char test_data[] = "Migration test data";
                long result = handle_syscall(1, 1, (long)test_data, sizeof(test_data), 0); // simulate migration request
                printk("  Bullet server response: %ld\n", result);
            } else if (c == 't') {
                printk("\n[main] Testing network server...\n");
                extern long handle_syscall(long, long, long, long, long);
                long sock = handle_syscall(1, 1, 1, 0, 0); // create TCP socket
                printk("  Network server socket: %ld\n", sock);
            } else if (c == 'l') {
                printk("\n[main] Running Phase 5 library tests...\n");
                extern void phase5_test(void);
                phase5_test();
            } else if (c == '6') {
                printk("\n[main] Running Phase 6 applications demo...\n");
                extern void phase6_demo(void);
                phase6_demo();
            } else if (c == 'd') {
                printk("\n[main] Distributed system status:\n");
                print_distributed_status();
            } else if (c == 'm') {
                printk("\n[main] Attempting to migrate raytracer process...\n");
                // Find a suitable target node
                kernel_node_t *target = distributed_find_best_node(NODE_TYPE_GRAPHICS);
                if (!target) {
                    target = distributed_find_best_node(NODE_TYPE_COMPUTE);
                }
                
                if (target) {
                    printk("  Target node: %u (%s)\n", target->node_id, target->name);
                    bool success = migrate_process_to_node(1, target->node_id); // Migrate process ID 1 (raytracer)
                    printk("  Migration %s\n", success ? "initiated" : "failed");
                } else {
                    printk("  No suitable target node found\n");
                }
            } else if (c == 'h') {
                printk("\n[main] Sending heartbeat to all nodes...\n");
                send_heartbeat();
            } else if (c == 'f') {
                printk("\n[main] Broadcasting node discovery...\n");
                send_discovery_request();
            } else {
                printk("\n[main] Commands: 'n'=send, 's'=stats, 'r'=RX, 'b'=bullet test, 't'=net test, 'l'=lib test, '6'=Phase 6 apps\n");
                printk("[main] Distributed: 'd'=status, 'm'=migrate raytracer, 'h'=heartbeat, 'f'=find nodes\n");
                printk("[main] Received char: %c, Timer ticks: %lu\n", c, get_timer_ticks());
            }
        }
        
        // Small delay to prevent busy waiting
        for (volatile int i = 0; i < 1000; i++);
        
        // Yield to allow task switching
        task_yield();
    }
}
