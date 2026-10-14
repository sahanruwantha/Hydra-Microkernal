/*
 * Hydra kernel migration mechanism.
 *
 * Real checkpoint/restore against the existing scheduler. Captures the
 * task's complete asynchronous trap state (integer, control, and floating
 * point registers) plus the full 8 KB embedded stack. The user-space buffer
 * holds a versioned self-describing record with a header, register file, and
 * stack bytes. Isolated-task records also include private user stack and
 * data/heap pages.
 *
 * Known limitations (disclosed in dissertation §5 limitations):
 *  - Supervisor tasks still use Hydra's global kernel heap, which is not
 *    captured. Isolated tasks have a fixed private data/heap mapping that is
 *    captured in full.
 *  - Stack bytes are copied verbatim. If the saved stack contains
 *    absolute pointers into itself (e.g. frame pointers tracking a
 *    chain), those pointers will be stale on the destination because
 *    the new task's stack lives at a different host address. The sp
 *    itself is rebased; deeper chains are not.
 *  - Restoring across nodes with different kernel text addresses would
 *    require relocating ra/saved-ra entries on the stack. Out of scope.
 */

#include "migration.h"
#include "sched.h"
#include "printk.h"
#include "bench.h"
#include "libc.h"
#include "vm.h"
#include <stddef.h>

typedef unsigned long size_t;

#define SSTATUS_SPP      (1UL << 8)
#define SSTATUS_FS_MASK  (3UL << 13)

/* See migration.h: caller sets this to request extra payload. */
unsigned long migration_extra_bytes = 0;

unsigned long checkpoint_size_for_pid(uint32_t pid) {
    struct task *task = task_find_by_pid(pid);
    if (!task) return 0;
    unsigned long user_size = task->user_mode
        ? (unsigned long)task->user_stack_page_count * PAGE_SIZE : 0;
    unsigned long data_size = task->user_mode
        ? (unsigned long)task->user_data_page_count * PAGE_SIZE : 0;
    return sizeof(struct checkpoint_header) + sizeof(struct trap_frame) +
           TASK_STACK_SIZE + user_size + data_size + migration_extra_bytes;
}

