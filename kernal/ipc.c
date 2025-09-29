#include "ipc.h"
#include "printk.h"
#include "mm.h"
#include "sched.h"

extern int current_task; // Defined in sched.c

static struct msg_queue message_queues[MAX_TASKS];
static struct message *free_messages = 0;

// Debug function to count free messages
static int count_free_messages(void) {
    int count = 0;
    struct message *current = free_messages;
    while (current) {
        count++;
        current = current->next;
    }
    return count;
}

void ipc_init(void) {
    printk("[ipc] Initializing IPC subsystem...\n");
    
    // Initialize message queues
    for (int i = 0; i < MAX_TASKS; i++) {
        message_queues[i].head = 0;
        message_queues[i].tail = 0;
        message_queues[i].count = 0;
    }
    printk("[ipc] Message queues initialized for %d tasks\n", MAX_TASKS);
    
    // Pre-allocate message pool
    int allocated_messages = 0;
    for (int i = 0; i < MAX_MESSAGES; i++) {
        struct message *msg = (struct message *)kmalloc(sizeof(struct message));
        if (msg) {
            msg->next = free_messages;
            free_messages = msg;
            allocated_messages++;
        } else {
            printk("[ipc] Warning: Failed to allocate message %d/%d\n", i + 1, MAX_MESSAGES);
        }
    }
    
    printk("[ipc] IPC initialized: %d/%d message slots allocated (size: %d bytes each)\n", 
           allocated_messages, MAX_MESSAGES, (int)sizeof(struct message));
    
    if (allocated_messages < MAX_MESSAGES) {
        printk("[ipc] Warning: Only %d out of %d message slots were allocated\n", 
               allocated_messages, MAX_MESSAGES);
    }
}

static struct message *alloc_message(void) {
    if (!free_messages) {
        int free_count = count_free_messages();
        printk("[ipc] CRITICAL: Message pool exhausted! (free: %d/%d, all slots in use)\n", 
               free_count, MAX_MESSAGES);
        
        // Show which tasks have the most messages queued
        printk("[ipc] Current queue status:\n");
        for (int i = 0; i < MAX_TASKS; i++) {
            if (message_queues[i].count > 0) {
                printk("[ipc]   Task %d: %d messages pending\n", 
                       i, message_queues[i].count);
            }
        }
        return 0; // No free messages
    }
    
    struct message *msg = free_messages;
    free_messages = free_messages->next;
    msg->next = 0;
    
    // Log allocation details for debugging
    int remaining = count_free_messages();
    if (remaining < 10) {
        printk("[ipc] Low memory warning: Only %d message slots remaining\n", remaining);
    }
    
    return msg;
}

static void free_message(struct message *msg) {
    if (!msg) {
        printk("[ipc] Warning: Attempted to free null message pointer\n");
        return;
    }
    
    // Clear message content for security
    msg->sender_pid = -1;
    msg->receiver_pid = -1;
    msg->size = 0;
    
    // Add back to free list
    msg->next = free_messages;
    free_messages = msg;
    
    // Log if we're recovering from low memory
    int free_count = count_free_messages();
    if (free_count == 10) {  // Just recovered from low memory
        printk("[ipc] Message slots recovering: %d slots now available\n", free_count);
    }
}

