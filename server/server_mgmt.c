#include "servers.h"
#include "../kernal/syscall.h"
#include "../kernal/printk.h"

// Global server PID storage
static int g_bullet_server_pid = -1;
static int g_net_server_pid = -1;

// Server management functions

void start_servers(void) {
    printk("[servers] Starting server processes...\n");
    
    // Create bullet server task
    int bullet_pid = syscall_create_task(bullet_server_main);
    if (bullet_pid >= 0) {
        g_bullet_server_pid = bullet_pid;
        printk("[servers] Bullet server started (PID: %d)\n", bullet_pid);
    } else {
        printk("[servers] Failed to start bullet server\n");
    }
    
    // Create network server task
    int net_pid = syscall_create_task(net_server_main);
    if (net_pid >= 0) {
        g_net_server_pid = net_pid;
        printk("[servers] Network server started (PID: %d)\n", net_pid);
    } else {
        printk("[servers] Failed to start network server\n");
    }
    
    printk("[servers] All servers started\n");
}

// Demo function to test server functionality
void server_demo(void) {
    printk("[demo] Starting server demonstration...\n");
    
    // Test bullet server
    printk("[demo] Testing bullet server...\n");
    char test_data[] = "Test process data";
    int req_id = bullet_migrate_process(2, test_data, sizeof(test_data));
    printk("[demo] Migration request ID: %d\n", req_id);
    
    // Wait a bit and check status - increased delay to reduce message frequency
    for (volatile int i = 0; i < 50000; i++); // increased delay
    int status = bullet_check_migration_status(req_id);
    printk("[demo] Migration status: %d\n", status);
    
    // Test network server with rate limiting
    printk("[demo] Testing network server...\n");
    int sock = net_socket(SOCK_TCP);
    printk("[demo] Created TCP socket: %d\n", sock);
    
    if (sock > 0) {
        int bind_result = net_bind(sock, 0x7F000001, 8080); // 127.0.0.1:8080
        if (bind_result >= 0) {
            printk("[demo] Bind result: %d\n", bind_result);
            
            char msg[] = "Hello, Network!";
            int send_result = net_socket_send(sock, msg, sizeof(msg));
            printk("[demo] Send result: %d\n", send_result);
        } else {
            printk("[demo] Bind failed: %d\n", bind_result);
        }
    }
    
    printk("[demo] Server demonstration completed\n");
}

// Test client for demonstrating server usage
void test_client_main(void) {
    int my_pid = syscall_get_pid();
    printk("[client] Test client started (PID: %d)\n", my_pid);
    
    // Give servers more time to start and initialize
    for (volatile int i = 0; i < 100000; i++);
    
    // Run the demo once - avoid repeated testing that exhausts message slots
    server_demo();
    
    // Add a longer delay before potential exit to let messages clear
    for (volatile int i = 0; i < 50000; i++);
    
    printk("[client] Test client finished\n");
}
