#include "../kernel/sched.h"
#include "../kernel/vm.h"
#include "../kernel/mm.h"
#include "../kernel/printk.h"
#include "../kernel/ipc.h"
#include "../kernel/migration.h"

#define COPY_REQUEST  0x13572468U
#define COPY_RESPONSE 0x24681357U

static volatile uint32_t copy_user_pid;
static volatile unsigned int copy_sink_received;
static unsigned char user_checkpoint_buffer[26000] __attribute__((aligned(16)));

static void copy_sink_task(void) {
    uint32_t value = 0;
    while (recv_message((int)copy_user_pid, &value, sizeof(value)) < 0)
        task_yield();
    if (value == COPY_REQUEST) copy_sink_received = 1;
    uint32_t response = COPY_RESPONSE;
    while (send_message((int)copy_user_pid, &response, sizeof(response)) < 0)
        task_yield();
    /* Exercise a completed reply whose helper has not exited yet. */
    task_yield();
}

/* This function is mapped at USER_CODE_BASE in a private Sv39 root. It has no
 * direct kernel references: all transitions use the U-mode ecall ABI. */
__attribute__((section(".user_text"), noinline, noreturn, used))
void isolated_user_entry(void) {
    asm volatile(
        "li a7, 5\n"          /* SYS_GET_PID */
        "ecall\n"
        "mv s0, a0\n"        /* survives the yield trap */
        "li a7, 6\n"          /* SYS_YIELD */
        "ecall\n"
        "mv a0, s0\n"        /* exit code must equal assigned PID */
        "li a7, 8\n"          /* SYS_EXIT */
        "ecall\n"
        "1: j 1b\n"
        : : : "a0", "a7", "s0", "memory");
    __builtin_unreachable();
}

__attribute__((section(".user_text"), noinline, noreturn, used))
static void isolated_write_attack(void) {
    asm volatile(
        "sd zero, 0(a0)\n"    /* a0 is a supervisor-only kernel address */
        "li a0, 0xBAD\n"      /* reaching exit means isolation failed */
        "li a7, 8\n"
        "ecall\n"
        "1: j 1b\n"
        : : : "a0", "a7", "memory");
    __builtin_unreachable();
}

__attribute__((section(".user_text"), noinline, noreturn, used))
static void isolated_ipc_roundtrip(void) {
    asm volatile(
        "mv s1, a0\n"          /* supervisor sink PID */
        "addi t0, sp, -16\n"   /* private user-stack buffer */
        "li t1, 0x13572468\n"
        "sw t1, 0(t0)\n"
        "mv a0, s1\n"
        "mv a1, t0\n"
        "li a2, 4\n"
        "li a7, 1\n"           /* SYS_SEND_MSG: copyin */
        "ecall\n"
        "bnez a0, 3f\n"
        "1:\n"
        "mv a0, s1\n"
        "mv a1, t0\n"
        "li a2, 4\n"
        "li a7, 2\n"           /* SYS_RECV_MSG: copyout */
        "ecall\n"
        "bgtz a0, 2f\n"
        "li a7, 6\n"
        "ecall\n"
        "j 1b\n"
        "2:\n"
        "lw t1, 0(t0)\n"
        "li t2, 0x24681357\n"
        "bne t1, t2, 3f\n"
        "mv a0, s1\n"
        "li a1, 0\n"          /* unmapped IPC source must be rejected */
        "li a2, 4\n"
        "li a7, 1\n"
        "ecall\n"
        "bgez a0, 3f\n"
        "li a0, 0\n"          /* unmapped network source must be rejected */
        "li a1, 4\n"
        "li a7, 3\n"
        "ecall\n"
        "bgez a0, 3f\n"
        "li a7, 5\n"           /* successful exit code = own PID */
        "ecall\n"
        "j 4f\n"
        "3:\n"
        "li a0, -1\n"
        "4:\n"
        "li a7, 8\n"
        "ecall\n"
        "5: j 5b\n"
        : : : "a0", "a1", "a2", "a7", "t0", "t1", "t2", "s1", "memory");
    __builtin_unreachable();
}

