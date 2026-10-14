#include "sched.h"
#include "printk.h"
#include "mm.h"
#include "ipc.h"
#include "irq.h"
#include "vm.h"

// Basic type definitions
typedef unsigned long size_t;
#define NULL ((void*)0)

// Simple string functions for kernel use
static void *k_memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = c;
    return s;
}

struct task tasks[MAX_TASKS];
int current_task = 0;
int num_tasks = 0;
static uint64_t tick_counter = 0;
static uint32_t next_pid = 1;
static int preemption_enabled = 0;
static uint32_t last_exit_pid;
static long last_exit_code;

#define SSTATUS_SPP  (1UL << 8)
#define SSTATUS_SPIE (1UL << 5)
#define SSTATUS_SIE  (1UL << 1)

static void task_bootstrap(void);

static void init_task_trap(struct task *task, void (*entry)(void)) {
    k_memset(&task->trap, 0, sizeof(task->trap));
    task->trap.sepc = (unsigned long)task_bootstrap;
    task->trap.sp = (unsigned long)&task->stack[TASK_STACK_SIZE];
    asm volatile("mv %0, gp" : "=r"(task->trap.gp));
    asm volatile("mv %0, tp" : "=r"(task->trap.tp));
    unsigned long status;
    asm volatile("csrr %0, sstatus" : "=r"(status));
    status |= SSTATUS_SPP;
    if (preemption_enabled || (status & SSTATUS_SIE)) status |= SSTATUS_SPIE;
    else status &= ~SSTATUS_SPIE;
    status &= ~SSTATUS_SIE;
    task->trap.sstatus = status;
    task->entry = entry;
    task->satp = vm_kernel_satp();
    task->user_mode = 0;
    task->user_data_pages = NULL;
    task->user_data_page_count = 0;
    task->user_stack_pages = NULL;
    task->user_stack_page_count = 0;
    task->trap_valid = 1;
}

static void task_bootstrap(void) {
    if (current_task <= 0 || current_task >= num_tasks) {
        printk("[sched] FATAL: bootstrap has invalid slot %d\n", current_task);
        while (1) asm volatile("wfi");
    }

    struct task *task = &tasks[current_task];
    void (*entry)(void) = task->entry;
    if (!entry) {
        printk("[sched] FATAL: task %d has no entry point\n", task->pid);
        while (1) asm volatile("wfi");
    }

    entry();

    /* A returned task exits cleanly instead of returning to its own entry
     * address forever (the old initial-ra behaviour). */
    uint32_t pid = (uint32_t)task->pid;
    trace_sched("[sched] task %u returned; terminating\n", pid);
    terminate_task(pid);
    task_yield();
    while (1) asm volatile("wfi");
}

static int find_free_slot(void) {
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == TASK_UNUSED) return i;
    }
    if (num_tasks >= MAX_TASKS) return -1;
    return num_tasks++;
}

static int allocate_pid(void) {
    /* Zero remains reserved as an invalid/error PID for legacy callers. */
    if (next_pid == 0) next_pid = 1;
    return (int)next_pid++;
}

struct task *task_find_by_pid(uint32_t pid) {
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state != TASK_UNUSED && (uint32_t)tasks[i].pid == pid) {
            return &tasks[i];
        }
    }
    return NULL;
}

int task_slot_from_pid(uint32_t pid) {
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state != TASK_UNUSED && (uint32_t)tasks[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

int task_current_pid(void) {
    if (current_task < 0 || current_task >= num_tasks ||
        tasks[current_task].state == TASK_UNUSED) return -1;
    return tasks[current_task].pid;
}

int sched_validate(void) {
    if (num_tasks < 1 || num_tasks > MAX_TASKS ||
        current_task < 0 || current_task >= num_tasks) return -1;
    if (tasks[0].pid != KERNEL_TASK_PID || tasks[0].state == TASK_UNUSED) return -1;

    int running = 0;
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == TASK_RUNNING) running++;
        if (tasks[i].state != TASK_UNUSED &&
            tasks[i].stack_guard != TASK_STACK_GUARD) return -1;
        for (int j = i + 1; j < num_tasks; j++) {
            if (tasks[i].state != TASK_UNUSED && tasks[j].state != TASK_UNUSED &&
                tasks[i].pid == tasks[j].pid) return -1;
        }
    }
    return running == 1 && tasks[current_task].state == TASK_RUNNING ? 0 : -1;
}

int task_set_state(uint32_t pid, task_state_t state) {
    irq_state_t irq = irq_save();
    struct task *task = task_find_by_pid(pid);
    if (!task) {
        irq_restore(irq);
        return -1;
    }
    task->state = state;
    irq_restore(irq);
    return 0;
}

