#include "syscall.h"
#include "ipc.h"
#include "sched.h"
#include "net_driver.h"
#include "printk.h"
#include <stddef.h>

// Handle system calls from user space
long handle_syscall(long syscall_num, long arg1, long arg2, long arg3, long arg4) {
    static uint32_t syscall_net_send_counter = 0;
    static uint32_t syscall_net_recv_counter = 0;
    
    switch (syscall_num) {
        case SYS_SEND_MSG:
            return send_message((int)arg1, (const void*)arg2, (int)arg3);
            
        case SYS_RECV_MSG:
            return recv_message((int)arg1, (void*)arg2, (int)arg3);
            
        case SYS_NET_SEND: {
            uint32_t current_syscall_id = ++syscall_net_send_counter;
            uint16_t len = (uint16_t)arg2;
            const uint8_t* data = (const uint8_t*)arg1;
            
            printk("[syscall] PACKET_SYSCALL_SEND_START: Syscall #%u - NET_SEND request (%d bytes)\n", 
                   current_syscall_id, len);
            
            // Log caller context if available
            extern int current_task;
            printk("[syscall] PACKET_SYSCALL_SEND_CALLER: Syscall #%u - Called by task %d\n", 
                   current_syscall_id, current_task);
            
            // Log packet preview
            if (len > 0 && data != NULL) {
                printk("[syscall] PACKET_SYSCALL_SEND_DATA: Syscall #%u - First 16 bytes: ", current_syscall_id);
                int preview_len = (len < 16) ? len : 16;
                for (int i = 0; i < preview_len; i++) {
                    printk("%02x ", data[i]);
                }
                printk("\n");
            }
            
            printk("[syscall] PACKET_SYSCALL_SEND_CALLING: Syscall #%u - Forwarding to net_send()\n", 
                   current_syscall_id);
            
            int result = net_send(data, len);
            
            printk("[syscall] PACKET_SYSCALL_SEND_COMPLETE: Syscall #%u - Result: %d\n", 
                   current_syscall_id, result);
            
            return result;
        }
            
        case SYS_NET_RECV: {
            uint32_t current_syscall_id = ++syscall_net_recv_counter;
            
            printk("[syscall] PACKET_SYSCALL_RECV_START: Syscall #%u - NET_RECV request (max %ld bytes)\n", 
                   current_syscall_id, arg2);
            
            extern int current_task;
            printk("[syscall] PACKET_SYSCALL_RECV_CALLER: Syscall #%u - Called by task %d\n", 
                   current_syscall_id, current_task);
            
            printk("[syscall] PACKET_SYSCALL_RECV_CALLING: Syscall #%u - Calling net_receive()\n", 
                   current_syscall_id);
            
            struct net_packet *pkt = net_receive();
            if (!pkt) {
                printk("[syscall] PACKET_SYSCALL_RECV_EMPTY: Syscall #%u - No packet available\n", 
                       current_syscall_id);
                return 0;
            }
            
            printk("[syscall] PACKET_SYSCALL_RECV_FOUND: Syscall #%u - Packet received (%d bytes)\n", 
                   current_syscall_id, pkt->length);
            
            int copy_len = (pkt->length < arg2) ? pkt->length : (int)arg2;
            
            printk("[syscall] PACKET_SYSCALL_RECV_COPYING: Syscall #%u - Copying %d bytes to user buffer\n", 
                   current_syscall_id, copy_len);
            
            for (int i = 0; i < copy_len; i++) {
                ((uint8_t*)arg1)[i] = pkt->data[i];
            }
            
            // Log copied data preview
            printk("[syscall] PACKET_SYSCALL_RECV_DATA: Syscall #%u - Copied first 16 bytes: ", current_syscall_id);
            int preview_len = (copy_len < 16) ? copy_len : 16;
            for (int i = 0; i < preview_len; i++) {
                printk("%02x ", ((uint8_t*)arg1)[i]);
            }
            printk("\n");
            
            int actual_len = pkt->length;
            net_free_packet(pkt);
            
            printk("[syscall] PACKET_SYSCALL_RECV_COMPLETE: Syscall #%u - Returned %d bytes to caller\n", 
                   current_syscall_id, actual_len);
            
            return actual_len;
        }
        
        case SYS_GET_PID:
            return current_task;
            
        case SYS_YIELD:
            task_yield();
            return SYSCALL_OK;
            
        case SYS_CREATE_TASK:
            return create_task((void (*)(void))arg1);
            
        case SYS_EXIT:
            // Mark current task as unused
            if (current_task >= 0 && current_task < MAX_TASKS) {
                tasks[current_task].state = TASK_UNUSED;
                task_yield(); // Switch to another task
            }
            return SYSCALL_OK;
            
        default:
            printk("[syscall] Unknown system call: %ld\n", syscall_num);
            return SYSCALL_ERROR;
    }
}
