#include "hydra.h"

// Forward declarations for syscalls
extern int syscall_get_pid(void);
extern int syscall_create_task(void (*entry_point)(void));
extern void syscall_yield(void);
extern void syscall_exit(void);
extern int syscall_send_msg(int receiver_pid, const void *data, int size);
extern int syscall_recv_msg(int sender_pid, void *buffer, int max_size);

// Process management functions

pid_t get_pid(void) {
    return syscall_get_pid();
}

int create_process(void (*entry_point)(void)) {
    return syscall_create_task(entry_point);
}

void yield_process(void) {
    syscall_yield();
}

void exit_process(void) {
    syscall_exit();
}

// IPC functions

int ipc_send_message(pid_t receiver, int msg_type, const void *data, size_t size) {
    if (size > 256) {
        return -1; // Message too large
    }
    
    struct ipc_message msg;
    msg.sender_pid = get_pid();
    msg.receiver_pid = receiver;
    msg.message_type = msg_type;
    msg.data_size = size;
    
    // Copy data into message
    char *src = (char *)data;
    for (size_t i = 0; i < size; i++) {
        msg.data[i] = src[i];
    }
    
    return syscall_send_msg(receiver, &msg, sizeof(struct ipc_message));
}

int ipc_receive_message(pid_t sender, int *msg_type, void *buffer, size_t max_size) {
    struct ipc_message msg;
    int result = syscall_recv_msg(sender, &msg, sizeof(struct ipc_message));
    
    if (result > 0) {
        *msg_type = msg.message_type;
        
        // Copy data to buffer
        size_t copy_size = (msg.data_size < max_size) ? msg.data_size : max_size;
        char *dest = (char *)buffer;
        for (size_t i = 0; i < copy_size; i++) {
            dest[i] = msg.data[i];
        }
        
        return copy_size;
    }
    
    return result;
}

int send_request(pid_t server, const void *request, size_t req_size, 
                 void *response, size_t max_resp_size) {
    // Send request
    int result = ipc_send_message(server, MSG_TYPE_REQUEST, request, req_size);
    if (result < 0) {
        return result;
    }
    
    // Wait for response
    int msg_type;
    return ipc_receive_message(server, &msg_type, response, max_resp_size);
}