#include "hydra.h"

// Network server PID (assuming it's known)
#define NET_SERVER_PID 2

// Network protocol messages
struct net_bind_request {
    uint16_t port;
};

struct net_connect_request {
    uint32_t address;
    uint16_t port;
};

struct net_send_request {
    int socket_id;
    int data_size;
    char data[200];  // Remaining space in IPC message
};

struct net_receive_request {
    int socket_id;
    int max_size;
};

struct net_response {
    int status;
    int socket_id;
    int data_size;
    char data[200];
};

int net_service_bind(uint16_t port) {
    struct net_bind_request req;
    req.port = port;
    
    struct net_response resp;
    int result = send_request(NET_SERVER_PID, &req, sizeof(req),
                             &resp, sizeof(resp));
    
    if (result > 0) {
        return resp.socket_id;
    }
    
    return -1;
}

int net_service_connect(uint32_t addr, uint16_t port) {
    struct net_connect_request req;
    req.address = addr;
    req.port = port;
    
    struct net_response resp;
    int result = send_request(NET_SERVER_PID, &req, sizeof(req),
                             &resp, sizeof(resp));
    
    if (result > 0) {
        return resp.socket_id;
    }
    
    return -1;
}

int net_service_send(int socket_id, const void *data, size_t size) {
    if (size > 200) {
        return -1; // Data too large for IPC message
    }
    
    struct net_send_request req;
    req.socket_id = socket_id;
    req.data_size = size;
    
    // Copy data
    const char *src = (const char *)data;
    for (size_t i = 0; i < size; i++) {
        req.data[i] = src[i];
    }
    
    struct net_response resp;
    int result = send_request(NET_SERVER_PID, &req, sizeof(req),
                             &resp, sizeof(resp));
    
    if (result > 0) {
        return resp.status;
    }
    
    return -1;
}

int net_service_receive(int socket_id, void *buffer, size_t max_size) {
    struct net_receive_request req;
    req.socket_id = socket_id;
    req.max_size = (max_size > 200) ? 200 : max_size;
    
    struct net_response resp;
    int result = send_request(NET_SERVER_PID, &req, sizeof(req),
                             &resp, sizeof(resp));
    
    if (result > 0 && resp.data_size > 0) {
        // Copy received data
        char *dest = (char *)buffer;
        size_t copy_size = (resp.data_size < max_size) ? resp.data_size : max_size;
        for (size_t i = 0; i < copy_size; i++) {
            dest[i] = resp.data[i];
        }
        return copy_size;
    }
    
    return -1;
}
