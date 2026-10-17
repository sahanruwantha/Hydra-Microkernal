#ifndef SCHED_H
#define SCHED_H

// Basic type definitions for cross-compilation (only if not already defined)
#ifndef UINT8_T_DEFINED
#define UINT8_T_DEFINED
typedef unsigned char uint8_t;
#endif

#ifndef UINT16_T_DEFINED
#define UINT16_T_DEFINED
typedef unsigned short uint16_t;
#endif

#ifndef UINT32_T_DEFINED
#define UINT32_T_DEFINED
typedef unsigned int uint32_t;
#endif

#ifndef UINT64_T_DEFINED
#define UINT64_T_DEFINED
typedef unsigned long uint64_t;
#endif

#define MAX_TASKS 16
#define TASK_STACK_SIZE 8192
#define KERNEL_TASK_PID 0
#define TASK_STACK_GUARD 0x4859445247554152UL /* "HYDRGUAR" */

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
    /* RISC-V psABI callee-saved floating-point registers. */
    unsigned long fs0;
    unsigned long fs1;
    unsigned long fs2;
    unsigned long fs3;
    unsigned long fs4;
    unsigned long fs5;
    unsigned long fs6;
    unsigned long fs7;
    unsigned long fs8;
    unsigned long fs9;
    unsigned long fs10;
    unsigned long fs11;
    unsigned long fcsr;
};

/* Complete asynchronous supervisor-trap state. Assembly offsets in
 * trap_riscv.s intentionally mirror this declaration. */
struct trap_frame {
    unsigned long ra, sp, gp, tp;
    unsigned long t0, t1, t2;
    unsigned long s0, s1;
    unsigned long a0, a1, a2, a3, a4, a5, a6, a7;
    unsigned long s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    unsigned long t3, t4, t5, t6;
    unsigned long sepc, sstatus, stval;
    unsigned long f[32];
    unsigned long fcsr;
    unsigned long reserved;
};

typedef char trap_frame_size_must_be_544[(sizeof(struct trap_frame) == 544) ? 1 : -1];

// Task control block
struct task {
    int pid;
    task_state_t state;
    struct task_context context;
    struct trap_frame trap;
    int trap_valid;
    void (*entry)(void);
    unsigned long satp;
    int user_mode;
    void *user_data_pages;
    unsigned int user_data_page_count;
    void *user_stack_pages;
    unsigned int user_stack_page_count;
    unsigned long stack_guard;
    char stack[TASK_STACK_SIZE];
};

// External assembly function
extern void set_trap_vector(void);

// External variables
extern struct task tasks[MAX_TASKS];
extern int current_task;
extern int num_tasks;

// Function declarations
void sched_init(void);
int create_task(void (*entry)(void));
int create_user_task(void (*entry)(void));
int create_user_task_arg(void (*entry)(void), unsigned long arg0);
int create_migrated_user_task(void);
int create_migrated_task(void);
void task_yield(void);
void resume_task(uint32_t task_id);
void terminate_task(uint32_t task_id);
uint32_t get_active_task_count(void);
/* PID-safe lifecycle helpers. PIDs are opaque and are not task slots. */
struct task *task_find_by_pid(uint32_t pid);
int task_slot_from_pid(uint32_t pid);
int task_current_pid(void);
int sched_validate(void);
struct trap_frame *sched_from_trap(struct trap_frame *frame, int timer_tick);
void sched_enable_preemption(void);
void sched_record_exit(uint32_t pid, long code);
long sched_last_exit_code(uint32_t *pid);

#endif