long migration_checkpoint(uint32_t pid, void *user_buf, size_t buflen) {
    BENCH_TS("ckpt_enter");
    if (user_buf == NULL) {
        printk("[migration] checkpoint: NULL buffer\n");
        return -1;
    }

    struct task *t = task_find_by_pid(pid);
    if (!t) {
        printk("[migration] checkpoint: pid %u not found\n", pid);
        return -1;
    }
    if (t->user_mode &&
        (!t->user_stack_pages ||
         t->user_stack_page_count != USER_STACK_PAGES ||
         !t->user_data_pages ||
         t->user_data_page_count != USER_DATA_PAGES)) {
        printk("[migration] checkpoint: incomplete U-mode address space\n");
        return -1;
    }
    unsigned long user_stack_size = t->user_mode
        ? (unsigned long)t->user_stack_page_count * PAGE_SIZE : 0;
    unsigned long user_data_size = t->user_mode
        ? (unsigned long)t->user_data_page_count * PAGE_SIZE : 0;
    const unsigned long fixed_size = sizeof(struct checkpoint_header)
                                   + sizeof(struct trap_frame)
                                   + TASK_STACK_SIZE + user_stack_size
                                   + user_data_size;
    if (fixed_size > 0xffffffffUL ||
        migration_extra_bytes > 0xffffffffUL - fixed_size ||
        migration_extra_bytes > (unsigned long)-1 - fixed_size) {
        printk("[migration] checkpoint: payload size overflow\n");
        return -1;
    }
    unsigned long need = fixed_size + migration_extra_bytes;
    if (buflen < need) {
        printk("[migration] checkpoint: buffer too small (%ld < %ld)\n",
               (unsigned long)buflen, need);
        return -1;
    }

    /*
     * Freeze the task. From this point the scheduler will skip it.
     * Note: we assume the caller has yielded to give the task a chance
     * to context-switch out cleanly first; in BLOCKED state the saved
     * context in t->context is the resumable register file.
     */
    task_state_t saved_state = t->state;
    t->state = TASK_BLOCKED;
    BENCH_TS("ckpt_freeze");

    /* Compute sp offset within the task's own stack array. */
    unsigned long stack_base = t->user_mode ? USER_STACK_BASE
                                             : (unsigned long)&t->stack[0];
    unsigned long stack_top  = stack_base +
        (t->user_mode ? user_stack_size : TASK_STACK_SIZE);
    if (!t->trap_valid) {
        printk("[migration] checkpoint: pid %u has no resumable trap state\n", pid);
        t->state = saved_state;
        return -1;
    }
    unsigned long sp_val     = t->trap.sp;

    /* If the task has never run yet, sp may point to the initial top of
     * stack as set up by create_task. That's still a valid offset. */
    if (sp_val < stack_base || sp_val > stack_top) {
        printk("[migration] checkpoint: sp 0x%lx out of stack range "
               "[0x%lx,0x%lx]\n", sp_val, stack_base, stack_top);
        t->state = saved_state;
        return -1;
    }

    unsigned long sp_offset = sp_val - stack_base;

    /* Layout the buffer: header, context, stack. */
    unsigned char *p = (unsigned char *)user_buf;
    struct checkpoint_header *hdr = (struct checkpoint_header *)p;

    memset(hdr, 0, sizeof(*hdr));
    hdr->magic        = CHECKPOINT_MAGIC;
    hdr->version      = CHECKPOINT_VERSION;
    hdr->pid          = pid;
    hdr->state        = (uint32_t)saved_state;
    hdr->context_size = (uint32_t)sizeof(struct trap_frame);
    hdr->stack_size   = (uint32_t)TASK_STACK_SIZE;
    hdr->sp_offset    = (uint32_t)sp_offset;
    hdr->extra_size   = (uint32_t)migration_extra_bytes;
    hdr->flags        = t->user_mode ? CHECKPOINT_FLAG_USER : 0;
    hdr->user_stack_size = (uint32_t)user_stack_size;
    hdr->user_data_size = (uint32_t)user_data_size;
    hdr->total_size   = (uint32_t)need;

    p += sizeof(*hdr);
    memcpy(p, &t->trap, sizeof(struct trap_frame));
    p += sizeof(struct trap_frame);
    memcpy(p, t->stack, TASK_STACK_SIZE);
    p += TASK_STACK_SIZE;
    if (user_stack_size) {
        memcpy(p, t->user_stack_pages, user_stack_size);
        p += user_stack_size;
    }
    if (user_data_size) {
        memcpy(p, t->user_data_pages, user_data_size);
        p += user_data_size;
    }
    if (migration_extra_bytes > 0) {
        /* Synthetic "heap" payload: zero-filled. Cost of this memset
         * is part of the serialize phase as expected. */
        memset(p, 0, migration_extra_bytes);
    }

    printk("[migration] checkpoint pid=%u state=%u sp_off=%ld extra=%ld bytes=%ld\n",
           pid, (uint32_t)saved_state, sp_offset, migration_extra_bytes, need);

    BENCH_TS("ckpt_copied");

    /* Leave the task BLOCKED. The user-space caller decides whether to
     * resume it (failed migration) or terminate it (successful migration).
     */
    return (long)need;
}

