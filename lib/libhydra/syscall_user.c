#include "../../kernal/syscall.h"
#include <stddef.h>

// Simple system call wrapper - for now we'll use direct function calls
// since ECALL handling is disabled due to stability issues

// System call implementations using direct kernel function calls
// In a real microkernel, these would use ECALL instructions

extern long handle_syscall(long syscall_num, long arg1, long arg2, long arg3, long arg4);
extern void uart_write(const char *str, int len);
extern int uart_read(char *buffer, int max_len);

int syscall_send_msg(int receiver_pid, const void *data, int size) {
    return handle_syscall(SYS_SEND_MSG, receiver_pid, (long)data, size, 0);
}

int syscall_recv_msg(int sender_pid, void *buffer, int max_size) {
    return handle_syscall(SYS_RECV_MSG, sender_pid, (long)buffer, max_size, 0);
}

int syscall_net_send(const void *data, int size) {
    return handle_syscall(SYS_NET_SEND, (long)data, size, 0, 0);
}

int syscall_net_recv(void *buffer, int max_size) {
    return handle_syscall(SYS_NET_RECV, (long)buffer, max_size, 0, 0);
}

int syscall_get_pid(void) {
    return handle_syscall(SYS_GET_PID, 0, 0, 0, 0);
}

void syscall_yield(void) {
    handle_syscall(SYS_YIELD, 0, 0, 0, 0);
}

int syscall_create_task(void (*entry_point)(void)) {
    return handle_syscall(SYS_CREATE_TASK, (long)entry_point, 0, 0, 0);
}

void syscall_exit(void) {
    handle_syscall(SYS_EXIT, 0, 0, 0, 0);
}

// I/O syscalls for libc
void syscall_write(const char *str, size_t len) {
    // For now, use handle_syscall to implement I/O
    // In a real system, this would be a proper syscall
    extern void uart_puts(const char *str);
    extern void printk(const char *format, ...);
    printk("%.*s", (int)len, str);
}

int syscall_read(char *buffer, size_t max_len) {
    // Simple implementation - read one character
    extern char uart_getc(void);
    if (max_len > 0) {
        buffer[0] = uart_getc();
        return 1;
    }
    return 0;
}
