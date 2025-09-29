#include "../kernal/syscall.h"
#include "../kernal/printk.h"
#include <stddef.h>
#include <stdint.h>

// Network server - provides TCP/UDP socket services to applications
// This is a simplified implementation demonstrating the concept

static int NET_SERVER_PID = 4; // Network server gets PID 4
#define MAX_SOCKETS 32
#define MAX_CONNECTIONS 16

// Socket types
typedef enum {
    SOCK_TCP = 1,
    SOCK_UDP = 2
} socket_type_t;

// Socket states
typedef enum {
    SOCK_CLOSED = 0,
    SOCK_LISTENING,
    SOCK_CONNECTED,
    SOCK_BOUND
} socket_state_t;

// Socket structure
struct socket {
    int sock_id;
    int owner_pid;
    socket_type_t type;
    socket_state_t state;
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint8_t rx_buffer[1024];
    int rx_head, rx_tail, rx_count;
    int in_use;
};

// Network server state
struct net_server {
    int server_pid;
    struct socket sockets[MAX_SOCKETS];
    int next_socket_id;
    int active;
};

static struct net_server netserv;

// Initialize network server
void net_server_init(void) {
    netserv.server_pid = syscall_get_pid();
    netserv.next_socket_id = 1;
    netserv.active = 1;
    
    // Clear all sockets
    for (int i = 0; i < MAX_SOCKETS; i++) {
        netserv.sockets[i].in_use = 0;
        netserv.sockets[i].state = SOCK_CLOSED;
        netserv.sockets[i].rx_head = 0;
        netserv.sockets[i].rx_tail = 0;
        netserv.sockets[i].rx_count = 0;
    }
    
    printk("[netserv] Network server initialized (PID: %d)\n", netserv.server_pid);
}

// Allocate a new socket
int allocate_socket(int client_pid, socket_type_t type) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!netserv.sockets[i].in_use) {
            struct socket *sock = &netserv.sockets[i];
            sock->sock_id = netserv.next_socket_id++;
            sock->owner_pid = client_pid;
            sock->type = type;
            sock->state = SOCK_CLOSED;
            sock->in_use = 1;
            sock->rx_head = sock->rx_tail = sock->rx_count = 0;
            
            printk("[netserv] Allocated socket %d for PID %d (type %d)\n", 
                   sock->sock_id, client_pid, type);
            
            return sock->sock_id;
        }
    }
    return -1; // No free sockets
}

// Find socket by ID
struct socket *find_socket(int sock_id) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (netserv.sockets[i].in_use && netserv.sockets[i].sock_id == sock_id) {
            return &netserv.sockets[i];
        }
    }
    return NULL;
}

// Close socket
void close_socket(int sock_id) {
    struct socket *sock = find_socket(sock_id);
    if (sock) {
        sock->in_use = 0;
        sock->state = SOCK_CLOSED;
        printk("[netserv] Closed socket %d\n", sock_id);
    }
}