void idle_task1(void) {
    static int count = 0;
    while (1) {
        count++;
        if (count % 1000 == 0) {
            printk("[Task 1] Running (count: %d)...\n", count);
        }
        for (volatile int i = 0; i < 100000; i++);
        task_yield();
    }
}

void idle_task2(void) {
    static int count = 0;
    while (1) {
        count++;
        if (count % 1000 == 0) {
            printk("[Task 2] Running (count: %d)...\n", count);
        }
        for (volatile int i = 0; i < 100000; i++);
        task_yield();
    }
}

void idle_task3(void) {
    static int count = 0;
    while (1) {
        count++;
        if (count % 1000 == 0) {
            printk("[Task 3] Running (count: %d)...\n", count);
        }
        for (volatile int i = 0; i < 100000; i++);
        task_yield();
    }
}

void sched_init(void) {
    printk("[sched] Scheduler initializing...\n");
    
    // Initialize task table
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
        tasks[i].pid = 0;
        tasks[i].entry = NULL;
        tasks[i].satp = 0;
        tasks[i].user_mode = 0;
        tasks[i].user_data_pages = NULL;
        tasks[i].user_data_page_count = 0;
        tasks[i].user_stack_pages = NULL;
        tasks[i].user_stack_page_count = 0;
        tasks[i].trap_valid = 0;
        tasks[i].stack_guard = TASK_STACK_GUARD;
    }

    /* Slot zero represents the currently executing kernel-main context.
     * The first context switch saves kmain here instead of overwriting the
     * first server task's prepared context. */
    current_task = 0;
    num_tasks = 1;
    tasks[0].pid = KERNEL_TASK_PID;
    tasks[0].state = TASK_RUNNING;
    tasks[0].entry = NULL;
    tasks[0].satp = vm_kernel_satp();
    tasks[0].trap_valid = 0;

    printk("[sched] Task table initialized\n");
    
    printk("[sched] Created %d tasks\n", num_tasks);
    printk("[sched] Scheduler initialized.\n");
}

int create_task(void (*entry)(void)) {
    /*
     * Find a free slot. Originally Hydra only ever appended at
     * tasks[num_tasks], which exhausts the table after MAX_TASKS
     * tasks have ever been created (even if some have since exited).
     * Migration benchmarks need to recycle slots, so we scan for an
     * UNUSED slot first and fall back to growing num_tasks otherwise.
     */
    irq_state_t irq = irq_save();
    int slot = find_free_slot();
    if (slot < 0) {
        irq_restore(irq);
        printk("[sched] Cannot create task: task table full\n");
        return -1;
    }

    struct task *task = &tasks[slot];
    /* Keep the slot unpublished until every resumable field is complete.
     * A timer interrupt during construction must continue to see UNUSED. */
    task->state = TASK_UNUSED;
    k_memset(&task->context, 0, sizeof(task->context));
    task->pid = allocate_pid();
    task->entry = entry;
    task->stack_guard = TASK_STACK_GUARD;
    
    // Set up initial context
    task->context.ra = (unsigned long)task_bootstrap;
    /* The RISC-V ABI requires a 16-byte-aligned stack pointer. */
    task->context.sp = (unsigned long)&task->stack[TASK_STACK_SIZE];
    init_task_trap(task, entry);
    task->state = TASK_READY;
    
    printk("[sched] Created task %d\n", task->pid);
    int pid = task->pid;
    irq_restore(irq);
    return pid;
}

static int create_user_task_internal(void (*entry)(void), unsigned long arg0,
                                     task_state_t initial_state) {
    if (!entry || vm_kernel_satp() == 0) return -1;
    irq_state_t irq = irq_save();
    int slot = find_free_slot();
    if (slot < 0) {
        irq_restore(irq);
        return -1;
    }

    void *user_data = page_alloc_n(USER_DATA_PAGES);
    if (!user_data) {
        irq_restore(irq);
        return -1;
    }
    void *user_stack = page_alloc_n(USER_STACK_PAGES);
    if (!user_stack) {
        page_free_n(user_data, USER_DATA_PAGES);
        irq_restore(irq);
        return -1;
    }
    unsigned long satp = vm_create_user_space(user_data, USER_DATA_PAGES,
                                              user_stack, USER_STACK_PAGES);
    if (!satp) {
        page_free_n(user_data, USER_DATA_PAGES);
        page_free_n(user_stack, USER_STACK_PAGES);
        irq_restore(irq);
        return -1;
    }

    struct task *task = &tasks[slot];
    task->state = TASK_UNUSED;
    k_memset(&task->context, 0, sizeof(task->context));
    k_memset(&task->trap, 0, sizeof(task->trap));
    task->pid = allocate_pid();
    task->entry = NULL;
    task->satp = satp;
    task->user_mode = 1;
    task->user_data_pages = user_data;
    task->user_data_page_count = USER_DATA_PAGES;
    task->user_stack_pages = user_stack;
    task->user_stack_page_count = USER_STACK_PAGES;
    task->stack_guard = TASK_STACK_GUARD;
    task->trap.sepc = vm_user_entry((const void *)entry);
    task->trap.sp = USER_STACK_BASE + USER_STACK_PAGES * PAGE_SIZE;
    task->trap.a0 = arg0;
    task->trap.reserved = (unsigned long)&task->stack[TASK_STACK_SIZE];
    unsigned long status;
    asm volatile("csrr %0, sstatus" : "=r"(status));
    status &= ~(SSTATUS_SPP | SSTATUS_SIE);
    if (preemption_enabled) status |= SSTATUS_SPIE;
    else status &= ~SSTATUS_SPIE;
    status |= (1UL << 18); /* SUM: trap code copies user frames. */
    task->trap.sstatus = status;
    task->trap_valid = 1;
    task->state = initial_state;

    int pid = task->pid;
    printk("[sched] Created isolated U-mode task %d\n", pid);
    irq_restore(irq);
    return pid;
}

