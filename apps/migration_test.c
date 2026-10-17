/*
 * Local migration round-trip self-test.
 *
 * Demonstrates that sys_checkpoint and sys_restore actually work, with
 * a single QEMU instance acting as both source and destination. The
 * "destination" is just a different task slot in the same kernel.
 *
 * Triggered from main.c by pressing 'c' on the UART. Emits BENCH lines
 * that the host-side analyze.py script consumes.
 */

#include "../kernel/migration.h"
#include "../kernel/sched.h"
#include "../kernel/printk.h"
#include "../kernel/syscall.h"
#include "../kernel/bench.h"

/*
 * Buffer big enough for one checkpoint with the largest payload sweep
 * we use: header + context + 8 KB stack + up to 64 KB extra. Bumped
 * to 96 KB for headroom.
 */
static unsigned char migration_buf[96 * 1024] __attribute__((aligned(16)));

/* Payload sizes to sweep over (extra bytes beyond the 8 KB task stack).
 * 0 = minimum, then geometric up to 64 KB so we get a useful curve. */
static const unsigned long SWEEP_SIZES[] = { 0, 4096, 16384, 65536 };
#define SWEEP_N (sizeof(SWEEP_SIZES) / sizeof(SWEEP_SIZES[0]))

/* A tiny victim task: increments a global counter forever. Useful as a
 * checkpoint subject because its state is well-defined. */
volatile static int victim_counter = 0;

static void victim_task(void) {
    while (1) {
        victim_counter++;
        for (volatile int i = 0; i < 50000; i++) { /* burn cycles */ }
        task_yield();
    }
}

/*
 * Run one full local checkpoint/restore cycle.
 *
 * 1. Spawn a victim task.
 * 2. Yield to let it run and context-switch out at least once.
 * 3. syscall_checkpoint(victim_pid).
 * 4. syscall_restore(buffer) -> new_pid.
 * 5. Yield to let new_pid run; observe counter increments.
 */
void migration_self_test(void) {
    /* Each call picks one payload size from SWEEP_SIZES in rotation so
     * a long run produces a balanced sample across sizes. */
    static unsigned int sweep_idx = 0;
    unsigned long sz = SWEEP_SIZES[sweep_idx % SWEEP_N];
    sweep_idx++;
    migration_extra_bytes = sz;

    /* Emit a tag line so analyze.py can group BENCH lines by size. */
    printk("BENCH_SIZE %ld\n", sz);

    printk("\n[migtest] Local migration self-test starting (extra=%ld)\n", sz);
    printk("[migtest] checkpoint record size = %ld bytes\n",
           (unsigned long)checkpoint_size_for_task());

    /* 1) Spawn the victim. */
    int victim_pid = create_task(victim_task);
    if (victim_pid < 0) {
        printk("[migtest] FAIL: could not create victim task\n");
        return;
    }
    printk("[migtest] victim task pid=%d created\n", victim_pid);

    /* 2) Yield so the victim runs and its resumable trap frame is saved. */
    int counter_before;
    for (int i = 0; i < 20; i++) {
        task_yield();
    }
    counter_before = victim_counter;
    printk("[migtest] victim ran, counter=%d before checkpoint\n", counter_before);

    /* 3) Checkpoint. Note: BENCH ckpt_enter/ckpt_freeze/ckpt_copied
     * are emitted from inside migration_checkpoint().
     *
     * In KERNEL mode we call migration_checkpoint() directly: the
     * "migration mechanism" is part of the kernel, the way Mach's
     * DIPC/DMM modules were.
     *
     * In USERSPACE mode we go through syscall_checkpoint, modelling
     * a user-space migration server that goes through the kernel
     * boundary to access the primitive.
     */
#if defined(MIGRATION_KERNEL_RESIDENT)
    long n = migration_checkpoint((uint32_t)victim_pid,
                                  migration_buf, sizeof(migration_buf));
#else
    long n = syscall_checkpoint(victim_pid, migration_buf, sizeof(migration_buf));
#endif
    if (n < 0) {
        printk("[migtest] FAIL: checkpoint returned %ld\n", n);
        return;
    }
    printk("[migtest] checkpoint OK, %ld bytes written\n", n);

    /* (Network transfer would happen here. For the local self-test we
     * just hand the same buffer to sys_restore.) */
    BENCH_TS("send_start");
    BENCH_TS("send_done");
    BENCH_TS("recv_start");
    BENCH_TS("recv_done");

    /* 4) Restore. BENCH rest_start/rest_done emitted inside the call.
     * Same kernel-vs-userspace distinction as for checkpoint. */
#if defined(MIGRATION_KERNEL_RESIDENT)
    long new_pid = migration_restore(migration_buf, n);
#else
    long new_pid = syscall_restore(migration_buf, n);
#endif
    if (new_pid < 0) {
        printk("[migtest] FAIL: restore returned %ld\n", new_pid);
        return;
    }
    printk("[migtest] restore OK, new pid=%ld\n", new_pid);

    /* 5) Let the restored task run; it should pick up where the victim
     * left off and keep incrementing victim_counter. */
    for (int i = 0; i < 20; i++) {
        task_yield();
    }
    int counter_after = victim_counter;
    printk("[migtest] after restore, counter=%d (delta=%d)\n",
           counter_after, counter_after - counter_before);

    if (counter_after > counter_before) {
        printk("[migtest] PASS: restored task is running\n");
    } else {
        printk("[migtest] WARN: counter did not advance after restore\n");
    }

    /* PIDs are opaque; scheduler helpers resolve them to reusable slots. */
    terminate_task((uint32_t)victim_pid);
    terminate_task((uint32_t)new_pid);

    /* Run a one-time negative test for hostile/truncated checkpoint
     * metadata. Restore must reject the record before allocating a task. */
    static int validation_tested = 0;
    if (!validation_tested) {
        struct checkpoint_header *hdr =
            (struct checkpoint_header *)migration_buf;
        uint32_t saved_total = hdr->total_size;
        hdr->total_size = (uint32_t)sizeof(*hdr);
        long invalid_result = migration_restore(migration_buf, (unsigned long)n);
        hdr->total_size = saved_total;
        printk("[migtest] %s: malformed checkpoint rejected\n",
               invalid_result < 0 ? "PASS" : "FAIL");
        validation_tested = 1;
    }

    printk("[migtest] Local migration self-test complete\n\n");
}