__attribute__((section(".user_text"), noinline, noreturn, used))
static void isolated_migration_loop(void) {
    asm volatile(
        "mv s1, a0\n"          /* iterations required */
        "li s2, 0x40001000\n"  /* first private data/heap page */
        "li s3, 0x40002ff8\n"  /* final word of the second page */
        "li t0, 0x1122334455667788\n"
        "sd t0, 0(s2)\n"
        "li t0, 0x8877665544332211\n"
        "sd t0, 0(s3)\n"
        "li s0, 0\n"
        "1:\n"
        "addi s0, s0, 1\n"
        "li a7, 6\n"
        "ecall\n"              /* checkpoint resumes after this ecall */
        "li t0, 0x1122334455667788\n"
        "ld t1, 0(s2)\n"
        "bne t0, t1, 3f\n"
        "li t0, 0x8877665544332211\n"
        "ld t1, 0(s3)\n"
        "bne t0, t1, 3f\n"
        "blt s0, s1, 1b\n"
        "li a7, 5\n"
        "ecall\n"              /* exit code = destination PID */
        "j 4f\n"
        "3:\n"
        "li a0, -2\n"          /* restored data/heap contents differ */
        "4:\n"
        "li a7, 8\n"
        "ecall\n"
        "5: j 5b\n"
        : : : "a0", "a7", "t0", "t1", "s0", "s1", "s2", "s3", "memory");
    __builtin_unreachable();
}