int create_user_task(void (*entry)(void)) {
    return create_user_task_internal(entry, 0, TASK_READY);
}

int create_user_task_arg(void (*entry)(void), unsigned long arg0) {
    return create_user_task_internal(entry, arg0, TASK_READY);
}

int create_migrated_user_task(void) {
    return create_user_task_internal((void (*)(void))&__user_text_start,
                                     0, TASK_BLOCKED);
}

void schedule(void) { task_yield(); }

void task_yield(void) {
    if (num_tasks <= 1) return;
    register unsigned long syscall_no asm("a7") = 6;
    /* S-mode ecall is reserved for SBI firmware and cannot trap back into
     * the same privilege level. A delegated breakpoint is our temporary
     * supervisor-mode yield gate; real U-mode tasks will use ecall. */
    /* Emit the uncompressed 32-bit encoding explicitly. With the C extension
     * enabled, the assembler otherwise selects 16-bit c.ebreak while the
     * trap handler correctly advances a system instruction by four bytes. */
    asm volatile(".4byte 0x00100073" : : "r"(syscall_no) : "memory");
}

struct trap_frame *sched_from_trap(struct trap_frame *frame, int timer_tick) {
    if (!frame || current_task < 0 || current_task >= num_tasks) return frame;

    struct task *old_task = &tasks[current_task];
    old_task->trap = *frame;
    old_task->trap_valid = 1;
    if (timer_tick && !preemption_enabled) return &old_task->trap;

    if (old_task->stack_guard != TASK_STACK_GUARD) {
        printk("[sched] FATAL: stack overflow in pid %d\n", old_task->pid);
        while (1) asm volatile("wfi");
    }
    if (old_task->state == TASK_RUNNING) old_task->state = TASK_READY;

    int next_task = current_task;
    for (int scanned = 1; scanned <= num_tasks; scanned++) {
        int candidate = (current_task + scanned) % num_tasks;
        if (tasks[candidate].state == TASK_READY && tasks[candidate].trap_valid) {
            next_task = candidate;
            break;
        }
    }
    if (next_task == current_task) {
        old_task->state = TASK_RUNNING;
        return &old_task->trap;
    }

    struct task *new_task = &tasks[next_task];
    if (new_task->stack_guard != TASK_STACK_GUARD) {
        printk("[sched] FATAL: corrupted stack guard for pid %d\n", new_task->pid);
        while (1) asm volatile("wfi");
    }
    new_task->state = TASK_RUNNING;
    current_task = next_task;
    unsigned long old_satp = old_task->satp;
    void *old_user_data = old_task->user_data_pages;
    unsigned int old_user_data_pages = old_task->user_data_page_count;
    void *old_user_stack = old_task->user_stack_pages;
    unsigned int old_user_stack_pages = old_task->user_stack_page_count;
    int reclaim_old = old_task->state == TASK_UNUSED && old_task->user_mode;
    if (vm_current_satp() != new_task->satp) vm_activate(new_task->satp);
    if (reclaim_old) {
        vm_destroy_user_space(old_satp);
        page_free_n(old_user_data, old_user_data_pages);
        page_free_n(old_user_stack, old_user_stack_pages);
        old_task->satp = vm_kernel_satp();
        old_task->user_mode = 0;
        old_task->user_data_pages = NULL;
        old_task->user_data_page_count = 0;
        old_task->user_stack_pages = NULL;
        old_task->user_stack_page_count = 0;
    }
    return &new_task->trap;
}

void sched_enable_preemption(void) { preemption_enabled = 1; }

void sched_record_exit(uint32_t pid, long code) {
    last_exit_pid = pid;
    last_exit_code = code;
}

long sched_last_exit_code(uint32_t *pid) {
    if (pid) *pid = last_exit_pid;
    return last_exit_code;
}

