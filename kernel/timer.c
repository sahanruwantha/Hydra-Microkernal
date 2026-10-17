#include "timer.h"
#include "printk.h"
#include "sched.h"
#include "uart.h"
#include "syscall.h"

/*
 * PRIVILEGE MODE NOTE
 *
 * OpenSBI boots this kernel in S-mode ("Domain0 Next Mode: S-mode"), not
 * M-mode. The original version of this file used M-mode CSRs (mie,
 * mstatus, mtvec, mepc) and wrote CLINT mtimecmp directly. Every one of
 * those is an illegal instruction from S-mode; with no stvec installed
 * the fault escalated to firmware and the board reset, which is the
 * "causes system crash" that made timer_init() get commented out.
 *
 * The S-mode equivalents are used instead:
 *   mie/mstatus -> sie/sstatus      mtvec -> stvec      mret -> sret
 *   CLINT mtimecmp -> stimecmp (Sstc), mtime -> rdtime
 *
 * QEMU's virt board reports "sstc" in the boot HART ISA extensions, so
 * stimecmp is written directly rather than going through an SBI ecall.
 */

// RISC-V trap causes (S-mode)
#define CAUSE_TIMER_INTERRUPT 0x8000000000000005UL  /* supervisor timer */
#define CAUSE_ECALL_FROM_S    0x0000000000000009UL  /* ecall from S-mode */
#define CAUSE_BREAKPOINT      0x0000000000000003UL  /* S-mode yield gate */
#define CAUSE_ECALL_FROM_U    0x0000000000000008UL  /* U-mode syscall */
#define CAUSE_INST_PAGE_FAULT 0x000000000000000cUL
#define CAUSE_LOAD_PAGE_FAULT 0x000000000000000dUL
#define CAUSE_STORE_PAGE_FAULT 0x000000000000000fUL
#define CAUSE_UART_INTERRUPT  0x8000000000000009UL  /* supervisor external */

#define SIE_STIE     (1UL << 5)   /* sie.STIE  - supervisor timer enable */
#define SSTATUS_SIE  (1UL << 1)   /* sstatus.SIE - global S-mode enable */
#define CSR_STIMECMP 0x14d        /* Sstc; assembler may not know the name */

/* Current time. rdtime is delegated to the same counter CLINT drives. */
static inline unsigned long read_time(void) {
    unsigned long t;
    asm volatile("rdtime %0" : "=r"(t));
    return t;
}

/* Arm the next supervisor timer interrupt. */
static inline void write_stimecmp(unsigned long value) {
    asm volatile("csrw %0, %1" : : "i"(CSR_STIMECMP), "r"(value));
}

/*
 * Timer period in mtime ticks. QEMU virt drives mtime at 10MHz, so
 * 100000 = 10ms = 100Hz.
 *
 * The timer exists to give wfi something to wake on: the idle path in
 * kmain() parks the hart until the next interrupt instead of spinning.
 * The tick rate therefore trades idle host CPU against poll latency.
 * Measured on this workload, node idle after the benchmark sweep:
 *
 *     no timer (spin)  ~102% CPU     -
 *     1kHz  (1ms)       ~57% CPU     1ms  worst-case poll latency
 *     100Hz (10ms)       ~9% CPU     10ms worst-case poll latency
 *
 * 100Hz is the default: 10ms is far inside the tolerance of anything
 * here (heartbeats are 5s, the migration timeout 30s, and inbound frames
 * buffer in the virtio ring meanwhile). Override with
 * -DTIMER_INTERVAL=<ticks> if you need tighter latency.
 */
#ifndef TIMER_INTERVAL
#define TIMER_INTERVAL 100000
#endif

static unsigned long timer_ticks = 0;

// Install the S-mode trap vector and enable the supervisor timer
void timer_init(void) {
    /* Install stvec first: once sie.STIE is set, a pending timer
     * interrupt must have somewhere to land. */
    set_trap_vector();

    // Arm the first interrupt well ahead so nothing fires mid-init
    write_stimecmp(read_time() + (TIMER_INTERVAL * 10));

    // Enable the supervisor timer interrupt in sie
    unsigned long sie;
    asm volatile("csrr %0, sie" : "=r"(sie));
    sie |= SIE_STIE;
    asm volatile("csrw sie, %0" : : "r"(sie));

    printk("[timer] S-mode timer initialized (stimecmp, %d tick interval)\n",
           TIMER_INTERVAL);
}

