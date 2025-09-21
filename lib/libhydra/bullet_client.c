#include "hydra.h"

// Bullet server PID (assuming it's known)
#define BULLET_SERVER_PID 1

// Bullet protocol messages
struct bullet_migrate_request {
    pid_t process_id;
    char target_node[64];
};

struct bullet_status_request {
    pid_t process_id;
};

struct bullet_response {
    int status;
    char message[128];
};

int client_bullet_migrate_process(pid_t process, const char *target_node) {
    struct bullet_migrate_request req;
    req.process_id = process;
    
    // Copy target node name
    int i = 0;
    while (target_node[i] && i < 63) {
        req.target_node[i] = target_node[i];
        i++;
    }
    req.target_node[i] = '\0';
    
    struct bullet_response resp;
    int result = send_request(BULLET_SERVER_PID, &req, sizeof(req), 
                             &resp, sizeof(resp));
    
    if (result > 0) {
        return resp.status;
    }
    
    return -1;
}

int client_bullet_get_migration_status(pid_t process) {
    struct bullet_status_request req;
    req.process_id = process;
    
    struct bullet_response resp;
    int result = send_request(BULLET_SERVER_PID, &req, sizeof(req),
                             &resp, sizeof(resp));
    
    if (result > 0) {
        return resp.status;
    }
    
    return -1;
}