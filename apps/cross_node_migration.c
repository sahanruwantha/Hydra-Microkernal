/*
 * Cross-node process migration over real VirtIO-Net.
 *
 * Each Hydra instance reads its MAC last byte to decide its role:
 *   0x01  → SOURCE:      spawn workload, checkpoint, send fragments
 *   0x02  → DESTINATION: receive fragments, reassemble, restore
 *
 * The fragment protocol is deliberately simple:
 *
 *   Ethernet header (14 bytes, dst=broadcast, ethertype=0x88B5)
 *   followed by:
 *     'H' 'Y' 'D' 'M'      (4-byte magic — distinguishes from net_test 'HYDR')
 *     u8   msg_type        (1 = MIG_DATA, 2 = MIG_ACK)
 *     u32  message_id      (big-endian, prevents cross-run mixing)
 *     u32  total_size      (big-endian, full checkpoint length)
 *     u32  offset          (big-endian, where this fragment lives in the buf)
 *     u16  frag_size       (big-endian, payload bytes in this frame)
 *     u8 [frag_size]       fragment data
 *
 * Max payload per frame: 1500 - 14 (eth) - 19 = 1467 bytes.
 * For an 8 KB checkpoint that's ~6 fragments; for a 72 KB checkpoint, ~50.
 *
 * Source emits BENCH net_send_start once before the first fragment and
 * net_send_done after the last, so the host harness can measure
 * cross-node transport latency. Destination emits BENCH net_recv_start
 * on the first fragment and net_recv_done on completion.
 */

#include "../kernel/net_driver.h"
#include "../kernel/migration.h"
#include "../kernel/sched.h"
#include "../kernel/printk.h"
#include "../kernel/syscall.h"
#include "../kernel/bench.h"

#define HYDRA_MIG_ETHERTYPE 0x88b5
#define MIG_DATA            1
#define MIG_ACK             2
#define MIG_FRAG_HDR        19   /* magic + type + id + total + offset + size */
#define MAX_FRAG_PAYLOAD    1467 /* fits in min-MTU Ethernet */
#define MAX_FRAGMENTS       128
#define SEND_RETRIES        4

/* The largest payload we sweep is 64 KB extra + 8 KB stack + headers. */
static unsigned char ck_buf[96 * 1024] __attribute__((aligned(16)));
/* Outgoing frame scratch space. */
static unsigned char tx_frame[1500] __attribute__((aligned(16)));

/* Workload task for the source: just counts. */
volatile static int xmig_counter = 0;
static void xmig_task(void) {
    while (1) {
        xmig_counter++;
        for (volatile int i = 0; i < 50000; i++) {}
        task_yield();
    }
}

/* Cross-node isolated workload. It writes both ends of its private data/heap
 * mapping, yields long enough for the source to checkpoint it, then validates
 * the values after destination restore before exiting with its new PID. */
__attribute__((section(".user_text"), noinline, noreturn, used))
static void xmig_user_task(void) {
    asm volatile(
        "mv s1, a0\n"
        "li s2, 0x40001000\n"
        "li s3, 0x40002ff8\n"
        "li t0, 0x1020304050607080\n"
        "sd t0, 0(s2)\n"
        "li t0, 0x8070605040302010\n"
        "sd t0, 0(s3)\n"
        "li s0, 0\n"
        "1:\n"
        "addi s0, s0, 1\n"
        "li a7, 6\n"
        "ecall\n"
        "li t0, 0x1020304050607080\n"
        "ld t1, 0(s2)\n"
        "bne t0, t1, 3f\n"
        "li t0, 0x8070605040302010\n"
        "ld t1, 0(s3)\n"
        "bne t0, t1, 3f\n"
        "blt s0, s1, 1b\n"
        "li a7, 5\n"
        "ecall\n"
        "j 4f\n"
        "3:\n"
        "li a0, -2\n"
        "4:\n"
        "li a7, 8\n"
        "ecall\n"
        "5: j 5b\n"
        : : : "a0", "a7", "t0", "t1", "s0", "s1", "s2", "s3", "memory");
    __builtin_unreachable();
}