long migration_restore(const void *user_buf, size_t buflen) {
    BENCH_TS("rest_start");
    if (user_buf == NULL) {
        printk("[migration] restore: NULL buffer\n");
        return -1;
    }

    if (buflen < sizeof(struct checkpoint_header)) {
        printk("[migration] restore: buffer too small for header\n");
        return -1;
    }

    const unsigned char *p = (const unsigned char *)user_buf;
    const struct checkpoint_header *hdr = (const struct checkpoint_header *)p;

    if (hdr->magic != CHECKPOINT_MAGIC) {
        printk("[migration] restore: bad magic 0x%x\n", hdr->magic);
        return -1;
    }
    if (hdr->version != CHECKPOINT_VERSION) {
        printk("[migration] restore: unsupported version %u\n", hdr->version);
        return -1;
    }
    if (hdr->context_size != sizeof(struct trap_frame)) {
        printk("[migration] restore: context size mismatch (%u != %ld)\n",
               hdr->context_size, (unsigned long)sizeof(struct trap_frame));
        return -1;
    }
    if (hdr->stack_size != TASK_STACK_SIZE) {
        printk("[migration] restore: stack size mismatch (%u != %d)\n",
               hdr->stack_size, TASK_STACK_SIZE);
        return -1;
    }
    if (hdr->flags & ~CHECKPOINT_FLAG_USER) {
        printk("[migration] restore: unsupported flags 0x%x\n", hdr->flags);
        return -1;
    }
    int is_user = (hdr->flags & CHECKPOINT_FLAG_USER) != 0;
    unsigned long expected_user_size = is_user
        ? USER_STACK_PAGES * PAGE_SIZE : 0;
    unsigned long expected_data_size = is_user
        ? USER_DATA_PAGES * PAGE_SIZE : 0;
    if (hdr->user_stack_size != expected_user_size) {
        printk("[migration] restore: user stack size mismatch\n");
        return -1;
    }
    if (hdr->user_data_size != expected_data_size) {
        printk("[migration] restore: user data size mismatch\n");
        return -1;
    }
    const unsigned long fixed_size = sizeof(struct checkpoint_header)
                                   + sizeof(struct trap_frame)
                                   + TASK_STACK_SIZE + expected_user_size
                                   + expected_data_size;
    if (buflen < fixed_size || hdr->extra_size > buflen - fixed_size ||
        hdr->total_size != fixed_size + hdr->extra_size) {
        printk("[migration] restore: inconsistent or truncated record\n");
        return -1;
    }
    unsigned long resumable_stack_size = is_user ? expected_user_size
                                                  : TASK_STACK_SIZE;
    if (hdr->sp_offset > resumable_stack_size) {
        printk("[migration] restore: bad sp_offset %u\n", hdr->sp_offset);
        return -1;
    }

    struct trap_frame saved_frame;
    memcpy(&saved_frame, p + sizeof(*hdr), sizeof(saved_frame));
    if (is_user) {
        unsigned long user_text_limit = USER_CODE_BASE +
            (unsigned long)&__user_text_end -
            (unsigned long)&__user_text_start;
        if (saved_frame.sepc < USER_CODE_BASE ||
            saved_frame.sepc >= user_text_limit ||
            (saved_frame.sepc & 1UL) ||
            (saved_frame.sstatus & SSTATUS_SPP)) {
            printk("[migration] restore: unsafe U-mode control state\n");
            return -1;
        }
    }

    int new_pid = is_user ? create_migrated_user_task()
                          : create_migrated_task();
    if (new_pid <= KERNEL_TASK_PID) {
        printk("[migration] restore: task table full\n");
        return -1;
    }
    struct task *t = task_find_by_pid((uint32_t)new_pid);
    if (!t) {
        printk("[migration] restore: scheduler allocation failed\n");
        return -1;
    }

    /* Copy register file. */
    const unsigned char *src = p + sizeof(*hdr);
    unsigned long safe_user_status = t->trap.sstatus;
    memcpy(&t->trap, src, sizeof(struct trap_frame));
    if (is_user) {
        /* Retain only the source floating-point state bits. Privilege and
         * interrupt-control bits come from the freshly created U task. */
        t->trap.sstatus = (safe_user_status & ~SSTATUS_FS_MASK) |
                          (t->trap.sstatus & SSTATUS_FS_MASK);
    }
    t->trap_valid = 1;

    /* Copy stack bytes. */
    src += sizeof(struct trap_frame);
    memcpy(t->stack, src, TASK_STACK_SIZE);
    src += TASK_STACK_SIZE;
    if (is_user) {
        memcpy(t->user_stack_pages, src, expected_user_size);
        src += expected_user_size;
        memcpy(t->user_data_pages, src, expected_data_size);
        src += expected_data_size;
    }

    /* Touch the extra payload bytes so the deserialize cost reflects
     * the size we're migrating. In a real implementation this would
     * populate an additional application-owned region. */
    if (hdr->extra_size > 0) {
        /* Read every byte to defeat the optimiser. */
        volatile unsigned char sink = 0;
        for (uint32_t i = 0; i < hdr->extra_size; i++) {
            sink ^= src[i];
        }
        (void)sink;
    }

    /* Rebase sp to point into THIS task's stack array at the original
     * offset. Without this, the restored task would attempt to use the
     * source's stack address and immediately fault. */
    if (is_user) {
        t->trap.sp = USER_STACK_BASE + hdr->sp_offset;
        t->trap.reserved = (unsigned long)&t->stack[TASK_STACK_SIZE];
    } else {
        t->trap.sp = (unsigned long)&t->stack[hdr->sp_offset];
    }

    /* Mark READY so the scheduler picks it up on the next tick. */
    t->state = TASK_READY;

    printk("[migration] restore: old_pid=%u new_pid=%d sp_off=%u\n",
           hdr->pid, t->pid, hdr->sp_offset);

    BENCH_TS("rest_done");
    return (long)new_pid;
}
