/*
 * micro_bench.c - null-syscall and IPC round-trip micro-benchmarks.
 *
 * Purpose: produce the two numbers needed for a ratio-normalised
 * comparison against Amoeba (Chapter 5). Absolute cycle counts under
 * QEMU are not comparable across ISAs, but the RATIO
 *
 *     IPC round trip / null syscall
 *
 * largely cancels emulation and ISA differences, because both paths
 * pay the same per-trap cost on the same emulated machine. Amoeba's
 * own test suite ships the matching pair:
 *
 *     test/performance/syscall/sys_null.c   -> null syscall
 *     test/performance/rpc/rpc_client.c     -> RPC round trip
 *
 * so the same ratio can be computed for real Amoeba and set beside
 * Hydra's.
 *
 * IMPORTANT (methodology): these benchmarks deliberately run in U-mode
 * and issue real `ecall` instructions. The wrappers in
 * lib/libhydra/syscall_user.c are direct kernel function calls, NOT
 * privilege transitions -- timing those would measure a C call and
 * would not be comparable to Amoeba's sys_null(). Everything here goes
 * through CAUSE_ECALL_FROM_U -> handle_user_syscall, the same path a
 * real user task takes.
 *
 * Each benchmark reports through its U-mode exit code, which the
 * supervisor-side driver reads with sched_last_exit_code() and prints
 * as BENCH lines for bench/analyze_micro.py.
 */

#include "../kernal/sched.h"
#include "../kernal/printk.h"
#include "../kernal/ipc.h"

/* Iteration counts. Kept modest so a run completes inside the QEMU
 * timeout, but large enough that the cycle counter's resolution and
 * the one-off trap-cache warmup are negligible. */
#define NULL_SYSCALL_ITERS  20000
#define IPC_ITERS            2000

/* Shared with the supervisor-side sink task. */
static volatile uint32_t bench_client_pid;
static volatile unsigned int bench_sink_replies;

/*
 * Supervisor-side echo server. Stands in for an Amoeba server process
 * answering an RPC: it blocks for a request and sends a reply.
 */
static void bench_echo_sink(void) {
    for (unsigned int i = 0; i < IPC_ITERS; i++) {
        uint32_t value = 0;
        while (recv_message((int)bench_client_pid, &value, sizeof(value)) < 0)
            task_yield();
        uint32_t reply = value + 1;
        while (send_message((int)bench_client_pid, &reply, sizeof(reply)) < 0)
            task_yield();
        bench_sink_replies++;
    }
}

/*
 * Null syscall: NULL_SYSCALL_ITERS x SYS_GET_PID from U-mode.
 *
 * SYS_GET_PID is Hydra's cheapest syscall -- it reads the current task
 * slot and returns, doing no copyin/copyout and no scheduling. That
 * makes it the closest analogue to Amoeba's sys_null(), which likewise
 * traps and returns immediately.
 *
 * Exits with the total cycle count for the whole loop.
 */
__attribute__((section(".user_text"), noinline, noreturn, used))
static void user_null_syscall_bench(void) {
    asm volatile(
        "li   s2, %[iters]\n"
        "csrr s3, cycle\n"          /* start */
        "1:\n"
        "li   a7, 5\n"              /* SYS_GET_PID */
        "ecall\n"
        "addi s2, s2, -1\n"
        "bnez s2, 1b\n"
        "csrr s4, cycle\n"          /* end */
        "sub  a0, s4, s3\n"         /* exit code = total cycles */
        "li   a7, 8\n"              /* SYS_EXIT */
        "ecall\n"
        "2: j 2b\n"
        :
        : [iters] "i" (NULL_SYSCALL_ITERS)
        : "a0", "a7", "s2", "s3", "s4", "memory");
    __builtin_unreachable();
}

/*
 * IPC round trip: IPC_ITERS x (SYS_SEND_MSG + SYS_RECV_MSG) from
 * U-mode to a supervisor echo server and back. This is Hydra's
 * equivalent of a local Amoeba RPC: two privilege transitions, a
 * copyin, a copyout, and the scheduling needed to run the server.
 *
 * a0 on entry carries the sink PID. Exits with the total cycle count.
 */