/* Big-endian helpers. */
static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xff; p[1] = (v >> 16) & 0xff;
    p[2] = (v >>  8) & 0xff; p[3] =  v        & 0xff;
}
static uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}
static uint16_t get_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static int send_one_fragment(const uint8_t *src_mac,
                             uint32_t message_id, uint32_t total, uint32_t offset,
                             const uint8_t *payload, uint16_t paylen)
{
    int o = 0;
    for (int i = 0; i < 6; i++) tx_frame[o++] = 0xff;
    for (int i = 0; i < 6; i++) tx_frame[o++] = src_mac[i];
    tx_frame[o++] = (HYDRA_MIG_ETHERTYPE >> 8) & 0xff;
    tx_frame[o++] =  HYDRA_MIG_ETHERTYPE       & 0xff;
    tx_frame[o++] = 'H'; tx_frame[o++] = 'Y';
    tx_frame[o++] = 'D'; tx_frame[o++] = 'M';
    tx_frame[o++] = MIG_DATA;
    put_be32(tx_frame + o, message_id); o += 4;
    put_be32(tx_frame + o, total);  o += 4;
    put_be32(tx_frame + o, offset); o += 4;
    tx_frame[o++] = (paylen >> 8) & 0xff;
    tx_frame[o++] =  paylen       & 0xff;
    for (uint16_t i = 0; i < paylen; i++) tx_frame[o + i] = payload[i];
    o += paylen;
    while (o < 60) tx_frame[o++] = 0;
    return net_send(tx_frame, (uint16_t)o);
}

static int send_ack(const uint8_t *src_mac, uint32_t message_id) {
    int o = 0;
    for (int i = 0; i < 6; i++) tx_frame[o++] = 0xff;
    for (int i = 0; i < 6; i++) tx_frame[o++] = src_mac[i];
    tx_frame[o++] = (HYDRA_MIG_ETHERTYPE >> 8) & 0xff;
    tx_frame[o++] =  HYDRA_MIG_ETHERTYPE       & 0xff;
    tx_frame[o++] = 'H'; tx_frame[o++] = 'Y';
    tx_frame[o++] = 'D'; tx_frame[o++] = 'M';
    tx_frame[o++] = MIG_ACK;
    put_be32(tx_frame + o, message_id); o += 4;
    put_be32(tx_frame + o, 0); o += 4;
    put_be32(tx_frame + o, 0); o += 4;
    tx_frame[o++] = 0; tx_frame[o++] = 0;
    while (o < 60) tx_frame[o++] = 0;
    return net_send(tx_frame, (uint16_t)o);
}

static int wait_for_ack(uint32_t message_id) {
    for (int poll = 0; poll < 30000; poll++) {
        for (volatile int spin = 0; spin < 500; spin++) {}
        struct net_packet *p = net_receive();
        if (!p) continue;
        int matched = p->length >= 14 + MIG_FRAG_HDR &&
                      p->data[12] == ((HYDRA_MIG_ETHERTYPE >> 8) & 0xff) &&
                      p->data[13] == (HYDRA_MIG_ETHERTYPE & 0xff) &&
                      p->data[14] == 'H' && p->data[15] == 'Y' &&
                      p->data[16] == 'D' && p->data[17] == 'M' &&
                      p->data[18] == MIG_ACK &&
                      get_be32(&p->data[19]) == message_id;
        net_free_packet(p);
        if (matched) return 1;
    }
    return 0;
}

