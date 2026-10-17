#include "../kernel/sched.h"
#include "../kernel/printk.h"
#include "../kernel/timer.h"

#define SCHED_STRESS_RUNS 32

static volatile unsigned int completed_tasks;
static volatile unsigned long non_yield_counter;
static volatile unsigned int register_test_done;
static volatile unsigned int register_test_pass;
static volatile unsigned long register_test_interrupts;

static void returning_task(void) {
    completed_tasks++;
    /* Returning exercises task_bootstrap's clean-exit path. */
}

static void non_yielding_task(void) {
    while (1) non_yield_counter++;
}

static void register_preservation_task(void) {
    unsigned long interrupts_before = get_timer_ticks();
    register unsigned long passed asm("a0");
    asm volatile(
        "li t3, 0x13579\n"
        "li a3, 0x2468a\n"
        "li s3, 0x55aa5\n"
        "li t5, 0x3ff0000000000000\n"
        "fmv.d.x fs3, t5\n"
        "rdtime t0\n"
        "1:\n"
        "rdtime t1\n"
        "sub t2, t1, t0\n"
        "li t4, 200000\n"       /* span at least two 100 Hz ticks */
        "bltu t2, t4, 1b\n"
        "li a0, 0\n"
        "li t4, 0x13579\n"
        "bne t3, t4, 2f\n"
        "li t4, 0x2468a\n"
        "bne a3, t4, 2f\n"
        "li t4, 0x55aa5\n"
        "bne s3, t4, 2f\n"
        "fmv.x.d t5, fs3\n"
        "li t4, 0x3ff0000000000000\n"
        "bne t5, t4, 2f\n"
        "li a0, 1\n"
        "2:\n"
        : "=r"(passed)
        :
        : "t0", "t1", "t2", "t3", "t4", "t5", "a3", "s3", "f11", "memory");
    register_test_pass = (unsigned int)passed;
    register_test_interrupts = get_timer_ticks() - interrupts_before;
    register_test_done = 1;
}

void scheduler_self_test(void) {
    printk("[schedtest] lifecycle stress starting (%d tasks)\n",
           SCHED_STRESS_RUNS);

    unsigned int baseline_active = get_active_task_count();
    completed_tasks = 0;

    for (int run = 0; run < SCHED_STRESS_RUNS; run++) {
        /* Establish the expectation before publishing a READY task. With
         * real preemption it may run and return inside create_task()'s final
         * interrupt-restoration window. */
        unsigned int deadline = completed_tasks + 1;
        int pid = create_task(returning_task);
        if (pid < 0) {
            printk("[schedtest] FAIL: create at run %d\n", run);
            return;
        }
        for (int spins = 0; completed_tasks < deadline && spins < 64; spins++) {
            task_yield();
        }
        if (completed_tasks < deadline) {
            printk("[schedtest] FAIL: task %d did not run/return\n", pid);
            return;
        }
    }

    if (get_active_task_count() != baseline_active || sched_validate() < 0 ||
        current_task != 0) {
        printk("[schedtest] FAIL: invariant/lifecycle mismatch active=%u baseline=%u current=%d\n",
               get_active_task_count(), baseline_active, current_task);
        return;
    }

    non_yield_counter = 0;
    int busy_pid = create_task(non_yielding_task);
    if (busy_pid < 0) {
        printk("[schedtest] FAIL: could not create non-yielding task\n");
        return;
    }
    task_yield();
    if (non_yield_counter == 0) {
        printk("[schedtest] FAIL: timer did not preempt busy task\n");
        return;
    }
    terminate_task((uint32_t)busy_pid);
    if (sched_validate() < 0) {
        printk("[schedtest] FAIL: invariant failure after preemption\n");
        return;
    }

    register_test_done = 0;
    register_test_pass = 0;
    register_test_interrupts = 0;
    unsigned long time_before;
    asm volatile("rdtime %0" : "=r"(time_before));
    int register_pid = create_task(register_preservation_task);
    if (register_pid < 0) {
        printk("[schedtest] FAIL: could not create register test task\n");
        return;
    }
    for (int spins = 0; !register_test_done && spins < 64; spins++) task_yield();
    unsigned long time_after;
    asm volatile("rdtime %0" : "=r"(time_after));
    /* Serviced ticks can lag elapsed time; use QEMU virt's 10 MHz timebase. */
    unsigned long elapsed_ms = (time_after - time_before) / 10000UL;
    if (!register_test_done || !register_test_pass || elapsed_ms < 20 ||
        register_test_interrupts == 0) {
        printk("[schedtest] FAIL: trap-frame preservation pass=%u elapsed=%u ms interrupts=%u\n",
               register_test_pass, (unsigned int)elapsed_ms,
               (unsigned int)register_test_interrupts);
        return;
    }

    printk("[schedtest] PASS: %u returns, busy-task preemption, integer+FP frame (%u ms, %u timer interrupts)\n",
           completed_tasks, (unsigned int)elapsed_ms,
           (unsigned int)register_test_interrupts);
}