__attribute__((section(".user_text"), noinline, noreturn, used))
static void user_ipc_roundtrip_bench(void) {
    asm volatile(
        "mv   s1, a0\n"             /* sink pid */
        "addi s5, sp, -16\n"        /* private user-stack scratch buffer */
        "li   s2, %[iters]\n"
        "csrr s3, cycle\n"          /* start */
        "1:\n"
        "li   t1, 0x51CA11ED\n"
        "sw   t1, 0(s5)\n"
        "mv   a0, s1\n"
        "mv   a1, s5\n"
        "li   a2, 4\n"
        "li   a7, 1\n"              /* SYS_SEND_MSG (copyin) */
        "ecall\n"
        "bnez a0, 3f\n"             /* send failed -> bail out */
        "2:\n"
        "mv   a0, s1\n"
        "mv   a1, s5\n"
        "li   a2, 4\n"
        "li   a7, 2\n"              /* SYS_RECV_MSG (copyout) */
        "ecall\n"
        "bgtz a0, 4f\n"             /* got the reply */
        "li   a7, 6\n"              /* SYS_YIELD, then retry recv */
        "ecall\n"
        "j    2b\n"
        "4:\n"
        "addi s2, s2, -1\n"
        "bnez s2, 1b\n"
        "csrr s4, cycle\n"          /* end */
        "sub  a0, s4, s3\n"         /* exit code = total cycles */
        "li   a7, 8\n"
        "ecall\n"
        "3:\n"
        "li   a0, -1\n"             /* signal failure to the driver */
        "li   a7, 8\n"
        "ecall\n"
        "5: j 5b\n"
        :
        : [iters] "i" (IPC_ITERS)
        : "a0", "a1", "a2", "a7", "t1",
          "s1", "s2", "s3", "s4", "s5", "memory");
    __builtin_unreachable();
}

/* Spin the scheduler until <pid> exits; returns its exit code, or
 * LONG_MIN-ish sentinel -1 if it never showed up. */
static long await_exit(int pid, int max_spins) {
    uint32_t exited_pid = 0;
    for (int spins = 0; spins < max_spins; spins++) {
        task_yield();
        (void)sched_last_exit_code(&exited_pid);
        if (exited_pid == (uint32_t)pid)
            return sched_last_exit_code(&exited_pid);
    }
    return -1;
}

/*
 * Driver. Emits:
 *   BENCH_MICRO null_syscall <iters> <total_cycles>
 *   BENCH_MICRO ipc_roundtrip <iters> <total_cycles>
 * and a derived ratio line for convenience. bench/analyze_micro.py
 * parses these.
 */
void micro_bench_run(void) {
    printk("[micro] null-syscall / IPC micro-benchmark starting\n");

    /* --- null syscall ------------------------------------------- */
    int pid = create_user_task(user_null_syscall_bench);
    if (pid < 0) {
        printk("[micro] FAIL: could not create null-syscall task\n");
        return;
    }
    long null_cycles = await_exit(pid, 4096);
    if (null_cycles <= 0) {
        printk("[micro] FAIL: null-syscall task returned %d\n",
               (int)null_cycles);
        return;
    }
    printk("BENCH_MICRO null_syscall %d %ld\n",
           NULL_SYSCALL_ITERS, null_cycles);

    /* --- IPC round trip ------------------------------------------ */
    bench_sink_replies = 0;
    bench_client_pid = 0;
    int sink_pid = create_task(bench_echo_sink);
    if (sink_pid < 0) {
        printk("[micro] FAIL: could not create echo sink\n");
        return;
    }
    int client_pid = create_user_task_arg(user_ipc_roundtrip_bench,
                                          (unsigned long)sink_pid);
    if (client_pid < 0) {
        printk("[micro] FAIL: could not create IPC client\n");
        return;
    }
    bench_client_pid = (uint32_t)client_pid;

    long ipc_cycles = await_exit(client_pid, 200000);
    if (ipc_cycles <= 0) {
        printk("[micro] FAIL: IPC task returned %d (replies=%u)\n",
               (int)ipc_cycles, bench_sink_replies);
        return;
    }
    printk("BENCH_MICRO ipc_roundtrip %d %ld\n", IPC_ITERS, ipc_cycles);

    /* Per-operation means and the ratio that Chapter 5 compares
     * against Amoeba. Integer arithmetic only -- no soft-float in the
     * kernel -- so the ratio is reported scaled by 100. */
    unsigned long null_per = (unsigned long)null_cycles / NULL_SYSCALL_ITERS;
    unsigned long ipc_per  = (unsigned long)ipc_cycles  / IPC_ITERS;
    printk("BENCH_MICRO null_syscall_per_op %ld\n", null_per);
    printk("BENCH_MICRO ipc_roundtrip_per_op %ld\n", ipc_per);
    if (null_per)
        printk("BENCH_MICRO ratio_x100 %ld\n", (ipc_per * 100UL) / null_per);
    printk("[micro] BENCH_MICRO_DONE\n");
}
