#ifndef MIGRATION_H
#define MIGRATION_H

#include "sched.h"

/*
 * Hydra minimal kernel migration API.
 *
 * Two primitives only. All migration policy (target selection, network
 * transport, retry, fault tolerance) lives in user space. The kernel
 * captures and restores process state; nothing else.
 *
 *   sys_checkpoint(pid, buf, len)  -> bytes_written, or -1 on error.
 *   sys_restore(buf, len)          -> new_pid, or -1 on error.
 *
 * Checkpoint layout in the user-provided buffer:
 *
 *   +----------------------------+  offset 0
 *   | struct checkpoint_header   |
 *   +----------------------------+  offset sizeof(header)
 *   | struct trap_frame (state)  |
 *   +----------------------------+  offset sizeof(header)+sizeof(context)
 *   | kernel/task stack (8 KB)   |
 *   +----------------------------+
 *   | user stack (if USER flag)  |
 *   +----------------------------+
 *   | user data/heap (USER flag) |
 *   +----------------------------+  offset total_size
 */

#define CHECKPOINT_MAGIC   0x48594452u  /* 'HYDR' */
#define CHECKPOINT_VERSION 4u
#define CHECKPOINT_FLAG_USER (1u << 0)

struct checkpoint_header {
    uint32_t magic;
    uint32_t version;
    uint32_t pid;            /* original pid on the source node */
    uint32_t state;          /* task_state_t at checkpoint time */
    uint32_t context_size;   /* sizeof(struct trap_frame) */
    uint32_t stack_size;     /* TASK_STACK_SIZE */
    uint32_t sp_offset;      /* sp - &stack[0] on source; rebased on restore */
    uint32_t extra_size;     /* synthetic "heap" payload after stack */
    uint32_t flags;          /* CHECKPOINT_FLAG_* for version 4 */
    uint32_t user_stack_size;/* private U stack bytes; zero for S tasks */
    uint32_t user_data_size; /* private U data/heap bytes; zero for S tasks */
    uint32_t total_size;     /* whole record incl. header */
};

/* Caller sets this before calling migration_checkpoint to request
 * extra payload bytes (zero-filled) after the stack. Lets benchmarks
 * sweep state size without changing the real task struct.
 *
 * Isolated tasks now have captured private data/heap pages. This synthetic
 * blob remains solely for the historical supervisor-task size-scaling
 * benchmark, whose global kernel heap is not process-owned. */
extern unsigned long migration_extra_bytes;

/* Kernel-facing entry points (called by syscall dispatch). */
long migration_checkpoint(uint32_t pid, void *user_buf, unsigned long buflen);
long migration_restore(const void *user_buf, unsigned long buflen);

/* Bytes occupied by the historical supervisor benchmark record. Isolated
 * callers must use checkpoint_size_for_pid() to include private regions. */
static inline unsigned long checkpoint_size_for_task(void) {
    return sizeof(struct checkpoint_header)
         + sizeof(struct trap_frame)
         + TASK_STACK_SIZE
         + migration_extra_bytes;
}

unsigned long checkpoint_size_for_pid(uint32_t pid);

#endif /* MIGRATION_H */