/* Source role: checkpoint + transmit. */
static void source_role(const uint8_t *mac, uint32_t message_id,
                        int isolated) {
    printk("[xmig] SOURCE %srole\n", isolated ? "U-mode " : "");

    int pid = isolated ? create_user_task_arg(xmig_user_task, 8)
                       : create_task(xmig_task);
    if (pid < 0) { printk("[xmig] SOURCE: create_task failed\n"); return; }
    for (int i = 0; i < (isolated ? 2 : 20); i++) task_yield();
    if (isolated)
        printk("[xmig] SOURCE: isolated workload pid=%d, checkpointing...\n", pid);
    else
        printk("[xmig] SOURCE: workload pid=%d counter=%d, checkpointing...\n",
               pid, xmig_counter);

    long n = syscall_checkpoint((uint32_t)pid, ck_buf, sizeof(ck_buf));
    if (n < 0) { printk("[xmig] SOURCE: checkpoint failed (%ld)\n", n); return; }
    printk("[xmig] SOURCE: checkpoint %ld bytes ready, fragmenting...\n", n);

    uint32_t total = (uint32_t)n;
    int acknowledged = 0;
    int frags = 0;
    BENCH_TS("net_send_start");
    for (int attempt = 0; attempt < SEND_RETRIES && !acknowledged; attempt++) {
        uint32_t off = 0;
        frags = 0;
        while (off < total) {
            uint32_t remain = total - off;
            uint16_t chunk = (remain > MAX_FRAG_PAYLOAD) ? MAX_FRAG_PAYLOAD : (uint16_t)remain;
            if (send_one_fragment(mac, message_id, total, off,
                                  ck_buf + off, chunk) < 0) {
                printk("[xmig] SOURCE: net_send failed @offset=%d\n", off);
                break;
            }
            off += chunk;
            frags++;
            for (volatile int i = 0; i < 4000; i++) {}
        }
        if (attempt == 0) BENCH_TS("net_send_done");
        acknowledged = wait_for_ack(message_id);
        if (!acknowledged) {
            printk("[xmig] SOURCE: no ACK for message %u, retry %d/%d\n",
                   message_id, attempt + 1, SEND_RETRIES);
        }
    }
    BENCH_TS("net_ack_done");
    printk("[xmig] SOURCE: sent %d fragments totalling %d bytes, %s\n",
           frags, total, acknowledged ? "ACK received" : "UNACKNOWLEDGED");

    /* Free the source task slot so we can loop. The checkpoint left
     * the task BLOCKED; we don't need it any more. */
    terminate_task((uint32_t)pid);
}

