#ifndef LIBNET_H
#define LIBNET_H

#include <stdint.h>
#include <stddef.h>

// Socket types
#define SOCK_STREAM 1  // TCP
#define SOCK_DGRAM  2  // UDP

// Address families
#define AF_INET     2

// Socket address structure
struct sockaddr_in {
    uint16_t sin_family;    // Address family (AF_INET)
    uint16_t sin_port;      // Port number (network byte order)
    uint32_t sin_addr;      // IP address (network byte order)
    char sin_zero[8];       // Padding
};

// Generic socket address
struct sockaddr {
    uint16_t sa_family;     // Address family
    char sa_data[14];       // Address data
};

// Network library initialization
void netlib_init(void);
void lwip_init(void);
void lwip_process_packets(void);

// Low-level packet functions
int net_send_packet(const uint8_t *data, uint16_t len);
int net_recv_packet(uint8_t *buffer, uint16_t max_len);
void net_print_stats(void);
int net_is_packet_available(void);

// High-level socket functions
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int send(int sockfd, const void *buf, size_t len, int flags);
int recv(int sockfd, void *buf, size_t len, int flags);
int sendto(int sockfd, const void *buf, size_t len, int flags,
           const struct sockaddr *dest_addr, uint32_t addrlen);
int recvfrom(int sockfd, void *buf, size_t len, int flags,
             struct sockaddr *src_addr, uint32_t *addrlen);
int close_user_socket(int sockfd);

// Utility functions
uint32_t inet_addr(const char *cp);
char *inet_ntoa(uint32_t addr);
uint16_t htons(uint16_t hostshort);
uint16_t ntohs(uint16_t netshort);
uint32_t htonl(uint32_t hostlong);
uint32_t ntohl(uint32_t netlong);

#endif
