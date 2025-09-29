#ifndef IPC_H
#define IPC_H

#define MAX_MESSAGE_SIZE 256
#define MAX_MESSAGES 64  // Increased from 16 to handle more concurrent messages

// Message structure
struct message {
    int sender_pid;
    int receiver_pid;
    int size;
    char data[MAX_MESSAGE_SIZE];
    struct message *next;
};

// Message queue for each process
struct msg_queue {
    struct message *head;
    struct message *tail;
    int count;
};

// IPC operations
void ipc_init(void);
int send_message(int receiver_pid, const void *data, int size);
int recv_message(int sender_pid, void *buffer, int max_size);
int try_recv_message(int sender_pid, void *buffer, int max_size);
void ipc_debug_status(void); // Debug function to show IPC status
void ipc_log_queue_status(int pid); // Log status of specific queue
void ipc_log_memory_usage(void); // Log current memory usage

#endif