// Function to enable interrupts after full system initialization
void enable_interrupts(void) {
    // Set proper timer compare value
    write_stimecmp(read_time() + TIMER_INTERVAL);

    sched_enable_preemption();

    // Enable global interrupts in sstatus (SIE, bit 1)
    asm volatile("csrs sstatus, %0" : : "r"(SSTATUS_SIE));

    printk("[timer] Interrupts enabled\n");
}

struct trap_frame *trap_handler(unsigned long cause, unsigned long epc,
                                unsigned long tval, struct trap_frame *frame) {
    static int trap_count = 0;
    static int in_trap = 0;
    
    if (in_trap) {
        printk("[trap] ERROR: Recursive trap detected!\n");
        asm volatile("csrci sstatus, 0x2"); // Clear sstatus.SIE
        while(1); // Halt
    }
    
    in_trap = 1;
    trap_count++;
    
    if (cause == CAUSE_TIMER_INTERRUPT) {
        // Handle timer interrupt
        timer_ticks++;
        
        // Re-arm; stimecmp is a compare value, not an interval, so it
        // must be pushed forward or the interrupt refires immediately.
        write_stimecmp(read_time() + TIMER_INTERVAL);
        
        // Limit debug output to prevent spam. Gated: at 1kHz an
        // every-10-ticks printk would emit 100 lines/s on its own.
        if (timer_ticks % 1000 == 0) {
            trace_sched("[trap] Timer tick %lu\n", timer_ticks);
        }

        // Network activity now handled by polling in net_receive()

        /* Choose the complete task frame that trap_entry restores with sret. */
        in_trap = 0;
        return sched_from_trap(frame, 1);
    }
    else if (cause == CAUSE_BREAKPOINT) {
        frame->sepc = epc + 4;
        if (frame->a7 == 6) {
            in_trap = 0;
            return sched_from_trap(frame, 0);
        }
        printk("[trap] Unknown S-mode yield gate %lu at 0x%lx\n", frame->a7, epc);
    }
    else if (cause == CAUSE_ECALL_FROM_U) {
        frame->sepc = epc + 4;
        long number = (long)frame->a7;
        if (number == SYS_YIELD) {
            frame->a0 = SYSCALL_OK;
            in_trap = 0;
            return sched_from_trap(frame, 0);
        }
        if (number == SYS_EXIT) {
            uint32_t pid = (uint32_t)task_current_pid();
            sched_record_exit(pid, (long)frame->a0);
            terminate_task(pid);
            in_trap = 0;
            return sched_from_trap(frame, 0);
        }
        frame->a0 = (unsigned long)handle_user_syscall(frame);
    }
    else if ((cause == CAUSE_INST_PAGE_FAULT ||
              cause == CAUSE_LOAD_PAGE_FAULT ||
              cause == CAUSE_STORE_PAGE_FAULT) &&
             current_task > 0 && current_task < num_tasks &&
             tasks[current_task].user_mode && !(frame->sstatus & (1UL << 8))) {
        uint32_t pid = (uint32_t)task_current_pid();
        printk("[trap] Contained U-mode page fault: pid=%u cause=%ld address=0x%lx\n",
               pid, cause, tval);
        sched_record_exit(pid, -(long)cause);
        terminate_task(pid);
        in_trap = 0;
        return sched_from_trap(frame, 0);
    }
    else if (cause == CAUSE_ECALL_FROM_S) {
        printk("[trap] Unexpected supervisor ecall at 0x%lx\n", epc);
    }
    else if ((cause & 0x8000000000000000UL) && ((cause & 0xFF) == 9)) {
        // Handle external interrupt (UART)
        uart_interrupt_handler();
    }
    else {
        // Handle other traps
        printk("[trap] Unhandled trap #%d: cause=0x%lx, epc=0x%lx, tval=0x%lx\n", 
               trap_count, cause, epc, tval);
        
        // If we get too many unknown traps, something is wrong
        if (trap_count > 10) {
            printk("[trap] Too many unknown traps, disabling interrupts\n");
            asm volatile("csrci sstatus, 0x2"); // Clear sstatus.SIE
        }
    }
    
    in_trap = 0;
    return frame;
}

unsigned long get_timer_ticks(void) {
    return timer_ticks;
}
