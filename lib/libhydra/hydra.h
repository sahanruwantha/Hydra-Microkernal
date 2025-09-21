#ifndef HYDRA_H
#define HYDRA_H

#include <stdint.h>
#include <stddef.h>

// Process IDs
typedef int pid_t;

// IPC message structure
struct ipc_message {
    int sender_pid;
    int receiver_pid;
    int message_type;
    int data_size;
    char data[256];  // Maximum message size
};

// Message types
#define MSG_TYPE_DATA       1
#define MSG_TYPE_REQUEST    2
#define MSG_TYPE_RESPONSE   3
#define MSG_TYPE_NOTIFY     4

// Process management
pid_t get_pid(void);
int create_process(void (*entry_point)(void));
void yield_process(void);
void exit_process(void);

// IPC functions
int ipc_send_message(pid_t receiver, int msg_type, const void *data, size_t size);
int ipc_receive_message(pid_t sender, int *msg_type, void *buffer, size_t max_size);
int send_request(pid_t server, const void *request, size_t req_size, 
                 void *response, size_t max_resp_size);

// Capability-based security (placeholder)
typedef uint64_t capability_t;
capability_t create_capability(pid_t process, int permissions);
int check_capability(capability_t cap, int operation);

// Bullet client functions (process migration)
int client_bullet_migrate_process(pid_t process, const char *target_node);
int client_bullet_get_migration_status(pid_t process);

// Network service functions  
int net_service_bind(uint16_t port);
int net_service_connect(uint32_t addr, uint16_t port);
int net_service_send(int socket_id, const void *data, size_t size);
int net_service_receive(int socket_id, void *buffer, size_t max_size);

#endif // HYDRA_H