int send_message(int receiver_pid, const void *data, int size) {
    printk("[ipc] Send request: PID %d -> PID %d (%d bytes)\n", 
           current_task, receiver_pid, size);
    
    if (receiver_pid < 0 || receiver_pid >= MAX_TASKS) {
        printk("[ipc] ERROR: Invalid receiver PID %d (valid range: 0-%d)\n", 
               receiver_pid, MAX_TASKS - 1);
        return -1;
    }
    
    if (size <= 0) {
        printk("[ipc] ERROR: Invalid message size: %d bytes\n", size);
        return -1;
    }
    
    if (size > MAX_MESSAGE_SIZE) {
        printk("[ipc] ERROR: Message too large: %d bytes (max %d bytes)\n", 
               size, MAX_MESSAGE_SIZE);
        return -1;
    }
    
    if (!data) {
        printk("[ipc] ERROR: Null data pointer in send_message\n");
        return -1;
    }
    
    struct message *msg = alloc_message();
    if (!msg) {
        printk("[ipc] ERROR: Failed to allocate message for PID %d -> PID %d\n", 
               current_task, receiver_pid);
        return -1;
    }
    
    // Fill message
    msg->sender_pid = current_task;
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
    
    printk("[ipc] Message queued: PID %d -> PID %d (%d bytes), queue depth: %d\n", 
           current_task, receiver_pid, size, queue->count);
    
    // Warning for high queue depth
    if (queue->count > 8) {
        printk("[ipc] WARNING: High queue depth for PID %d: %d messages pending\n", 
               receiver_pid, queue->count);
    }
    
    // Critical warning for very high queue depth
    if (queue->count > 20) {
        printk("[ipc] CRITICAL: PID %d queue severely backlogged with %d messages!\n", 
               receiver_pid, queue->count);
    }
    
    return 0;
}

int recv_message(int sender_pid, void *buffer, int max_size) {
    int current_pid = current_task;
    
    printk("[ipc] Receive request: PID %d waiting for message from PID %d (max %d bytes)\n", 
           current_pid, sender_pid, max_size);
    
    if (!buffer) {
        printk("[ipc] ERROR: Null buffer pointer in recv_message\n");
        return -1;
    }
    
    if (max_size <= 0) {
        printk("[ipc] ERROR: Invalid buffer size: %d bytes\n", max_size);
        return -1;
    }
    
    if (current_pid < 0 || current_pid >= MAX_TASKS) {
        printk("[ipc] ERROR: Invalid current task PID: %d\n", current_pid);
        return -1;
    }
    
    struct msg_queue *queue = &message_queues[current_pid];
    
    if (!queue->head) {
        // No messages available - in a real system, we'd block here
        printk("[ipc] No messages available for PID %d (queue empty)\n", current_pid);
        return -1;
    }
    
    struct message *msg = queue->head;
    queue->head = msg->next;
    if (!queue->head) {
        queue->tail = 0;
    }
    queue->count--;
    
    // Validate message integrity
    if (msg->sender_pid < 0 || msg->sender_pid >= MAX_TASKS) {
        printk("[ipc] WARNING: Message with invalid sender PID: %d\n", msg->sender_pid);
    }
    
    if (msg->size <= 0 || msg->size > MAX_MESSAGE_SIZE) {
        printk("[ipc] WARNING: Message with invalid size: %d bytes\n", msg->size);
        free_message(msg);
        return -1;
    }
    
    // Copy data to buffer
    int copy_size = (msg->size < max_size) ? msg->size : max_size;
    char *src = msg->data;
    char *dst = (char *)buffer;
    for (int i = 0; i < copy_size; i++) {
        dst[i] = src[i];
    }
    
    int actual_size = msg->size;
    int sender = msg->sender_pid;
    
    if (copy_size < actual_size) {
        printk("[ipc] WARNING: Message truncated: %d bytes copied, %d bytes total\n", 
               copy_size, actual_size);
    }
    
    printk("[ipc] Message received: PID %d <- PID %d (%d bytes), remaining queue: %d\n", 
           current_pid, sender, copy_size, queue->count);
    
    // Log queue state changes
    if (queue->count == 0) {
        printk("[ipc] Queue empty: PID %d has processed all pending messages\n", current_pid);
    } else if (queue->count < 5 && queue->count > 0) {
        printk("[ipc] Queue draining: PID %d has %d messages remaining\n", 
               current_pid, queue->count);
    }
    
    free_message(msg);
    
    return actual_size;
}

int try_recv_message(int sender_pid, void *buffer, int max_size) {
    // Non-blocking version of recv_message
    printk("[ipc] Non-blocking receive attempt: PID %d\n", current_task);
    int result = recv_message(sender_pid, buffer, max_size);
    
    if (result == -1) {
        printk("[ipc] Non-blocking receive: No messages available for PID %d\n", current_task);
    }
    
    return result;
}

