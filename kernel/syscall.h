#ifndef SYSCALL_H
#define SYSCALL_H

// System call numbers
#define SYS_SEND_MSG    1
#define SYS_RECV_MSG    2
#define SYS_NET_SEND    3
#define SYS_NET_RECV    4
#define SYS_GET_PID     5
#define SYS_YIELD       6
#define SYS_CREATE_TASK 7
#define SYS_EXIT        8
#define SYS_CHECKPOINT  20  /* migration: dump task state to user buffer */
#define SYS_RESTORE     21  /* migration: instantiate task from user buffer */

// System call return values
#define SYSCALL_OK      0
#define SYSCALL_ERROR  -1

long handle_syscall(long syscall_num, long arg1, long arg2,
                    long arg3, long arg4);
struct trap_frame;
long handle_user_syscall(struct trap_frame *frame);

/* Historical supervisor-mode wrappers around the migration primitives. */
long syscall_checkpoint(int pid, void *buf, unsigned long buflen);
long syscall_restore(const void *buf, unsigned long buflen);

#endif
