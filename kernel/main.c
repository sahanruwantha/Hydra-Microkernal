#include "uart.h"
#include "printk.h"
#include "mm.h"
#include "vm.h"
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
#if defined(MIGRATION_KERNEL_RESIDENT)
    printk(" Migration mode: KERNEL-RESIDENT\n");
#elif defined(MIGRATION_USERSPACE)
    printk(" Migration mode: USER-SPACE\n");
#else
    printk(" Migration mode: UNCONFIGURED\n");
#endif
    printk("==============================\n\n");

    mm_init();
    if (vm_init() < 0 || vm_validate() < 0) {
        printk("[main] FATAL: Sv39 initialization failed\n");
        while (1) asm volatile("wfi");
    }
    
    // Initialize logging system early
    log_init();
    
    // Open system log files
    int kernel_log = log_open_file("kernel.log", LOG_INFO);
    int network_log = log_open_file("network.log", LOG_DEBUG);
    int ipc_log = log_open_file("ipc.log", LOG_INFO);
    int migration_log = log_open_file("migration.log", LOG_DEBUG);
    (void)kernel_log;
    (void)network_log;
    (void)ipc_log;
    (void)migration_log;
    
    KLOG_INFO("Hydra Microkernel starting up...");
    KLOG_INFO("Memory manager initialized");
    
    ipc_init();
    KLOG_INFO("IPC system initialized");
    
    sched_init();
    KLOG_INFO("Scheduler initialized");
    printk("[main] Scheduler init completed, about to init timer...\n");
    
    /*
     * Install the complete supervisor trap frame. Timer scheduling becomes
     * active in enable_interrupts(); synchronous yield traps work immediately.
     */
    timer_init();
    KLOG_INFO("Timer initialized (full trap frame, preemption pending)");
    printk("[main] Timer init done, about to init network...\n");
    
    net_init();
    KLOG_INFO("Network driver initialized");
    printk("[main] Network init completed\n");

    /* Quick VirtIO-Net send/recv smoke test. Emits BENCH net_send_*
     * timestamps; QEMU's -object filter-dump captures the wire bytes. */
    extern void net_self_test(void);
    net_self_test();

    /* If we booted with a two-node MAC (01 or 02 in the last byte),
     * run the cross-node migration demonstration. */
    {
        uint8_t mac[6];
        net_get_mac_address(mac);
        if (mac[5] == 0x01 || mac[5] == 0x02) {
            extern void cross_node_migration(void);
            cross_node_migration();
        }
    }
    
    /*
     * Enable interrupts now that the trap vector is installed and the
     * subsystems are up. Timer interrupts drive round-robin preemption;
     * network I/O remains polling-based.
     */
    enable_interrupts();
    KLOG_INFO("Timer preemption enabled; network remains polling-based");
    
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

    /* Verify that kmain owns its reserved scheduler slot and that tasks can
     * run, return, terminate, and reuse slots without corrupting it. */
    extern void scheduler_self_test(void);
    scheduler_self_test();

    extern void isolation_self_test(void);
    isolation_self_test();

#ifdef MICRO_BENCH
    /* Null-syscall / IPC round-trip micro-benchmarks for the Amoeba
     * ratio comparison (Chapter 5). Off by default: it creates extra
     * tasks, so leaving it out keeps the migration sweep's numbers
     * comparable with previously published runs.
     * Build with: make MICRO_BENCH=1 */
    extern void micro_bench_run(void);
    micro_bench_run();