// Debug function to show current IPC status
void ipc_debug_status(void) {
    int free_count = count_free_messages();
    int used_count = MAX_MESSAGES - free_count;
    
    printk("[ipc] ========== IPC SYSTEM STATUS ==========\n");
    printk("[ipc] Memory: %d/%d messages in use, %d free (%.1f%% utilization)\n", 
           used_count, MAX_MESSAGES, free_count, 
           (float)(used_count * 100) / MAX_MESSAGES);
    
    printk("[ipc] Message size: %d bytes each, pool size: %d KB total\n", 
           (int)sizeof(struct message), 
           (int)(MAX_MESSAGES * sizeof(struct message)) / 1024);
    
    // Show queue status for each task
    int total_queued = 0;
    int active_queues = 0;
    
    printk("[ipc] Queue Status:\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (message_queues[i].count > 0) {
            printk("[ipc]   Task %d: %d messages queued\n", 
                   i, message_queues[i].count);
            total_queued += message_queues[i].count;
            active_queues++;
        }
    }
    
    if (active_queues == 0) {
        printk("[ipc]   All queues empty\n");
    } else {
        printk("[ipc] Active queues: %d, Total queued messages: %d\n", 
               active_queues, total_queued);
        
        if (total_queued != used_count) {
            printk("[ipc] WARNING: Queue count mismatch! Queued: %d, Used: %d\n", 
                   total_queued, used_count);
        }
    }
    
    // Performance statistics
    printk("[ipc] Current task: %d\n", current_task);
    
    // Memory health check
    if (free_count < 5) {
        printk("[ipc] ALERT: Critical low memory - only %d slots free!\n", free_count);
    } else if (free_count < 15) {
        printk("[ipc] WARNING: Low memory - %d slots free\n", free_count);
    } else {
        printk("[ipc] Memory status: Healthy\n");
    }
    
    printk("[ipc] ========================================\n");
}

// Additional logging utilities

// Log status of a specific queue
void ipc_log_queue_status(int pid) {
    if (pid < 0 || pid >= MAX_TASKS) {
        printk("[ipc] ERROR: Invalid PID %d for queue status\n", pid);
        return;
    }
    
    struct msg_queue *queue = &message_queues[pid];
    printk("[ipc] Queue status for PID %d: %d messages", pid, queue->count);
    
    if (queue->count > 0) {
        printk(" (head: %p, tail: %p)", (void*)queue->head, (void*)queue->tail);
        
        // Walk the queue to validate integrity
        int actual_count = 0;
        struct message *current = queue->head;
        while (current && actual_count < queue->count + 5) { // Safety limit
            actual_count++;
            current = current->next;
        }
        
        if (actual_count != queue->count) {
            printk(" [CORRUPTION: expected %d, found %d]", queue->count, actual_count);
        }
    }
    
    printk("\n");
}

// Log current memory usage in detail
void ipc_log_memory_usage(void) {
    int free_count = count_free_messages();
    int used_count = MAX_MESSAGES - free_count;
    
    printk("[ipc] Memory Usage Report:\n");
    printk("[ipc]   Total slots: %d\n", MAX_MESSAGES);
    printk("[ipc]   Used slots: %d (%.1f%%)\n", used_count, (float)(used_count * 100) / MAX_MESSAGES);
    printk("[ipc]   Free slots: %d (%.1f%%)\n", free_count, (float)(free_count * 100) / MAX_MESSAGES);
    printk("[ipc]   Slot size: %d bytes\n", (int)sizeof(struct message));
    printk("[ipc]   Total memory: %d KB\n", (int)(MAX_MESSAGES * sizeof(struct message)) / 1024);
    printk("[ipc]   Used memory: %d KB\n", (int)(used_count * sizeof(struct message)) / 1024);
    
    // Memory pressure analysis
    if (free_count < 5) {
        printk("[ipc]   Status: CRITICAL - Memory exhaustion imminent!\n");
    } else if (free_count < 15) {
        printk("[ipc]   Status: WARNING - Low memory condition\n");
    } else if (free_count < 32) {
        printk("[ipc]   Status: CAUTION - Medium memory usage\n");
    } else {
        printk("[ipc]   Status: HEALTHY - Ample memory available\n");
    }
}
