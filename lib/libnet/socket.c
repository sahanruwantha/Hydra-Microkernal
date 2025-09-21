#include "libnet.h"
#include <stddef.h>

// Forward declarations for syscalls
extern int syscall_net_send(const void *data, int size);
extern int syscall_net_recv(void *buffer, int max_size);
extern int syscall_send_msg(int receiver_pid, const void *data, int size);
extern int syscall_recv_msg(int sender_pid, void *buffer, int max_size);

// Socket descriptor structure
struct socket_info {
    int type;           // SOCK_STREAM or SOCK_DGRAM
    int state;          // Socket state
    uint16_t local_port;
    uint32_t local_addr;
    uint16_t remote_port;
    uint32_t remote_addr;
};

// Socket states
#define SOCK_CLOSED     0
#define SOCK_BOUND      1
#define SOCK_LISTENING  2
#define SOCK_CONNECTED  3

// Maximum number of sockets
#define MAX_SOCKETS 16
static struct socket_info sockets[MAX_SOCKETS];

// Network server PID (assuming it's known)
#define NET_SERVER_PID 2

// Socket creation
int socket(int domain, int type, int protocol) {
    if (domain != AF_INET) {
        return -1; // Only IPv4 supported
    }
    
    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        return -1; // Only TCP and UDP supported
    }
    
    // Find free socket slot
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].state == SOCK_CLOSED) {
            sockets[i].type = type;
            sockets[i].state = SOCK_CLOSED;
            sockets[i].local_port = 0;
            sockets[i].local_addr = 0;
            sockets[i].remote_port = 0;
            sockets[i].remote_addr = 0;
            return i + 1; // Return socket descriptor (1-based)
        }
    }
    
    return -1; // No free sockets
}

// Bind socket to address
int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    if (sockets[idx].state == SOCK_CLOSED) {
        return -1;
    }
    
    const struct sockaddr_in *addr_in = (const struct sockaddr_in *)addr;
    if (addr_in->sin_family != AF_INET) {
        return -1;
    }
    
    sockets[idx].local_port = addr_in->sin_port;
    sockets[idx].local_addr = addr_in->sin_addr;
    sockets[idx].state = SOCK_BOUND;
    
    return 0;
}

// Listen for connections (TCP only)
int listen(int sockfd, int backlog) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    if (sockets[idx].state != SOCK_BOUND || sockets[idx].type != SOCK_STREAM) {
        return -1;
    }
    
    sockets[idx].state = SOCK_LISTENING;
    return 0;
}

// Accept connection (TCP only)
int accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    if (sockets[idx].state != SOCK_LISTENING) {
        return -1;
    }
    
    // This is a simplified implementation
    // In a real system, this would block until a connection arrives
    
    // Find a new socket for the accepted connection
    int new_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (new_sockfd < 0) {
        return -1;
    }
    
    int new_idx = new_sockfd - 1;
    sockets[new_idx].state = SOCK_CONNECTED;
    sockets[new_idx].local_port = sockets[idx].local_port;
    sockets[new_idx].local_addr = sockets[idx].local_addr;
    
    return new_sockfd;
}

// Connect to remote address
int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    if (sockets[idx].state == SOCK_CLOSED) {
        return -1;
    }
    
    const struct sockaddr_in *addr_in = (const struct sockaddr_in *)addr;
    if (addr_in->sin_family != AF_INET) {
        return -1;
    }
    
    sockets[idx].remote_port = addr_in->sin_port;
    sockets[idx].remote_addr = addr_in->sin_addr;
    sockets[idx].state = SOCK_CONNECTED;
    
    return 0;
}

// Send data
int send(int sockfd, const void *buf, size_t len, int flags) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    if (sockets[idx].state != SOCK_CONNECTED) {
        return -1;
    }
    
    // Send to network server via IPC
    return syscall_send_msg(NET_SERVER_PID, buf, len);
}

// Receive data
int recv(int sockfd, void *buf, size_t len, int flags) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    if (sockets[idx].state != SOCK_CONNECTED) {
        return -1;
    }
    
    // Receive from network server via IPC
    return syscall_recv_msg(NET_SERVER_PID, buf, len);
}

// Send to specific address (UDP)
int sendto(int sockfd, const void *buf, size_t len, int flags,
           const struct sockaddr *dest_addr, uint32_t addrlen) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    if (sockets[idx].type != SOCK_DGRAM) {
        return -1;
    }
    
    // For UDP, we can send to any address
    return syscall_send_msg(NET_SERVER_PID, buf, len);
}

// Receive from specific address (UDP)
int recvfrom(int sockfd, void *buf, size_t len, int flags,
             struct sockaddr *src_addr, uint32_t *addrlen) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    if (sockets[idx].type != SOCK_DGRAM) {
        return -1;
    }
    
    // Receive from network server via IPC
    return syscall_recv_msg(NET_SERVER_PID, buf, len);
}

// Close socket
int close_user_socket(int sockfd) {
    if (sockfd < 1 || sockfd > MAX_SOCKETS) {
        return -1;
    }
    
    int idx = sockfd - 1;
    sockets[idx].state = SOCK_CLOSED;
    return 0;
}

// Utility functions

// Convert IP address from string to binary
uint32_t inet_addr(const char *cp) {
    uint32_t addr = 0;
    int parts[4];
    int part_count = 0;
    int current_part = 0;
    
    // Parse dotted decimal notation
    while (*cp && part_count < 4) {
        if (*cp >= '0' && *cp <= '9') {
            current_part = current_part * 10 + (*cp - '0');
        } else if (*cp == '.') {
            parts[part_count++] = current_part;
            current_part = 0;
        } else {
            return 0; // Invalid character
        }
        cp++;
    }
    
    if (part_count == 3) {
        parts[part_count] = current_part;
        
        // Build 32-bit address
        addr = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    }
    
    return addr;
}

// Convert IP address from binary to string
char *inet_ntoa(uint32_t addr) {
    static char str[16];
    
    int a = (addr >> 24) & 0xFF;
    int b = (addr >> 16) & 0xFF;
    int c = (addr >> 8) & 0xFF;  
    int d = addr & 0xFF;
    
    // Simple sprintf replacement
    str[0] = '0' + (a / 100);
    str[1] = '0' + ((a / 10) % 10);
    str[2] = '0' + (a % 10);
    str[3] = '.';
    str[4] = '0' + (b / 100);
    str[5] = '0' + ((b / 10) % 10);
    str[6] = '0' + (b % 10);
    str[7] = '.';
    str[8] = '0' + (c / 100);
    str[9] = '0' + ((c / 10) % 10);
    str[10] = '0' + (c % 10);
    str[11] = '.';
    str[12] = '0' + (d / 100);
    str[13] = '0' + ((d / 10) % 10);
    str[14] = '0' + (d % 10);
    str[15] = '\0';
    
    return str;
}

// Byte order conversion functions
uint16_t htons(uint16_t hostshort) {
    return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
}

uint16_t ntohs(uint16_t netshort) {
    return htons(netshort); // Same operation
}

uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) | 
           (((hostlong >> 8) & 0xFF) << 16) |
           (((hostlong >> 16) & 0xFF) << 8) |
           ((hostlong >> 24) & 0xFF);
}

uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong); // Same operation
}
