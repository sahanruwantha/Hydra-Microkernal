#include "syscall.h"
#include "ipc.h"
#include "sched.h"
#include "net_driver.h"
#include "printk.h"

// Handle system calls from user space
long handle_syscall(long syscall_num, long arg1, long arg2, long arg3, long arg4) {
    switch (syscall_num) {
        case SYS_SEND_MSG:
            return send_message((int)arg1, (const void*)arg2, (int)arg3);
            
        case SYS_RECV_MSG:
            return recv_message((int)arg1, (void*)arg2, (int)arg3);
            
        case SYS_NET_SEND:
            return net_send((const uint8_t*)arg1, (uint16_t)arg2);
            
        case SYS_NET_RECV: {
            struct net_packet *pkt = net_receive();
            if (!pkt) return 0;
            
            int copy_len = (pkt->length < arg2) ? pkt->length : (int)arg2;
            // In a real implementation, we'd need to copy to user space safely
            for (int i = 0; i < copy_len; i++) {
                ((uint8_t*)arg1)[i] = pkt->data[i];
            }
            int actual_len = pkt->length;
            net_free_packet(pkt);
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