void isolation_self_test(void) {
    unsigned int allocator_before = page_free_count();
    void *three_pages = page_alloc_n(3);
    void *one_page = page_alloc();
    if (!three_pages || !one_page ||
        ((unsigned long)three_pages & (PAGE_SIZE - 1)) ||
        ((unsigned long)one_page & (PAGE_SIZE - 1)) ||
        one_page == three_pages) {
        printk("[mmtest] FAIL: physical page allocation\n");
        return;
    }
    page_free_n(three_pages, 3);
    page_free(one_page);
    if (page_free_count() != allocator_before || page_allocator_validate() < 0) {
        printk("[mmtest] FAIL: page reclamation/invariants\n");
        return;
    }
    printk("[mmtest] PASS: aligned contiguous allocation and reclamation\n");

    printk("[isotest] Sv39/U-mode isolation test starting\n");
    unsigned int active_before = get_active_task_count();
    unsigned int pages_before = page_free_count();
    int pid = create_user_task(isolated_user_entry);
    if (pid < 0) {
        printk("[isotest] FAIL: could not create U-mode task\n");
        return;
    }

    struct task *task = task_find_by_pid((uint32_t)pid);
    unsigned long pa, code_flags, data_flags, stack_flags, kernel_flags;
    if (!task ||
        vm_translate(task->satp, USER_CODE_BASE, &pa, &code_flags) < 0 ||
        vm_translate(task->satp, USER_DATA_BASE, &pa, &data_flags) < 0 ||
        vm_translate(task->satp, USER_STACK_BASE, &pa, &stack_flags) < 0 ||
        vm_translate(task->satp, USER_GUARD_BASE, &pa, 0) == 0 ||
        vm_translate(task->satp, (unsigned long)&tasks, &pa, &kernel_flags) < 0 ||
        !(code_flags & PTE_U) || !(code_flags & PTE_X) ||
        (code_flags & PTE_W) || !(data_flags & PTE_U) ||
        !(data_flags & PTE_W) || (data_flags & PTE_X) ||
        !(stack_flags & PTE_U) ||
        !(stack_flags & PTE_W) || (stack_flags & PTE_X) ||
        (kernel_flags & PTE_U)) {
        printk("[isotest] FAIL: page permission mismatch\n");
        terminate_task((uint32_t)pid);
        task_yield();
        return;
    }

    uint32_t exited_pid = 0;
    for (int spins = 0; spins < 128; spins++) {
        task_yield();
        (void)sched_last_exit_code(&exited_pid);
        if (exited_pid == (uint32_t)pid) break;
    }
    long exit_code = sched_last_exit_code(&exited_pid);
    if (exited_pid != (uint32_t)pid || exit_code != pid ||
        get_active_task_count() != active_before ||
        page_free_count() != pages_before || page_allocator_validate() < 0 ||
        vm_validate() < 0) {
        printk("[isotest] FAIL: pid=%u code=%d pages=%u/%u active=%u/%u\n",
               exited_pid, (int)exit_code, page_free_count(), pages_before,
               get_active_task_count(), active_before);
        return;
    }

    copy_sink_received = 0;
    copy_user_pid = 0;
    pages_before = page_free_count();
    int sink_pid = create_task(copy_sink_task);
    int copy_pid = create_user_task_arg(isolated_ipc_roundtrip,
                                        (unsigned long)sink_pid);
    if (sink_pid < 0 || copy_pid < 0) {
        printk("[isotest] FAIL: could not create copyin/copyout tasks\n");
        return;
    }
    copy_user_pid = (uint32_t)copy_pid;
    exited_pid = 0;
    for (int spins = 0; spins < 256; spins++) {
        task_yield();
        (void)sched_last_exit_code(&exited_pid);
        if (exited_pid == (uint32_t)copy_pid && copy_sink_received) break;
    }
    exit_code = sched_last_exit_code(&exited_pid);
    /* Receiving the reply does not imply the helper has reclaimed its slot. */
    for (int spins = 0; spins < 256 &&
         task_find_by_pid((uint32_t)sink_pid); spins++) {
        task_yield();
    }
    if (!copy_sink_received || exited_pid != (uint32_t)copy_pid ||
        exit_code != copy_pid || get_active_task_count() != active_before ||
        page_free_count() != pages_before || page_allocator_validate() < 0) {
        printk("[isotest] FAIL: copyin/copyout IPC pid=%u code=%d received=%u\n",
               exited_pid, (int)exit_code, copy_sink_received);
        return;
    }

    pages_before = page_free_count();
    unsigned long saved_extra = migration_extra_bytes;
    migration_extra_bytes = 0;
    int source_pid = create_user_task_arg(isolated_migration_loop, 6);
    if (source_pid < 0) {
        printk("[isotest] FAIL: could not create U-mode migration task\n");
        migration_extra_bytes = saved_extra;
        return;
    }
    task_yield();
    unsigned long record_size = checkpoint_size_for_pid((uint32_t)source_pid);
    long written = migration_checkpoint((uint32_t)source_pid,
                                        user_checkpoint_buffer,
                                        sizeof(user_checkpoint_buffer));
    if (!record_size || written != (long)record_size) {
        printk("[isotest] FAIL: U-mode checkpoint size %d/%u\n",
               (int)written, (unsigned int)record_size);
        terminate_task((uint32_t)source_pid);
        migration_extra_bytes = saved_extra;
        return;
    }
    terminate_task((uint32_t)source_pid);
    int restored_pid = (int)migration_restore(user_checkpoint_buffer,
                                              (unsigned long)written);
    migration_extra_bytes = saved_extra;
    if (restored_pid < 0) {
        printk("[isotest] FAIL: U-mode restore rejected\n");
        return;
    }
    exited_pid = 0;
    for (int spins = 0; spins < 256; spins++) {
        task_yield();
        (void)sched_last_exit_code(&exited_pid);
        if (exited_pid == (uint32_t)restored_pid) break;
    }
    exit_code = sched_last_exit_code(&exited_pid);
    if (exited_pid != (uint32_t)restored_pid || exit_code != restored_pid ||
        page_free_count() != pages_before || page_allocator_validate() < 0) {
        printk("[isotest] FAIL: U-mode migration pid=%u/%d code=%d pages=%u/%u\n",
               exited_pid, restored_pid, (int)exit_code,
               page_free_count(), pages_before);
        return;
    }

    struct checkpoint_header *saved_header =
        (struct checkpoint_header *)user_checkpoint_buffer;
    struct trap_frame *saved_frame = (struct trap_frame *)(
        user_checkpoint_buffer + sizeof(*saved_header));
    unsigned long saved_status = saved_frame->sstatus;
    saved_frame->sstatus |= (1UL << 8); /* forged supervisor return state */
    unsigned int pages_before_reject = page_free_count();
    unsigned int active_before_reject = get_active_task_count();
    long unsafe_restore = migration_restore(user_checkpoint_buffer,
                                             (unsigned long)written);
    saved_frame->sstatus = saved_status;
    if (unsafe_restore >= 0 || page_free_count() != pages_before_reject ||
        get_active_task_count() != active_before_reject ||
        page_allocator_validate() < 0) {
        printk("[isotest] FAIL: unsafe U-mode restore was not contained\n");
        return;
    }

    pages_before = page_free_count();
    int attacker_pid = create_user_task_arg(isolated_write_attack,
                                            (unsigned long)&tasks);
    if (attacker_pid < 0) {
        printk("[isotest] FAIL: could not create protection test task\n");
        return;
    }
    /* Exercise completion before the creator inspects the exit result. */
    task_yield();
    exited_pid = 0;
    for (int spins = 0; spins < 128; spins++) {
        task_yield();
        (void)sched_last_exit_code(&exited_pid);
        if (exited_pid == (uint32_t)attacker_pid) break;
    }
    exit_code = sched_last_exit_code(&exited_pid);
    if (exited_pid != (uint32_t)attacker_pid || exit_code != -15 ||
        task_find_by_pid((uint32_t)attacker_pid) ||
        page_free_count() != pages_before || page_allocator_validate() < 0) {
        printk("[isotest] FAIL: kernel-write containment pid=%u code=%d pages=%u/%u\n",
               exited_pid, (int)exit_code, page_free_count(), pages_before);
        return;
    }

    printk("[isotest] PASS: U-mode IPC, data/heap migration, W^X, safe restore, containment, reclamation\n");
}
