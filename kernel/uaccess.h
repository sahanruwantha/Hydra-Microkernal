#ifndef UACCESS_H
#define UACCESS_H

#include "sched.h"

int user_range_valid(const struct task *task, const void *address,
                     unsigned long length, int write_access);
int copy_from_user_task(const struct task *task, void *kernel_dst,
                        const void *user_src, unsigned long length);
int copy_to_user_task(const struct task *task, void *user_dst,
                      const void *kernel_src, unsigned long length);
int copy_from_user(void *kernel_dst, const void *user_src,
                   unsigned long length);
int copy_to_user(void *user_dst, const void *kernel_src,
                 unsigned long length);

#endif
