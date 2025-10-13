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
#include "log.h"
#include <stddef.h>

void kmain(void) {
    uart_init();
    printk("\n==============================\n");
    printk(" Hydra Distributed Microkernel\n");
    printk(" Raytracer Node\n");
    printk("==============================\n\n");

    mm_init();
    
    // Initialize logging system early
    log_init();
    
    // Open system log files
    int kernel_log = log_open_file("kernel.log", LOG_INFO);
    int network_log = log_open_file("network.log", LOG_DEBUG);
    int ipc_log = log_open_file("ipc.log", LOG_INFO);
    int migration_log = log_open_file("migration.log", LOG_DEBUG);
    
    KLOG_INFO("Hydra Microkernel starting up...");
    KLOG_INFO("Memory manager initialized");
    
    ipc_init();
    KLOG_INFO("IPC system initialized");
    
    sched_init();
    KLOG_INFO("Scheduler initialized");
    printk("[main] Scheduler init completed, about to init timer...\n");
    
    // timer_init(); // Temporarily disabled - causes system crash
    KLOG_INFO("Timer initialization skipped for debugging");
    printk("[main] Timer init skipped, about to init network...\n");
    
    net_init();
    KLOG_INFO("Network driver initialized");
    printk("[main] Network init completed\n");
    
    // Note: Interrupts disabled due to trap vector instability
    // Using polling-based approach instead
    KLOG_INFO("Network polling mode enabled");
    
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
    KLOG_INFO("Detected Node ID: %u, Type: %s", node_id, 
              (node_type == NODE_TYPE_COMPUTE) ? "COMPUTE" : "GRAPHICS");
    
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
    
    printk("[main] About to initialize distributed system...\n");
    distributed_init(node_id, node_type, node_name);
    printk("[main] Distributed system initialized successfully\n");
    
    // Create server processes
    extern void bullet_server_main(void);
    extern void net_server_main(void);
    
    printk("[main] Creating bullet server task...\n");
    int bullet_task = create_task(bullet_server_main);
    printk("[main] Creating network server task...\n");
    int net_task = create_task(net_server_main);
    
    if (bullet_task == -1 || net_task == -1) {
        printk("[main] ERROR: Failed to create server tasks!\n");
        printk("[main] Available tasks: %d/%d\n", MAX_TASKS - num_tasks, MAX_TASKS);
    } else {
        printk("[main] Server tasks created successfully\n");
    }
    
    printk("[main] Ready. Commands: 'd'=status, 'm'=migrate, 'f'=find nodes\n");
    
    static bool raytracer_started = false;
    static int startup_counter = 0;
    
    printk("[main] Entering main loop...\n");
    
    while (1) {
        // Process distributed system updates
        distributed_tick();
        
        // Auto-start distributed raytracer after initial system stabilization
        if (!raytracer_started && ++startup_counter > 1000) {
            printk("[main] Auto-starting distributed raytracer...\n");
            extern void distributed_raytracer_main(void);
            int raytracer_task = create_task(distributed_raytracer_main);
            
            if (raytracer_task != -1) {
                raytracer_started = true;
                // Send discovery requests to find other nodes
                send_discovery_request();
                printk("[main] Distributed raytracer started - looking for compute nodes...\n");
            } else {
                printk("[main] ERROR: Failed to create raytracer task! Available: %d/%d\n", 
                       MAX_TASKS - num_tasks, MAX_TASKS);
                raytracer_started = true; // Prevent retry
            }
        }
        
        // Check if raytracer completed and show status
        if (raytracer_started && startup_counter % 5000 == 0) {
            extern bool is_raytracer_completed(void);
            if (is_raytracer_completed()) {
                printk("[main] Raytracer processing completed successfully!\n");
                printk("[main] Use 'd' to check status, 'm' to test migration, 'f' to find nodes\n");
            }
        }
        
        char c = uart_getc();
        if (c != 0) {
            uart_putc(c);
            
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
        
        for (volatile int i = 0; i < 10000; i++);
        
        task_yield();
    }
}
