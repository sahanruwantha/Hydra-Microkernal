#include "ipc.h"
#include "printk.h"
#include "mm.h"
#include "sched.h"

static struct msg_queue message_queues[MAX_TASKS];
static struct message *free_messages = 0;

void ipc_init(void) {
    // Initialize message queues
    for (int i = 0; i < MAX_TASKS; i++) {
        message_queues[i].head = 0;
        message_queues[i].tail = 0;
        message_queues[i].count = 0;
    }
    
    // Pre-allocate message pool
    for (int i = 0; i < MAX_MESSAGES; i++) {
        struct message *msg = (struct message *)kmalloc(sizeof(struct message));
        if (msg) {
            msg->next = free_messages;
            free_messages = msg;
        }
    }
    
    printk("[ipc] IPC initialized with %d message slots.\n", MAX_MESSAGES);
}

static struct message *alloc_message(void) {
    if (!free_messages) {
        return 0; // No free messages
    }
    
    struct message *msg = free_messages;
    free_messages = free_messages->next;
    msg->next = 0;
    return msg;
}

static void free_message(struct message *msg) {
    if (!msg) return;
    
    msg->next = free_messages;
    free_messages = msg;
}

int send_message(int receiver_pid, const void *data, int size) {
    if (receiver_pid < 0 || receiver_pid >= MAX_TASKS) {
        printk("[ipc] Invalid receiver PID: %d\n", receiver_pid);
        return -1;
    }
    
    if (size > MAX_MESSAGE_SIZE) {
        printk("[ipc] Message too large: %d bytes (max %d)\n", size, MAX_MESSAGE_SIZE);
        return -1;
    }
    
    struct message *msg = alloc_message();
    if (!msg) {
        printk("[ipc] No free message slots\n");
        return -1;
    }
    
    // Fill message
    msg->sender_pid = 0; // TODO: get current task PID
    msg->receiver_pid = receiver_pid;
    msg->size = size;
    
    // Copy data
    char *src = (char *)data;
    char *dst = msg->data;
    for (int i = 0; i < size; i++) {
        dst[i] = src[i];
    }
    
    // Add to receiver's queue
    struct msg_queue *queue = &message_queues[receiver_pid];
    if (!queue->head) {
        queue->head = queue->tail = msg;
    } else {
        queue->tail->next = msg;
        queue->tail = msg;
    }
    queue->count++;
    
    printk("[ipc] Message sent to PID %d (%d bytes)\n", receiver_pid, size);
    return 0;
}

int recv_message(int sender_pid, void *buffer, int max_size) {
    // For now, receive from any sender (sender_pid ignored)
    int current_pid = 0; // TODO: get current task PID
    struct msg_queue *queue = &message_queues[current_pid];
    
    if (!queue->head) {
        // No messages available - in a real system, we'd block here
        return -1;
    }
    
    struct message *msg = queue->head;
    queue->head = msg->next;
    if (!queue->head) {
        queue->tail = 0;
    }
    queue->count--;
    
    // Copy data to buffer
    int copy_size = (msg->size < max_size) ? msg->size : max_size;
    char *src = msg->data;
    char *dst = (char *)buffer;
    for (int i = 0; i < copy_size; i++) {
        dst[i] = src[i];
    }
    
    printk("[ipc] Message received from PID %d (%d bytes)\n", 
           msg->sender_pid, copy_size);
    
    int actual_size = msg->size;
    free_message(msg);
    
    return actual_size;
}

int try_recv_message(int sender_pid, void *buffer, int max_size) {
    // Non-blocking version of recv_message
    return recv_message(sender_pid, buffer, max_size);
}