/* Advance wall-clock accounting in interrupt context. Context selection is
 * performed separately by sched_from_trap(), after the complete register
 * frame has been captured by trap_entry. */
void sched_timer_tick(uint64_t elapsed_ms) {
    /* Advance by elapsed milliseconds, not by 1. Callers of
     * get_tick_count() treat it as milliseconds — HEARTBEAT_INTERVAL is
     * "5000 // 5 seconds in ms", the migration timeout is "30000 // 30
     * second timeout". Incrementing per-tick instead would rescale both
     * whenever the timer frequency changed. */
    tick_counter += elapsed_ms;
}

void sched_tick(void) {
    // Called by timer interrupt - be quiet to avoid spam
    static int tick_count = 0;
    tick_count++;
    tick_counter++;
    
    // Only print every 50 ticks to reduce output
    if (tick_count % 50 == 0) {
        trace_sched("[sched] Timer tick %d\n", tick_count);
    }
    
    // Only schedule if we have tasks and they're ready
    if (num_tasks > 1) {
        schedule();
    }
}

int create_migrated_task(void) {
    irq_state_t irq = irq_save();
    int task_id = find_free_slot();
    if (task_id < 0) {
        irq_restore(irq);
        printk("[sched] Cannot create migrated task: task table full\n");
        return 0;
    }

    struct task *new_task = &tasks[task_id];

    new_task->pid = allocate_pid();
    /* A restore placeholder must remain unschedulable until migration_restore
     * has installed the complete frame and stack, then publishes READY. */
    new_task->state = TASK_BLOCKED;
    new_task->entry = NULL;
    new_task->satp = vm_kernel_satp();
    new_task->user_mode = 0;
    new_task->user_data_pages = NULL;
    new_task->user_data_page_count = 0;
    new_task->user_stack_pages = NULL;
    new_task->user_stack_page_count = 0;
    new_task->stack_guard = TASK_STACK_GUARD;
    
    // Initialize context with dummy values
    k_memset(&new_task->context, 0, sizeof(new_task->context));
    new_task->context.sp = (unsigned long)&new_task->stack[TASK_STACK_SIZE];
    init_task_trap(new_task, NULL);
    
    trace_sched("[sched] Created migrated task slot=%d pid=%d\n",
                task_id, new_task->pid);
    int pid = new_task->pid;
    irq_restore(irq);
    return pid;
}

void suspend_task(uint32_t task_id) {
    irq_state_t irq = irq_save();
    struct task *task = task_find_by_pid(task_id);
    if (!task) {
        irq_restore(irq);
        printk("[sched] Invalid task ID for suspend: %u\n", task_id);
        return;
    }

    task->state = TASK_BLOCKED;
    printk("[sched] Suspended task %u\n", task_id);
    irq_restore(irq);
}

void resume_task(uint32_t task_id) {
    irq_state_t irq = irq_save();
    struct task *task = task_find_by_pid(task_id);
    if (!task) {
        irq_restore(irq);
        printk("[sched] Invalid task ID for resume: %u\n", task_id);
        return;
    }

    if (task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
        printk("[sched] Resumed task %u\n", task_id);
    }
    irq_restore(irq);
}

void terminate_task(uint32_t task_id) {
    if (task_id == KERNEL_TASK_PID) {
        printk("[sched] Refusing to terminate kernel-main task\n");
        return;
    }
    irq_state_t irq = irq_save();
    struct task *task = task_find_by_pid(task_id);
    if (!task) {
        irq_restore(irq);
        printk("[sched] Invalid task ID for terminate: %u\n", task_id);
        return;
    }

    int slot = (int)(task - tasks);
    /* BLOCKED prevents a timer from selecting this slot while its IPC and
     * lifecycle state are being dismantled. UNUSED is published last. */
    task->state = TASK_BLOCKED;
    ipc_task_cleanup(slot);
    task->entry = NULL;
    task->trap_valid = 0;
    task->state = TASK_UNUSED;
    if (slot != current_task && task->user_mode) {
        vm_destroy_user_space(task->satp);
        page_free_n(task->user_data_pages, task->user_data_page_count);
        page_free_n(task->user_stack_pages, task->user_stack_page_count);
        task->satp = vm_kernel_satp();
        task->user_mode = 0;
        task->user_data_pages = NULL;
        task->user_data_page_count = 0;
        task->user_stack_pages = NULL;
        task->user_stack_page_count = 0;
    }
    printk("[sched] Terminated task %u\n", task_id);
    irq_restore(irq);
}

uint32_t get_active_task_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state != TASK_UNUSED) {
            count++;
        }
    }
    return count;
}

uint32_t get_memory_usage_percent(void) {
    uint32_t used_tasks = get_active_task_count();
    return (used_tasks * 100) / MAX_TASKS;
}

uint64_t get_tick_count(void) {
    return tick_counter;
}
