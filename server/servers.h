#ifndef SERVERS_H
#define SERVERS_H

#include <stdint.h>

// Bullet server API
int bullet_migrate_process(int target_node, void *process_data, int size);
int bullet_check_migration_status(int req_id);
int bullet_get_server_pid(void);
void bullet_server_main(void);

// Network server API
typedef enum {
    SOCK_TCP = 1,
    SOCK_UDP = 2
} socket_type_t;

int net_socket(socket_type_t type);
int net_bind(int sock_id, uint32_t ip, uint16_t port);
int net_socket_send(int sock_id, const void *data, int len);
int net_get_server_pid(void);
void net_server_main(void);

// Server management
void start_servers(void);
void server_demo(void);

#endif
