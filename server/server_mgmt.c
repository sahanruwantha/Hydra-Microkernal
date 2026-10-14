#include "servers.h"
#include "../kernel/syscall.h"
#include "../kernel/printk.h"

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

void server_demo(void) {
    printk("[mgmt] Starting server operations...\n");
    
    printk("[mgmt] Processing bullet server request...\n");
    char process_data[] = "Process migration data";
    int req_id = bullet_migrate_process(2, process_data, sizeof(process_data));
    printk("[mgmt] Migration request ID: %d\n", req_id);
    
    for (volatile int i = 0; i < 50000; i++);
    int status = bullet_check_migration_status(req_id);
    printk("[mgmt] Migration status: %d\n", status);
    
    printk("[mgmt] Processing network server request...\n");
    int sock = net_socket(SOCK_TCP);
    printk("[mgmt] Created TCP socket: %d\n", sock);
    
    if (sock > 0) {
        int bind_result = net_bind(sock, 0x7F000001, 8080);
        if (bind_result >= 0) {
            printk("[mgmt] Bind result: %d\n", bind_result);
            
            char msg[] = "Network communication data";
            int send_result = net_socket_send(sock, msg, sizeof(msg));
            printk("[mgmt] Send result: %d\n", send_result);
        } else {
            printk("[mgmt] Bind failed: %d\n", bind_result);
        }
    }
    
    printk("[mgmt] Server operations completed\n");
}

void client_main(void) {
    int my_pid = syscall_get_pid();
    printk("[client] Client process started (PID: %d)\n", my_pid);
    
    for (volatile int i = 0; i < 100000; i++);
    
    server_demo();
    
    for (volatile int i = 0; i < 50000; i++);
    
    printk("[client] Client process finished\n");
}
