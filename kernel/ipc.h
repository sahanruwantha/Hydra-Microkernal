#ifndef IPC_H
#define IPC_H

#define MAX_MESSAGE_SIZE 512  // Increased from 256 to handle larger messages
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
/* Scheduler hook: release all messages queued to a terminating task slot. */
void ipc_task_cleanup(int task_slot);

#endif
