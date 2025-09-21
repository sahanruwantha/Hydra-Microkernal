#include "sched.h"
#include "printk.h"
#include "mm.h"

struct task tasks[MAX_TASKS];
int current_task = 0;
int num_tasks = 0;

// Dummy task functions for testing
void idle_task1(void) {
    static int count = 0;
    while (1) {
        count++;
        if (count % 1000 == 0) {
            printk("[Task 1] Running (count: %d)...\n", count);
        }
        for (volatile int i = 0; i < 100000; i++); // Simple delay
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
        for (volatile int i = 0; i < 100000; i++); // Simple delay
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
        for (volatile int i = 0; i < 100000; i++); // Simple delay
        task_yield();
    }
}

void sched_init(void) {
    printk("[sched] Scheduler initializing...\n");
    
    // Initialize task table
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
        tasks[i].pid = i;
    }
    
    printk("[sched] Task table initialized\n");
    
    // Trap vector setup causes system instability - keep disabled for now
    // TODO: Debug trap vector issue in future phase
    // set_trap_vector();
    // printk("[sched] Trap vector set\n");
    printk("[sched] Trap vector setup disabled (causes instability)\n");
    
    // Create some test tasks
    create_task(idle_task1);
    create_task(idle_task2);
    create_task(idle_task3);
    
    printk("[sched] Created %d tasks\n", num_tasks);
    printk("[sched] Scheduler initialized.\n");
}

int create_task(void (*entry)(void)) {
    if (num_tasks >= MAX_TASKS) {
        printk("[sched] Cannot create task: task table full\n");
        return -1;
    }
    
    struct task *task = &tasks[num_tasks];
    task->state = TASK_READY;
    task->pid = num_tasks;
    
    // Set up initial context
    task->context.ra = (unsigned long)entry;
    task->context.sp = (unsigned long)&task->stack[TASK_STACK_SIZE - 1];
    
    // Clear other registers
    task->context.s0 = 0;
    task->context.s1 = 0;
    task->context.s2 = 0;
    task->context.s3 = 0;
    task->context.s4 = 0;
    task->context.s5 = 0;
    task->context.s6 = 0;
    task->context.s7 = 0;
    task->context.s8 = 0;
    task->context.s9 = 0;
    task->context.s10 = 0;
    task->context.s11 = 0;
    
    num_tasks++;
    printk("[sched] Created task %d\n", task->pid);
    return task->pid;
}

void schedule(void) {
    if (num_tasks <= 1) return; // Nothing to schedule
    
    struct task *old_task = &tasks[current_task];
    
    // Find next ready task (round-robin)
    int next_task = (current_task + 1) % num_tasks;
    while (next_task != current_task && tasks[next_task].state != TASK_READY) {
        next_task = (next_task + 1) % num_tasks;
    }
    
    if (next_task == current_task) return; // No other ready tasks
    
    struct task *new_task = &tasks[next_task];
    
    // Reduce debug output to prevent spam
    static int switch_count = 0;
    switch_count++;
    if (switch_count % 100 == 0) {
        printk("[sched] Switch #%d: task %d -> task %d\n", switch_count, current_task, next_task);
    }
    
    // Update task states
    if (old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    new_task->state = TASK_RUNNING;
    
    current_task = next_task;
    
    // Perform context switch
    context_switch(&old_task->context, &new_task->context);
}

void task_yield(void) {
    // Only schedule if we have multiple tasks and they're properly initialized
    if (num_tasks > 1) {
        schedule();
    }
}

void sched_tick(void) {
    // Called by timer interrupt - be quiet to avoid spam
    static int tick_count = 0;
    tick_count++;
    
    // Only print every 50 ticks to reduce output
    if (tick_count % 50 == 0) {
        printk("[sched] Timer tick %d\n", tick_count);
    }
    
    // Only schedule if we have tasks and they're ready
    if (num_tasks > 1) {
        schedule();
    }
}
