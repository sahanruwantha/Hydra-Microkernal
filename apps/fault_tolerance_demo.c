/*
 * Local fault-tolerance demonstration.
 *
 * Demonstrates that the checkpoint/restore primitives we built for
 * migration also serve as a fault-recovery mechanism — one of the
 * "killer applications" that Milojicic et al. (1999, §7.3) identified
 * as missing when they wrote their survey.
 *
 * Demo flow:
 *   1. Spawn a counter task.
 *   2. Let it run for a while; observe counter.
 *   3. Take a checkpoint to a persistent buffer.
 *   4. Let it run further; counter advances past the checkpoint value.
 *   5. Simulate a fault: force-terminate the task slot.
 *   6. "Detect" the failure (we just notice the task is gone).
 *   7. Restore from the saved checkpoint.
 *   8. Verify the restored task is running and counter > checkpoint value.
 *
 * The same checkpoint blob that drives migration drives recovery here.
 * That is the structural argument the dissertation makes: a single
 * minimal kernel primitive subsumes both migration and fault recovery.
 */

#include "../kernel/migration.h"
#include "../kernel/sched.h"
#include "../kernel/printk.h"
#include "../kernel/syscall.h"
#include "../kernel/bench.h"

/* Persistent checkpoint buffer — survives the "failure" because it
 * lives in BSS, not in the killed task's memory. */
static unsigned char fault_demo_buf[16 * 1024] __attribute__((aligned(16)));
static long          fault_demo_size = 0;

volatile static int fdemo_counter = 0;

static void fdemo_task(void) {
    while (1) {
        fdemo_counter++;
        for (volatile int i = 0; i < 50000; i++) { /* burn cycles */ }
        task_yield();
    }
}

void fault_tolerance_demo(void) {
    printk("\n[fdemo] === Local fault-tolerance demonstration ===\n");
    printk("[fdemo] Premise: checkpoint+restore = fault recovery (Milojicic 1999 §7.3 killer app)\n");

    /* Disable extra payload for this demo to keep it fast. */
    unsigned long saved_extra = migration_extra_bytes;
    migration_extra_bytes = 0;

    /* 1. Spawn the workload task. */
    fdemo_counter = 0;
    int pid = create_task(fdemo_task);
    if (pid < 0) {
        printk("[fdemo] FAIL: could not create workload task\n");
        migration_extra_bytes = saved_extra;
        return;
    }
    printk("[fdemo] Step 1: workload task pid=%d spawned\n", pid);

    /* 2. Let it run. */
    for (int i = 0; i < 30; i++) task_yield();
    int counter_at_t1 = fdemo_counter;
    printk("[fdemo] Step 2: workload ran, counter=%d\n", counter_at_t1);

    /* 3. Checkpoint. */
    BENCH_TS("fdemo_ckpt_start");
    fault_demo_size = syscall_checkpoint(pid, fault_demo_buf, sizeof(fault_demo_buf));
    BENCH_TS("fdemo_ckpt_done");
    if (fault_demo_size < 0) {
        printk("[fdemo] FAIL: checkpoint failed (%ld)\n", fault_demo_size);
        migration_extra_bytes = saved_extra;
        return;
    }
    printk("[fdemo] Step 3: checkpoint saved (%ld bytes) at counter=%d\n",
           fault_demo_size, counter_at_t1);

    /* Resume the task (checkpoint left it BLOCKED). */
    resume_task((uint32_t)pid);

    /* 4. Let it run further. */
    for (int i = 0; i < 30; i++) task_yield();
    int counter_at_t2 = fdemo_counter;
    printk("[fdemo] Step 4: workload progressed past checkpoint, counter=%d\n", counter_at_t2);

    /* 5. Simulate a fault: kill the task slot. The counter will stop
     * advancing because no task is incrementing it any more. */
    printk("[fdemo] Step 5: SIMULATING FAULT — killing task %d\n", pid);
    BENCH_TS("fdemo_fault_inject");
    terminate_task((uint32_t)pid);

    /* 6. Yield to give the scheduler a chance; counter should not move. */
    for (int i = 0; i < 30; i++) task_yield();
    int counter_during_fault = fdemo_counter;
    printk("[fdemo] Step 6: detected failure (counter frozen at %d)\n", counter_during_fault);

    /* 7. Restore from the persistent checkpoint. */
    BENCH_TS("fdemo_recovery_start");
    long restored_pid = syscall_restore(fault_demo_buf, fault_demo_size);
    BENCH_TS("fdemo_recovery_done");
    if (restored_pid < 0) {
        printk("[fdemo] FAIL: recovery restore failed (%ld)\n", restored_pid);
        migration_extra_bytes = saved_extra;
        return;
    }
    printk("[fdemo] Step 7: restored from checkpoint as pid=%ld\n", restored_pid);

    /* 8. Verify the restored task runs and the counter advances. */
    for (int i = 0; i < 30; i++) task_yield();
    int counter_after_recovery = fdemo_counter;
    printk("[fdemo] Step 8: post-recovery counter=%d (delta from fault=%d)\n",
           counter_after_recovery, counter_after_recovery - counter_during_fault);

    if (counter_after_recovery > counter_during_fault) {
        printk("[fdemo] PASS: workload recovered from simulated fault\n");
    } else {
        printk("[fdemo] WARN: workload did not advance after recovery\n");
    }

    /* Cleanup. */
    terminate_task((uint32_t)restored_pid);
    migration_extra_bytes = saved_extra;

    printk("[fdemo] === Demonstration complete ===\n\n");
}
