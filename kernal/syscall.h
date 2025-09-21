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

// System call return values
#define SYSCALL_OK      0
#define SYSCALL_ERROR  -1

// System call data structure
struct syscall_args {
    long syscall_num;
    long arg1;
    long arg2;
    long arg3;
    long arg4;
    long result;
};

// System call functions for user-space
int syscall_send_msg(int receiver_pid, const void *data, int size);
int syscall_recv_msg(int sender_pid, void *buffer, int max_size);
int syscall_net_send(const void *data, int size);
int syscall_net_recv(void *buffer, int max_size);
int syscall_get_pid(void);
void syscall_yield(void);
int syscall_create_task(void (*entry_point)(void));
void syscall_exit(void);

#endif
