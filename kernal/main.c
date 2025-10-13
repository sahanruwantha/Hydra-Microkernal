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
#include <stddef.h>

void kmain(void) {
    uart_init();
    printk("\n==============================\n");
    printk(" Hydra Distributed Microkernel\n");
    printk(" Raytracer Node\n");
    printk("==============================\n\n");

    mm_init();
    ipc_init();
    sched_init();
    net_init();
    
    // Initialize distributed system - node ID based on MAC address or detection
    uint32_t node_id = 1; // Default node ID
    node_type_t node_type = NODE_TYPE_COMPUTE; // Default node type
    
    // Simple node detection approach - use a global counter
    // This will give different IDs to different QEMU instances
    static uint32_t global_node_counter = 0;
    global_node_counter++;
    
    // Use MAC address for more reliable node detection
    uint8_t mac_addr[6];
    extern void net_get_mac_address(uint8_t *buffer);
    net_get_mac_address(mac_addr);
    
    // Use last byte of MAC address to determine node ID
    if (mac_addr[5] == 0x01) {
        node_id = 1;
        node_type = NODE_TYPE_COMPUTE;
    } else if (mac_addr[5] == 0x02) {
        node_id = 2;
        node_type = NODE_TYPE_GRAPHICS;
    } else {
        // Fallback to sequential assignment for other cases
        node_id = (global_node_counter == 1) ? 1 : 2;
        node_type = (node_id == 1) ? NODE_TYPE_COMPUTE : NODE_TYPE_GRAPHICS;
    }
    
    printk("[main] Node ID: %u\n", node_id);
    
    char node_name[32];
    // Generate node name based on detected node ID
    const char *name_prefix = "hydra-node-";
    int i = 0;
    
    // Copy prefix
    while (name_prefix[i] && i < 30) {
        node_name[i] = name_prefix[i];
        i++;
    }
    
    // Add node ID digit
    if (i < 31) {
        node_name[i++] = '0' + (char)(node_id % 10);
    }
    node_name[i] = '\0';
    
    distributed_init(node_id, node_type, node_name);
    
    // Create server processes
    extern void bullet_server_main(void);
    extern void net_server_main(void);
    
    create_task(bullet_server_main);
    create_task(net_server_main);
    
    printk("[main] Ready. Commands: 'd'=status, 'm'=migrate, 'f'=find nodes\n");
    
    static bool raytracer_started = false;
    static int startup_counter = 0;
    
    // Main kernel loop
    while (1) {
        // Process distributed system updates
        distributed_tick();
        
        // Auto-start distributed raytracer after initial system stabilization
        if (!raytracer_started && ++startup_counter > 1000) {
            printk("[main] Auto-starting distributed raytracer demonstration...\n");
            extern void distributed_raytracer_main(void);
            create_task(distributed_raytracer_main);
            raytracer_started = true;
            
            // Send discovery requests to find other nodes
            send_discovery_request();
            printk("[main] Distributed raytracer started - looking for compute nodes...\n");
        }
        
        // Check if raytracer completed and show status
        if (raytracer_started && startup_counter % 5000 == 0) {
            extern bool is_raytracer_completed(void);
            if (is_raytracer_completed()) {
                printk("[main] Raytracer demonstration completed successfully!\n");
                printk("[main] Use 'd' to check status, 'm' to test migration, 'f' to find nodes\n");
            }
        }
        
        char c = uart_getc();
        if (c != 0) {  // Only process if we actually got a character
            uart_putc(c); // echo test
            
            // Handle distributed commands
            if (c == 'd') {
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
            } else if (c == 'f') {
                printk("\n[main] Broadcasting node discovery...\n");
                send_discovery_request();
            } else {
                printk("\n[main] Distributed commands: 'd'=status, 'm'=migrate raytracer, 'f'=find nodes\n");
            }
        }
        
        // Small delay to prevent busy waiting
        for (volatile int i = 0; i < 1000; i++);
        
        // Yield to allow task switching
        task_yield();
    }
}
