#include "../lib/libc/libc.h"
#include "../lib/libhydra/hydra.h"
#include "../lib/libnet/libnet.h"

void test_libc(void) {
    printf("=== Testing libc ===\n");
    
    // Test string functions
    char str1[64] = "Hello";
    char str2[64] = "World";
    
    printf("str1: '%s', length: %d\n", str1, strlen(str1));
    strcat(str1, " ");
    strcat(str1, str2);
    printf("After concatenation: '%s'\n", str1);
    
    // Test memory allocation
    char *buffer = (char*)malloc(100);
    if (buffer) {
        printf("Allocated 100 bytes at address 0x%x\n", (int)buffer);
        strcpy(buffer, "Dynamic memory test");
        printf("Buffer content: '%s'\n", buffer);
        free(buffer);
        printf("Memory freed\n");
    }
    
    // Test number conversion
    int num = 12345;
    char num_str[32];
    itoa(num, num_str, 10);
    printf("Number %d as string: '%s'\n", num, num_str);
    
    int converted = atoi(num_str);
    printf("String '%s' as number: %d\n", num_str, converted);
}

void test_ipc(void) {
    printf("\n=== Testing libamoeba IPC ===\n");
    
    pid_t my_pid = get_pid();
    printf("My process ID: %d\n", my_pid);
    
    // Test sending a message to ourselves (echo test)
    const char *test_msg = "Hello IPC";
    printf("Sending message: '%s'\n", test_msg);
    
    int result = ipc_send_message(my_pid, MSG_TYPE_DATA, test_msg, strlen(test_msg) + 1);
    if (result >= 0) {
        printf("Message sent successfully\n");
        
        // Try to receive the message
        char recv_buffer[64];
        int msg_type;
        int recv_len = ipc_receive_message(my_pid, &msg_type, recv_buffer, sizeof(recv_buffer));
        
        if (recv_len > 0) {
            printf("Received message (type %d): '%s'\n", msg_type, recv_buffer);
        } else {
            printf("Failed to receive message\n");
        }
    } else {
        printf("Failed to send message\n");
    }
}

void test_networking(void) {
    printf("\n=== Testing libnet ===\n");
    
    // Test socket creation
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        printf("Created TCP socket: %d\n", sock);
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);
        addr.sin_addr = inet_addr("127.0.0.1");
        
        printf("Attempting to bind to 127.0.0.1:8080\n");
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            printf("Bind successful\n");
            
            if (listen(sock, 5) == 0) {
                printf("Listening for connections\n");
            }
        }
        
        close_user_socket(sock);
        printf("Socket closed\n");
    } else {
        printf("Failed to create socket\n");
    }
    
    // Test UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock >= 0) {
        printf("Created UDP socket: %d\n", udp_sock);
        close_user_socket(udp_sock);
        printf("UDP socket closed\n");
    }
}

void library_test_main(void) {
    printf("\n*** Phase 5 - User-space Libraries Test ***\n");
    
    test_libc();
    test_ipc();
    test_networking();
    
    printf("\n*** Library tests completed ***\n");
}

// Export this function for the kernel to call
void phase5_test(void) {
    library_test_main();
}
