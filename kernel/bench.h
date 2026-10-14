#ifndef BENCH_H
#define BENCH_H

#include "printk.h"

/*
 * Cycle-counter instrumentation for migration benchmarks.
 *
 * Reads RISC-V mcycle (CSR 0xB00). Hydra runs in machine mode on QEMU
 * virt, so mcycle is always accessible. We could use the unprivileged
 * rdcycle (CSR 0xC00) but on bare-metal M-mode both read the same
 * counter and mcycle is unambiguous.
 *
 * Each BENCH_TS(label) call emits one line over UART:
 *   BENCH <label> <cycle_count>
 *
 * The host-side benchmark harness greps for ^BENCH and parses the
 * label/cycle pairs. Keep labels short and unique within a migration:
 *
 *   BENCH_TS("ckpt_enter");   <- before suspending the task
 *   BENCH_TS("ckpt_freeze");  <- task is now suspended
 *   BENCH_TS("ckpt_copied");  <- state copied into user buffer
 *   BENCH_TS("send_start");   <- about to push onto the wire
 *   BENCH_TS("send_done");    <- wire transfer acknowledged
 *   BENCH_TS("recv_start");   <- destination first byte
 *   BENCH_TS("recv_done");    <- destination last byte
 *   BENCH_TS("rest_start");   <- about to call sys_restore
 *   BENCH_TS("rest_done");    <- restored task is READY
 *
 * Derived metrics (computed by analyze.py):
 *   freeze_time   = ckpt_freeze - ckpt_enter
 *   serialize_t   = ckpt_copied - ckpt_freeze
 *   total_latency = rest_done   - ckpt_enter
 *   transport_t   = recv_done   - send_start
 */

/*
 * Read the cycle counter. We use the unprivileged 'cycle' CSR (0xC00)
 * rather than the M-mode 'mcycle' (0xB00) because OpenSBI hands Hydra
 * off in S-mode. Reading mcycle from S-mode is an illegal instruction;
 * 'cycle' is readable from S/U when mcounteren bit 0 is set (which
 * OpenSBI does by default).
 */
static inline unsigned long bench_rdcycle(void) {
    unsigned long c;
    __asm__ volatile("csrr %0, cycle" : "=r"(c));
    return c;
}

#define BENCH_TS(label)                                          \
    do {                                                         \
        unsigned long _c = bench_rdcycle();                      \
        printk("BENCH %s %ld\n", (label), _c);                   \
    } while (0)

/*
 * BENCH_TS_TAGGED(label, run_id) is for sweeps where multiple
 * migrations run back-to-back. The host harness uses run_id to group
 * timestamps belonging to the same migration:
 *
 *   BENCH 42 ckpt_enter 1234567
 */
#define BENCH_TS_TAGGED(label, run_id)                           \
    do {                                                         \
        unsigned long _c = bench_rdcycle();                      \
        printk("BENCH %d %s %ld\n", (run_id), (label), _c);      \
    } while (0)

#endif /* BENCH_H */
