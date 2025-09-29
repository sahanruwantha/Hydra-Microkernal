#include "../kernal/syscall.h"
#include "../kernal/printk.h"
#include <stddef.h>

// Bullet server - handles process migration
// This is a simplified implementation demonstrating the concept

static int BULLET_SERVER_PID = 3; // Bullet server gets PID 3
#define MAX_MIGRATION_REQUESTS 16

// Migration request structure
struct migration_request {
    int source_pid;
    int target_node;
    int process_size;
    void *process_data;
    int status; // 0=pending, 1=in_progress, 2=completed, -1=failed
};

// Bullet server state
struct bullet_server {
    int server_pid;
    struct migration_request requests[MAX_MIGRATION_REQUESTS];
    int num_requests;
    int active;
};

static struct bullet_server bullet;

// Initialize bullet server
void bullet_init(void) {
    bullet.server_pid = syscall_get_pid();
    bullet.num_requests = 0;
    bullet.active = 1;
    
    printk("[bullet] Bullet server initialized (PID: %d)\n", bullet.server_pid);
}

// Handle migration request
int handle_migration_request(int source_pid, int target_node, void *process_data, int size) {
    if (bullet.num_requests >= MAX_MIGRATION_REQUESTS) {
        printk("[bullet] Migration request queue full\n");
        return -1;
    }
    
    struct migration_request *req = &bullet.requests[bullet.num_requests];
    req->source_pid = source_pid;
    req->target_node = target_node;
    req->process_data = process_data;
    req->process_size = size;
    req->status = 0; // pending
    
    bullet.num_requests++;
    
    printk("[bullet] Migration request queued: PID %d -> Node %d (%d bytes)\n", 
           source_pid, target_node, size);
    
    return bullet.num_requests - 1; // return request ID
}

// Process migration requests
void process_migrations(void) {
    for (int i = 0; i < bullet.num_requests; i++) {
        struct migration_request *req = &bullet.requests[i];
        
        if (req->status == 0) { // pending
            printk("[bullet] Processing migration: PID %d -> Node %d\n", 
                   req->source_pid, req->target_node);
            
            req->status = 1; // in progress
            
            // Simulate migration process
            // In a real implementation, this would:
            // 1. Checkpoint the process state
            // 2. Send process image over network
            // 3. Start process on target node
            // 4. Clean up source process
            
            // For demo, we'll just simulate success
            req->status = 2; // completed
            
            printk("[bullet] Migration completed: PID %d -> Node %d\n", 
                   req->source_pid, req->target_node);
        }
    }
}

// Handle messages from clients
void bullet_handle_client_message(int sender_pid, char *msg_data, int msg_size) {
    if (msg_size < 4) return;
    
    int *cmd = (int*)msg_data;
    
    switch (*cmd) {
        case 1: // Migration request
            if (msg_size >= 16) {
                int *args = (int*)msg_data;
                int target_node = args[1];
                int data_size = args[2];
                void *data = (void*)&args[3];
                
                int req_id = handle_migration_request(sender_pid, target_node, data, data_size);
                
                // Send response back to client with error handling
                if (syscall_send_msg(sender_pid, &req_id, sizeof(req_id)) < 0) {
                    printk("[bullet] Failed to send migration response to PID %d\n", sender_pid);
                }
            }
            break;
            
        case 2: // Migration status query
            if (msg_size >= 8) {
                int req_id = ((int*)msg_data)[1];
                if (req_id >= 0 && req_id < bullet.num_requests) {
                    int status = bullet.requests[req_id].status;
                    if (syscall_send_msg(sender_pid, &status, sizeof(status)) < 0) {
                        printk("[bullet] Failed to send status response to PID %d\n", sender_pid);
                    }
                } else {
                    int error_status = -1;
                    syscall_send_msg(sender_pid, &error_status, sizeof(error_status));
                }
            }
            break;
            
        default:
            printk("[bullet] Unknown command from PID %d: %d\n", sender_pid, *cmd);
            break;
    }
}

// Main bullet server loop
void bullet_server_main(void) {
    bullet_init();
    
    printk("[bullet] Bullet server starting main loop\n");
    
    char msg_buffer[256];
    int loop_count = 0;
    
    while (bullet.active) {
        loop_count++;
        
        // Debug output every 1000 iterations
        if (loop_count % 1000 == 0) {
            printk("[bullet] Server loop iteration %d\n", loop_count);
        }
        
        // Check for incoming messages
        int msg_size = syscall_recv_msg(-1, msg_buffer, sizeof(msg_buffer)); // -1 = any sender
        
        if (msg_size > 0) {
            printk("[bullet] Received message of %d bytes\n", msg_size);
            int sender_pid = ((int*)msg_buffer)[0]; // First int is sender PID
            bullet_handle_client_message(sender_pid, msg_buffer + 4, msg_size - 4);
        }
        
        // Process pending migrations
        process_migrations();
        
        // Yield to other tasks
        syscall_yield();
    }
    
    printk("[bullet] Bullet server shutting down\n");
}

// API functions for clients to use bullet server
int bullet_migrate_process(int target_node, void *process_data, int size) {
    int cmd_data[4];
    cmd_data[0] = syscall_get_pid(); // sender PID
    cmd_data[1] = 1; // migration request command
    cmd_data[2] = target_node;
    cmd_data[3] = size;
    
    // Send request to bullet server with retry logic
    int retries = 3;
    while (retries > 0) {
        if (syscall_send_msg(BULLET_SERVER_PID, cmd_data, sizeof(cmd_data)) >= 0) {
            break;
        }
        retries--;
        // Simple delay before retry
        for (volatile int i = 0; i < 1000; i++);
    }
    
    if (retries == 0) {
        printk("[bullet-client] Failed to send migration request after retries\n");
        return -1;
    }
    
    // Wait for response
    int req_id = -1;
    syscall_recv_msg(BULLET_SERVER_PID, &req_id, sizeof(req_id));
    
    return req_id;
}

int bullet_check_migration_status(int req_id) {
    int cmd_data[3];
    cmd_data[0] = syscall_get_pid(); // sender PID
    cmd_data[1] = 2; // status query command
    cmd_data[2] = req_id;
    
    // Send request to bullet server with retry logic
    int retries = 3;
    while (retries > 0) {
        if (syscall_send_msg(BULLET_SERVER_PID, cmd_data, sizeof(cmd_data)) >= 0) {
            break;
        }
        retries--;
        // Simple delay before retry
        for (volatile int i = 0; i < 1000; i++);
    }
    
    if (retries == 0) {
        printk("[bullet-client] Failed to send status request after retries\n");
        return -1;
    }
    
    // Wait for response
    int status = -1;
    syscall_recv_msg(BULLET_SERVER_PID, &status, sizeof(status));
    
    return status;
}

// Get the bullet server PID
int bullet_get_server_pid(void) {
    return BULLET_SERVER_PID;
}