/* Destination role: receive fragments, reassemble, restore. */
static void destination_role(const uint8_t *mac, uint32_t message_id,
                             int isolated) {
    printk("[xmig] DESTINATION %srole, waiting for fragments...\n",
           isolated ? "U-mode " : "");

    uint32_t expected_total = 0;
    uint32_t received_bytes = 0;
    int first_frag = 1;
    uint64_t received_map[2] = {0, 0};

    /* Poll for up to several seconds. */
    for (int t = 0; t < 30000; t++) {
        for (volatile int i = 0; i < 5000; i++) {}
        struct net_packet *p = net_receive();
        if (!p) continue;

        if (p->length >= 14 + MIG_FRAG_HDR &&
            p->data[12] == ((HYDRA_MIG_ETHERTYPE >> 8) & 0xff) &&
            p->data[13] == (HYDRA_MIG_ETHERTYPE & 0xff) &&
            p->data[14] == 'H' && p->data[15] == 'Y' &&
            p->data[16] == 'D' && p->data[17] == 'M' &&
            p->data[18] == MIG_DATA) {
            uint32_t incoming_id = get_be32(&p->data[19]);
            uint32_t total  = get_be32(&p->data[23]);
            uint32_t offset = get_be32(&p->data[27]);
            uint16_t fsz    = get_be16(&p->data[31]);

            if (incoming_id < message_id) {
                /* The previous ACK was lost. Re-acknowledge without mixing
                 * an old migration into the new reassembly buffer. */
                send_ack(mac, incoming_id);
                net_free_packet(p);
                continue;
            }
            if (incoming_id != message_id || total == 0 ||
                total > sizeof(ck_buf) || fsz == 0 ||
                p->length < 14 + MIG_FRAG_HDR + fsz) {
                net_free_packet(p);
                continue;
            }

            if (first_frag) {
                BENCH_TS("net_recv_start");
                expected_total = total;
                first_frag = 0;
            }
            if (total != expected_total) {
                net_free_packet(p);
                continue;
            }
            uint32_t frag_index = offset / MAX_FRAG_PAYLOAD;
            int valid_fragment = frag_index < MAX_FRAGMENTS &&
                                 offset == frag_index * MAX_FRAG_PAYLOAD &&
                                 offset + fsz <= total &&
                                 fsz <= MAX_FRAG_PAYLOAD;
            if (valid_fragment) {
                uint64_t bit = 1UL << (frag_index % 64);
                uint64_t *word = &received_map[frag_index / 64];
                for (uint16_t i = 0; i < fsz; i++) {
                    ck_buf[offset + i] = p->data[33 + i];
                }
                if (!(*word & bit)) {
                    *word |= bit;
                    received_bytes += fsz;
                }
            }
            net_free_packet(p);

            if (received_bytes >= expected_total && expected_total > 0) {
                BENCH_TS("net_recv_done");
                printk("[xmig] DESTINATION: reassembled %d bytes, restoring...\n",
                       expected_total);
                long new_pid = syscall_restore(ck_buf, expected_total);
                if (new_pid < 0) {
                    printk("[xmig] DESTINATION: restore failed (%ld)\n", new_pid);
                } else {
                    int restored_ok = 1;
                    if (isolated) {
                        uint32_t exited_pid = 0;
                        for (int i = 0; i < 64; i++) {
                            task_yield();
                            (void)sched_last_exit_code(&exited_pid);
                            if (exited_pid == (uint32_t)new_pid) break;
                        }
                        long exit_code = sched_last_exit_code(&exited_pid);
                        restored_ok = exited_pid == (uint32_t)new_pid &&
                                      exit_code == new_pid;
                        if (restored_ok) {
                            printk("[xmig] DESTINATION U: PASS — new pid=%ld data/heap intact\n",
                                   new_pid);
                        } else {
                            printk("[xmig] DESTINATION U: FAIL — pid=%u/%ld code=%ld\n",
                                   exited_pid, new_pid, exit_code);
                            if (task_find_by_pid((uint32_t)new_pid))
                                terminate_task((uint32_t)new_pid);
                        }
                    } else {
                        for (int i = 0; i < 30; i++) task_yield();
                        printk("[xmig] DESTINATION: PASS — new pid=%ld counter=%d\n",
                               new_pid, xmig_counter);
                        terminate_task((uint32_t)new_pid);
                    }
                    if (restored_ok) {
                        /* Repeat the ACK to tolerate a dropped acknowledgement. */
                        for (int ack = 0; ack < 3; ack++)
                            send_ack(mac, message_id);
                    }
                }
                return;
            }
        } else {
            net_free_packet(p);
        }
    }
    printk("[xmig] DESTINATION: TIMEOUT waiting for fragments (got %d / %d bytes)\n",
           received_bytes, expected_total);
}

/* Number of cross-node migrations to run for benchmarking. Both
 * source and destination loop this many times in lock-step. */
#define XMIG_RUNS 15

void cross_node_migration(void) {
    uint8_t mac[6];
    net_get_mac_address(mac);
    if (mac[5] != 0x01 && mac[5] != 0x02) {
        printk("[xmig] not a two-node MAC (last byte 0x%02x); skipping\n", mac[5]);
        return;
    }
    for (int run = 0; run < XMIG_RUNS; run++) {
        printk("[xmig] === run %d/%d ===\n", run + 1, XMIG_RUNS);
        uint32_t message_id = (uint32_t)run + 1;
        if (mac[5] == 0x01) source_role(mac, message_id, 0);
        else                destination_role(mac, message_id, 0);
        /* Brief pause so logs from each role stay loosely interleaved. */
        for (volatile int i = 0; i < 200000; i++) {}
    }
    printk("[xmig] === isolated U-mode run 1/1 ===\n");
    uint32_t user_message_id = XMIG_RUNS + 1;
    if (mac[5] == 0x01) source_role(mac, user_message_id, 1);
    else                destination_role(mac, user_message_id, 1);
    printk("[xmig] BENCH_XMIG_DONE runs=%d isolated=1\n", XMIG_RUNS);
}
