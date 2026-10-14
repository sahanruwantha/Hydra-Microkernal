#include "uaccess.h"
#include "vm.h"

static int range_bounds(const void *address, unsigned long length,
                        unsigned long *start, unsigned long *end) {
    unsigned long first = (unsigned long)address;
    if (length == 0) {
        if (start) *start = first;
        if (end) *end = first;
        return 0;
    }
    if (!address || first > (unsigned long)-1 - (length - 1)) return -1;
    unsigned long last = first + length - 1;
    /* Low canonical Sv39 user half only. */
    if (last >= (1UL << 38)) return -1;
    if (start) *start = first;
    if (end) *end = last;
    return 0;
}

int user_range_valid(const struct task *task, const void *address,
                     unsigned long length, int write_access) {
    if (!task || !task->user_mode || !task->satp) return -1;
    unsigned long start, end;
    if (range_bounds(address, length, &start, &end) < 0) return -1;
    if (length == 0) return 0;

    unsigned long page = start & ~(PAGE_SIZE - 1);
    unsigned long last_page = end & ~(PAGE_SIZE - 1);
    for (;;) {
        unsigned long pa, flags;
        if (vm_translate(task->satp, page, &pa, &flags) < 0 ||
            !(flags & PTE_U) || !(flags & PTE_R) ||
            (write_access && !(flags & PTE_W))) return -1;
        if (page == last_page) break;
        page += PAGE_SIZE;
    }
    return 0;
}

int copy_from_user_task(const struct task *task, void *kernel_dst,
                        const void *user_src, unsigned long length) {
    if ((!kernel_dst && length) ||
        user_range_valid(task, user_src, length, 0) < 0) return -1;
    unsigned char *dst = (unsigned char *)kernel_dst;
    unsigned long cursor = (unsigned long)user_src;
    unsigned long remaining = length;
    while (remaining) {
        unsigned long pa, flags;
        if (vm_translate(task->satp, cursor, &pa, &flags) < 0) return -1;
        unsigned long chunk = PAGE_SIZE - (cursor & (PAGE_SIZE - 1));
        if (chunk > remaining) chunk = remaining;
        const unsigned char *src = (const unsigned char *)pa;
        for (unsigned long i = 0; i < chunk; i++) dst[i] = src[i];
        dst += chunk;
        cursor += chunk;
        remaining -= chunk;
    }
    return 0;
}

int copy_to_user_task(const struct task *task, void *user_dst,
                      const void *kernel_src, unsigned long length) {
    if ((!kernel_src && length) ||
        user_range_valid(task, user_dst, length, 1) < 0) return -1;
    const unsigned char *src = (const unsigned char *)kernel_src;
    unsigned long cursor = (unsigned long)user_dst;
    unsigned long remaining = length;
    while (remaining) {
        unsigned long pa, flags;
        if (vm_translate(task->satp, cursor, &pa, &flags) < 0) return -1;
        unsigned long chunk = PAGE_SIZE - (cursor & (PAGE_SIZE - 1));
        if (chunk > remaining) chunk = remaining;
        unsigned char *dst = (unsigned char *)pa;
        for (unsigned long i = 0; i < chunk; i++) dst[i] = src[i];
        src += chunk;
        cursor += chunk;
        remaining -= chunk;
    }
    return 0;
}

int copy_from_user(void *kernel_dst, const void *user_src,
                   unsigned long length) {
    if (current_task < 0 || current_task >= num_tasks) return -1;
    return copy_from_user_task(&tasks[current_task], kernel_dst,
                               user_src, length);
}

int copy_to_user(void *user_dst, const void *kernel_src,
                 unsigned long length) {
    if (current_task < 0 || current_task >= num_tasks) return -1;
    return copy_to_user_task(&tasks[current_task], user_dst,
                             kernel_src, length);
}
