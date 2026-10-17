#include "../../kernel/syscall.h"

/* Historical supervisor-mode wrappers; isolated tasks issue real ecalls. */
long syscall_checkpoint(int pid, void *buf, unsigned long buflen) {
    return handle_syscall(SYS_CHECKPOINT, pid, (long)buf, (long)buflen, 0);
}

long syscall_restore(const void *buf, unsigned long buflen) {
    return handle_syscall(SYS_RESTORE, (long)buf, (long)buflen, 0, 0);
}
