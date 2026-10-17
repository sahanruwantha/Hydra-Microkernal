#include "uart.h"
#include "printk.h"
#include "mm.h"
#include "vm.h"
#include "sched.h"
#include "ipc.h"
#include "net_driver.h"
#include "timer.h"

void kmain(void) {
    uart_init();
    printk("\n==============================\n");
    printk(" Hydra Process Migration\n");
#if defined(MIGRATION_KERNEL_RESIDENT)
    printk(" Migration mode: KERNEL-RESIDENT\n");
#elif defined(MIGRATION_USERSPACE)
    printk(" Migration mode: USER-SPACE\n");
#else
    printk(" Migration mode: UNCONFIGURED\n");
#endif
    printk(" Both comparison paths run in supervisor mode.\n");
    printk("==============================\n\n");

    mm_init();
    if (vm_init() < 0 || vm_validate() < 0) {
        printk("[main] FATAL: Sv39 initialization failed\n");
        while (1) asm volatile("wfi");
    }
    ipc_init();
    sched_init();
    timer_init();
    net_init();

    extern void net_self_test(void);
    net_self_test();

    uint8_t mac[6];
    net_get_mac_address(mac);
    if (mac[5] == 0x01 || mac[5] == 0x02) {
        extern void cross_node_migration(void);
        cross_node_migration();
    }

    enable_interrupts();

    extern void scheduler_self_test(void);
    scheduler_self_test();

    extern void isolation_self_test(void);
    isolation_self_test();

#ifdef MICRO_BENCH
    extern void micro_bench_run(void);
    micro_bench_run();
#endif

    const unsigned int auto_runs = 60;
    unsigned int migtest_runs = 0;
    printk("[main] Ready. Commands: 'c'=checkpoint/restore, 'r'=fault recovery, 'd'=status\n");

    while (1) {
        if (migtest_runs < auto_runs) {
            extern void migration_self_test(void);
            migration_self_test();
            migtest_runs++;
            if (migtest_runs == auto_runs) {
                printk("[main] BENCH_DONE auto_runs=%u\n", migtest_runs);
                extern void fault_tolerance_demo(void);
                fault_tolerance_demo();
            }
        }

        char c = uart_getc();
        if (c != 0) {
            uart_putc(c);
            if (c == 'c') {
                extern void migration_self_test(void);
                migration_self_test();
            } else if (c == 'r') {
                extern void fault_tolerance_demo(void);
                fault_tolerance_demo();
            } else if (c == 'd') {
                struct net_stats *stats = net_get_stats();
                printk("\n[main] Status: auto_runs=%u/%u active_tasks=%u\n",
                       migtest_runs, auto_runs, get_active_task_count());
                printk("[main] Network: tx=%u rx=%u tx_errors=%u rx_errors=%u\n",
                       stats->tx_packets, stats->rx_packets,
                       stats->tx_errors, stats->rx_errors);
            } else {
                printk("\n[main] Commands: 'c'=checkpoint/restore, 'r'=fault recovery, 'd'=status\n");
            }
        }

        if (migtest_runs < auto_runs) {
            for (volatile int i = 0; i < 10000; i++);
        } else {
            asm volatile("wfi");
        }
        task_yield();
    }
}