// Bind socket to address/port
int bind_socket(int sock_id, uint32_t ip, uint16_t port) {
    struct socket *sock = find_socket(sock_id);
    if (!sock) return -1;
    
    sock->local_ip = ip;
    sock->local_port = port;
    sock->state = SOCK_BOUND;
    
    printk("[netserv] Bound socket %d to %u.%u.%u.%u:%u\n", 
           sock_id, 
           (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, 
           (ip >> 8) & 0xFF, ip & 0xFF, 
           port);
    
    return 0;
}

// Listen on socket (TCP only)
int listen_socket(int sock_id, int backlog) {
    struct socket *sock = find_socket(sock_id);
    if (!sock || sock->type != SOCK_TCP) return -1;
    
    sock->state = SOCK_LISTENING;
    printk("[netserv] Socket %d listening (backlog: %d)\n", sock_id, backlog);
    
    return 0;
}

// Connect socket (TCP only)
int connect_socket(int sock_id, uint32_t remote_ip, uint16_t remote_port) {
    struct socket *sock = find_socket(sock_id);
    if (!sock || sock->type != SOCK_TCP) return -1;
    
    sock->remote_ip = remote_ip;
    sock->remote_port = remote_port;
    sock->state = SOCK_CONNECTED;
    
    printk("[netserv] Socket %d connected to %u.%u.%u.%u:%u\n", 
           sock_id,
           (remote_ip >> 24) & 0xFF, (remote_ip >> 16) & 0xFF, 
           (remote_ip >> 8) & 0xFF, remote_ip & 0xFF, 
           remote_port);
    
    return 0;
}

// Send data through socket
int send_socket_data(int sock_id, const void *data, int len) {
    struct socket *sock = find_socket(sock_id);
    if (!sock || sock->state != SOCK_CONNECTED) return -1;
    
    // For now, just send via network driver
    int result = syscall_net_send(data, len);
    
    printk("[netserv] Sent %d bytes through socket %d\n", len, sock_id);
    
    return result;
}

// Receive data from socket
int recv_socket_data(int sock_id, void *buffer, int max_len) {
    struct socket *sock = find_socket(sock_id);
    if (!sock) return -1;
    
    // Check if we have data in the socket's buffer
    if (sock->rx_count == 0) {
        return 0; // No data available
    }
    
    int bytes_to_copy = (sock->rx_count < max_len) ? sock->rx_count : max_len;
    uint8_t *buf = (uint8_t*)buffer;
    
    for (int i = 0; i < bytes_to_copy; i++) {
        buf[i] = sock->rx_buffer[sock->rx_tail];
        sock->rx_tail = (sock->rx_tail + 1) % sizeof(sock->rx_buffer);
        sock->rx_count--;
    }
    
    printk("[netserv] Received %d bytes from socket %d\n", bytes_to_copy, sock_id);
    
    return bytes_to_copy;
}

// Add received data to socket buffer
void socket_add_rx_data(struct socket *sock, const void *data, int len) {
    const uint8_t *src = (const uint8_t*)data;
    
    for (int i = 0; i < len && sock->rx_count < sizeof(sock->rx_buffer); i++) {
        sock->rx_buffer[sock->rx_head] = src[i];
        sock->rx_head = (sock->rx_head + 1) % sizeof(sock->rx_buffer);
        sock->rx_count++;
    }
}

// Process network packets and route to appropriate sockets
void process_network_packets(void) {
    uint8_t packet_buffer[1500];
    int packet_len = syscall_net_recv(packet_buffer, sizeof(packet_buffer));
    
    if (packet_len > 0) {
        printk("[netserv] Received network packet: %d bytes\n", packet_len);
        
        // Simple packet routing - in a real implementation, this would
        // parse IP/TCP/UDP headers and route to the correct socket
        // For now, just add to the first connected socket
        for (int i = 0; i < MAX_SOCKETS; i++) {
            if (netserv.sockets[i].in_use && 
                netserv.sockets[i].state == SOCK_CONNECTED) {
                socket_add_rx_data(&netserv.sockets[i], packet_buffer, packet_len);
                break;
            }
        }
    }
}

// Handle messages from clients
void net_handle_client_message(int sender_pid, char *msg_data, int msg_size) {
    if (msg_size < 4) return;
    
    int *cmd = (int*)msg_data;
    int response = -1;
    
    switch (*cmd) {
        case 1: // socket() - create socket
            if (msg_size >= 8) {
                int type = ((int*)msg_data)[1];
                response = allocate_socket(sender_pid, (socket_type_t)type);
            }
            break;
            
        case 2: // bind() - bind socket
            if (msg_size >= 16) {
                int *args = (int*)msg_data;
                response = bind_socket(args[1], args[2], args[3]);
            }
            break;
            
        case 3: // listen() - listen on socket
            if (msg_size >= 12) {
                int *args = (int*)msg_data;
                response = listen_socket(args[1], args[2]);
            }
            break;
            
        case 4: // connect() - connect socket
            if (msg_size >= 16) {
                int *args = (int*)msg_data;
                response = connect_socket(args[1], args[2], args[3]);
            }
            break;
            
        case 5: // send() - send data
            if (msg_size >= 16) {
                int *args = (int*)msg_data;
                int sock_id = args[1];
                int len = args[2];
                void *data = &args[3];
                response = send_socket_data(sock_id, data, len);
            }
            break;
            
        case 6: // recv() - receive data
            if (msg_size >= 12) {
                int *args = (int*)msg_data;
                int sock_id = args[1];
                int max_len = args[2];
                
                uint8_t recv_buffer[1024];
                int received = recv_socket_data(sock_id, recv_buffer, 
                                              (max_len < sizeof(recv_buffer)) ? max_len : sizeof(recv_buffer));
                
                if (received > 0) {
                    syscall_send_msg(sender_pid, recv_buffer, received);
                    return; // Don't send the response integer
                }
                response = received;
            }
            break;
            
        case 7: // close() - close socket
            if (msg_size >= 8) {
                int sock_id = ((int*)msg_data)[1];
                close_socket(sock_id);
                response = 0;
            }
            break;
            
        default:
            printk("[netserv] Unknown command from PID %d: %d\n", sender_pid, *cmd);
            break;
    }
    
    // Send response back to client
    syscall_send_msg(sender_pid, &response, sizeof(response));
}

// Main network server loop
void net_server_main(void) {
    net_server_init();
    
    printk("[netserv] Network server starting main loop\n");
    
    char msg_buffer[256];
    int loop_count = 0;
    
    while (netserv.active) {
        loop_count++;
        
        // Debug output every 1000 iterations
        if (loop_count % 1000 == 0) {
            printk("[netserv] Server loop iteration %d\n", loop_count);
        }
        
        // Check for incoming messages from clients
        int msg_size = syscall_recv_msg(-1, msg_buffer, sizeof(msg_buffer)); // -1 = any sender
        
        if (msg_size > 0) {
            printk("[netserv] Received message of %d bytes\n", msg_size);
            int sender_pid = ((int*)msg_buffer)[0]; // First int is sender PID
            net_handle_client_message(sender_pid, msg_buffer + 4, msg_size - 4);
        }
        
        // Process incoming network packets
        process_network_packets();
        
        // Yield to other tasks
        syscall_yield();
    }
    
    printk("[netserv] Network server shutting down\n");
}

// Client API functions for socket operations
int net_socket(socket_type_t type) {
    int cmd_data[3];
    cmd_data[0] = syscall_get_pid(); // sender PID
    cmd_data[1] = 1; // socket command
    cmd_data[2] = type;
    
    // Send with retry logic
    int retries = 3;
    while (retries > 0) {
        if (syscall_send_msg(NET_SERVER_PID, cmd_data, sizeof(cmd_data)) >= 0) {
            break;
        }
        retries--;
        for (volatile int i = 0; i < 1000; i++); // delay
    }
    
    if (retries == 0) {
        printk("[net-client] Failed to send socket request after retries\n");
        return -1;
    }
    
    int sock_id = -1;
    syscall_recv_msg(NET_SERVER_PID, &sock_id, sizeof(sock_id));
    
    return sock_id;
}

int net_bind(int sock_id, uint32_t ip, uint16_t port) {
    int cmd_data[4];
    cmd_data[0] = syscall_get_pid();
    cmd_data[1] = 2; // bind command
    cmd_data[2] = sock_id;
    cmd_data[3] = (ip << 16) | port; // pack IP and port
    
    // Send with retry logic
    int retries = 3;
    while (retries > 0) {
        if (syscall_send_msg(NET_SERVER_PID, cmd_data, sizeof(cmd_data)) >= 0) {
            break;
        }
        retries--;
        for (volatile int i = 0; i < 1000; i++); // delay
    }
    
    if (retries == 0) {
        printk("[net-client] Failed to send bind request after retries\n");
        return -1;
    }
    
    int result = -1;
    syscall_recv_msg(NET_SERVER_PID, &result, sizeof(result));
    
    return result;
}

int net_socket_send(int sock_id, const void *data, int len) {
    // Simplified - in reality would need to handle larger data
    int cmd_data[64]; // enough for small messages
    cmd_data[0] = syscall_get_pid();
    cmd_data[1] = 5; // send command
    cmd_data[2] = sock_id;
    cmd_data[3] = len;
    
    // Copy data (limited by buffer size)
    int copy_len = (len < 240) ? len : 240; // leave room for header
    for (int i = 0; i < copy_len; i++) {
        ((char*)&cmd_data[4])[i] = ((const char*)data)[i];
    }
    
    // Send with retry logic
    int retries = 3;
    while (retries > 0) {
        if (syscall_send_msg(NET_SERVER_PID, cmd_data, 16 + copy_len) >= 0) {
            break;
        }
        retries--;
        for (volatile int i = 0; i < 1000; i++); // delay
    }
    
    if (retries == 0) {
        printk("[net-client] Failed to send socket send request after retries\n");
        return -1;
    }
    
    int result = -1;
    syscall_recv_msg(NET_SERVER_PID, &result, sizeof(result));
    
    return result;
}

// Get the network server PID
int net_get_server_pid(void) {
    return NET_SERVER_PID;
}
