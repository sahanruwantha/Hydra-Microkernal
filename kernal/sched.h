#ifndef SCHED_H
#define SCHED_H

#define MAX_TASKS 8
#define TASK_STACK_SIZE 4096

// Task states
typedef enum {
    TASK_UNUSED = 0,
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED
} task_state_t;

// Context structure for saving/restoring registers
struct task_context {
    unsigned long ra;  // return address
    unsigned long sp;  // stack pointer
    unsigned long s0;  // saved registers s0-s11
    unsigned long s1;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
};

// Task control block
struct task {
    int pid;
    task_state_t state;
    struct task_context context;
    char stack[TASK_STACK_SIZE];
};

// External assembly function
extern void context_switch(struct task_context *old, struct task_context *new);
extern void set_trap_vector(void);

// External variables
extern struct task tasks[MAX_TASKS];
extern int current_task;
extern int num_tasks;

// Function declarations
void sched_init(void);
void schedule(void);
void sched_tick(void);
int create_task(void (*entry)(void));
void task_yield(void);

#endif