#endif

    printk("[main] Ready. Commands: 'd'=status, 'f'=find nodes,\n");
    printk("       'c'=REAL checkpoint+restore self-test, 'r'=fault recovery demo,\n");
    printk("       'm'=migration request handshake (transfers no process state)\n");
    
    static bool raytracer_started = false;
    static bool migtest_done = false;
    /* Declared at loop scope: the idle check below needs it too. */
    extern bool is_raytracer_completed(void);
    static int  migtest_runs = 0;
    static int  startup_counter = 0;

    /* Number of automatic migration self-test runs. Each run emits the
     * full set of BENCH timestamps that analyze.py groups by
     * ckpt_enter. With 4 payload sizes cycled in rotation, 60 runs
     * gives 15 samples per size — enough for a usable mean ± 95% CI.
     * Set to 0 to disable auto-run (manual 'c' only). */
    const int MIGTEST_AUTO_RUNS = 60;

    printk("[main] Entering main loop...\n");
    
    while (1) {
        startup_counter++;  /* always advance so post-raytracer triggers fire */
        // Process distributed system updates
        distributed_tick();

        // Auto-start distributed raytracer after initial system stabilization
        if (!raytracer_started && startup_counter > 1000) {
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

        /*
         * Auto-run the migration self-test after the kernel has
         * settled. We trigger here rather than from interactive UART
         * input because Hydra's scheduler starves the main task under
         * a busy net-server load (pre-existing scheduler fairness
         * issue), making stdin-driven triggers unreliable.
         */
        if (!migtest_done && startup_counter > 5000 && migtest_runs < MIGTEST_AUTO_RUNS) {
            extern void migration_self_test(void);
            migration_self_test();
            migtest_runs++;
            if (migtest_runs >= MIGTEST_AUTO_RUNS) {
                migtest_done = true;
                printk("[main] BENCH_DONE auto_runs=%d\n", migtest_runs);
                /* Run the fault-tolerance demo once the benchmark
                 * sweep is finished — gives the §7.3 killer-app
                 * evidence the dissertation needs. */
                extern void fault_tolerance_demo(void);
                fault_tolerance_demo();
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
                /*
                 * Migration-request HANDSHAKE only. This exercises the
                 * inherited distributed.c protocol layer: it picks a peer
                 * node and sends MSG_MIGRATION_REQUEST. It does NOT capture
                 * or transfer any process state — migrate_process_to_node()
                 * leaves data_size = 0 and process_data = NULL, and no task
                 * is frozen, copied or restored.
                 *
                 * The real mechanism is sys_checkpoint/sys_restore in
                 * kernel/migration.c: press 'c' for the local round trip,
                 * or run bench/two_node_test.sh for the cross-node path.
                 * Earlier builds printed "migrating raytracer process" here,
                 * which was misleading on two counts — nothing is migrated,
                 * and the hardcoded pid is not the raytracer.
                 */
                printk("\n[main] Sending migration REQUEST to a peer node.\n");
                printk("       (handshake only - no process state is captured;\n");
                printk("        press 'c' for a real checkpoint/restore)\n");
                // Find a suitable target node
                kernel_node_t *target = distributed_find_best_node(NODE_TYPE_GRAPHICS);
                if (!target) {
                    target = distributed_find_best_node(NODE_TYPE_COMPUTE);
                }

                if (target) {
                    printk("  Target node: %u (%s)\n", target->node_id, target->name);
                    bool success = migrate_process_to_node(1, target->node_id);
                    printk("  Migration request %s\n",
                           success ? "sent (no state transferred)" : "failed");
                } else {
                    printk("  No suitable target node found\n");
                }
            } else if (c == 'f') {
                printk("\n[main] Broadcasting node discovery...\n");
                send_discovery_request();
            } else if (c == 'c') {
                extern void migration_self_test(void);
                migration_self_test();
            } else if (c == 'r') {
                extern void fault_tolerance_demo(void);
                fault_tolerance_demo();
            } else {
                printk("\n[main] Commands: 'd'=status, 'f'=find nodes,"
                       " 'm'=migration request (handshake only),\n"
                       "       'c'=REAL checkpoint+restore, 'r'=fault recovery demo\n");
            }
        }
        
        /*
         * Idle policy.
         *
         * While there is still work to do — the raytracer running, or the
         * migration benchmark sweep in progress — spin exactly as before
         * so throughput and BENCH timings are unaffected.
         *
         * Once the sweep is done the node has nothing left but to poll
         * for network traffic and UART input. Spinning there held a full
         * host core at 100% indefinitely. wfi parks the hart until the
         * next timer interrupt (1kHz), which drops idle CPU to near zero
         * while keeping worst-case poll latency at 1ms.
         */
        bool work_pending = !migtest_done ||
                            (raytracer_started && !is_raytracer_completed());

        if (work_pending) {
            for (volatile int i = 0; i < 10000; i++);
        } else {
            asm volatile("wfi");
        }

        task_yield();
    }
}
