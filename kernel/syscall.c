#include "syscall.h"
#include "ipc.h"
#include "sched.h"
#include "net_driver.h"
#include "migration.h"
#include "printk.h"
#include "uaccess.h"
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
            /* Counter still advances so the running total stays accurate
             * even when tracing is compiled out. */
            uint32_t current_syscall_id = ++syscall_net_send_counter;
            uint16_t len = (uint16_t)arg2;
            const uint8_t* data = (const uint8_t*)arg1;
            (void)current_syscall_id;

            trace_pk("[syscall] PACKET_SYSCALL_SEND_START: Syscall #%u - NET_SEND request (%d bytes)\n",
                   current_syscall_id, len);

            // Log caller context if available
            extern int current_task;
            (void)current_task;
            trace_pk("[syscall] PACKET_SYSCALL_SEND_CALLER: Syscall #%u - Called by task %d\n",
                   current_syscall_id, current_task);

            // Log packet preview
#if TRACE_PACKETS
            if (len > 0 && data != NULL) {
                trace_pk("[syscall] PACKET_SYSCALL_SEND_DATA: Syscall #%u - First 16 bytes: ", current_syscall_id);
                int preview_len = (len < 16) ? len : 16;
                for (int i = 0; i < preview_len; i++) {
                    trace_pk("%02x ", data[i]);
                }
                trace_pk("\n");
            }
#endif

            trace_pk("[syscall] PACKET_SYSCALL_SEND_CALLING: Syscall #%u - Forwarding to net_send()\n",
                   current_syscall_id);

            int result = net_send(data, len);

            trace_pk("[syscall] PACKET_SYSCALL_SEND_COMPLETE: Syscall #%u - Result: %d\n",
                   current_syscall_id, result);

            return result;
        }
            
        case SYS_NET_RECV: {
            uint32_t current_syscall_id = ++syscall_net_recv_counter;
            (void)current_syscall_id;

            trace_pk("[syscall] PACKET_SYSCALL_RECV_START: Syscall #%u - NET_RECV request (max %ld bytes)\n",
                   current_syscall_id, arg2);

            extern int current_task;
            (void)current_task;
            trace_pk("[syscall] PACKET_SYSCALL_RECV_CALLER: Syscall #%u - Called by task %d\n",
                   current_syscall_id, current_task);

            trace_pk("[syscall] PACKET_SYSCALL_RECV_CALLING: Syscall #%u - Calling net_receive()\n",
                   current_syscall_id);

            struct net_packet *pkt = net_receive();
            if (!pkt) {
                /* The overwhelmingly common case — the old unconditional
                 * printk here was the single largest source of log spam. */
                trace_pk("[syscall] PACKET_SYSCALL_RECV_EMPTY: Syscall #%u - No packet available\n",
                       current_syscall_id);
                return 0;
            }

            trace_pk("[syscall] PACKET_SYSCALL_RECV_FOUND: Syscall #%u - Packet received (%d bytes)\n",
                   current_syscall_id, pkt->length);

            int copy_len = (pkt->length < arg2) ? pkt->length : (int)arg2;

            trace_pk("[syscall] PACKET_SYSCALL_RECV_COPYING: Syscall #%u - Copying %d bytes to user buffer\n",
                   current_syscall_id, copy_len);

            for (int i = 0; i < copy_len; i++) {
                ((uint8_t*)arg1)[i] = pkt->data[i];
            }

            // Log copied data preview
#if TRACE_PACKETS
            trace_pk("[syscall] PACKET_SYSCALL_RECV_DATA: Syscall #%u - Copied first 16 bytes: ", current_syscall_id);
            int preview_len = (copy_len < 16) ? copy_len : 16;
            for (int i = 0; i < preview_len; i++) {
                trace_pk("%02x ", ((uint8_t*)arg1)[i]);
            }
            trace_pk("\n");
#endif

            int actual_len = pkt->length;
            net_free_packet(pkt);

            trace_pk("[syscall] PACKET_SYSCALL_RECV_COMPLETE: Syscall #%u - Returned %d bytes to caller\n",
                   current_syscall_id, actual_len);

            return actual_len;
        }
        
        case SYS_GET_PID:
            return task_current_pid();
            
        case SYS_YIELD:
            task_yield();
            return SYSCALL_OK;
            
        case SYS_CREATE_TASK:
            return create_task((void (*)(void))arg1);
            
        case SYS_EXIT:
            if (task_current_pid() > KERNEL_TASK_PID)
                terminate_task((uint32_t)task_current_pid());
            task_yield();
            return SYSCALL_OK;

        case SYS_CHECKPOINT:
            /* arg1 = pid, arg2 = user buffer, arg3 = buffer length */
            return migration_checkpoint((uint32_t)arg1,
                                        (void *)arg2,
                                        (unsigned long)arg3);

        case SYS_RESTORE:
            /* arg1 = user buffer, arg2 = buffer length */
            return migration_restore((const void *)arg1,
                                     (unsigned long)arg2);

        default:
            printk("[syscall] Unknown system call: %ld\n", syscall_num);
            return SYSCALL_ERROR;
    }
}

long handle_user_syscall(struct trap_frame *frame) {
    if (!frame || current_task <= 0 || current_task >= num_tasks ||
        !tasks[current_task].user_mode) return SYSCALL_ERROR;

    long number = (long)frame->a7;
    switch (number) {
        case SYS_GET_PID:
            return task_current_pid();

        case SYS_SEND_MSG: {
            int size = (int)frame->a2;
            unsigned char bounce[MAX_MESSAGE_SIZE];
            if (size <= 0 || size > MAX_MESSAGE_SIZE ||
                copy_from_user(bounce, (const void *)frame->a1,
                               (unsigned long)size) < 0) return SYSCALL_ERROR;
            return send_message((int)frame->a0, bounce, size);
        }

        case SYS_RECV_MSG: {
            int max_size = (int)frame->a2;
            unsigned char bounce[MAX_MESSAGE_SIZE];
            if (max_size <= 0 || max_size > MAX_MESSAGE_SIZE ||
                user_range_valid(&tasks[current_task], (void *)frame->a1,
                                 (unsigned long)max_size, 1) < 0)
                return SYSCALL_ERROR;
            int result = recv_message((int)frame->a0, bounce, max_size);
            if (result <= 0) return result;
            int copied = result < max_size ? result : max_size;
            if (copy_to_user((void *)frame->a1, bounce,
                             (unsigned long)copied) < 0) return SYSCALL_ERROR;
            return result;
        }

        case SYS_NET_SEND: {
            int size = (int)frame->a1;
            unsigned char bounce[1514];
            if (size <= 0 || size > (int)sizeof(bounce) ||
                copy_from_user(bounce, (const void *)frame->a0,
                               (unsigned long)size) < 0) return SYSCALL_ERROR;
            return net_send(bounce, (uint16_t)size);
        }

        case SYS_NET_RECV: {
            int max_size = (int)frame->a1;
            if (max_size <= 0 || max_size > 1514 ||
                user_range_valid(&tasks[current_task], (void *)frame->a0,
                                 (unsigned long)max_size, 1) < 0)
                return SYSCALL_ERROR;
            struct net_packet *packet = net_receive();
            if (!packet) return 0;
            int copied = packet->length < max_size ? packet->length : max_size;
            int result = packet->length;
            if (copy_to_user((void *)frame->a0, packet->data,
                             (unsigned long)copied) < 0) result = SYSCALL_ERROR;
            net_free_packet(packet);
            return result;
        }

        default:
            return SYSCALL_ERROR;
    }
}
