#include "distributed.h"
#include "printk.h"
#include "net_driver.h"
#include "mm.h"
#include "sched.h"
#include "timer.h"

// Basic type definitions
typedef unsigned long size_t;
#define NULL ((void*)0)

// Simple string functions for kernel use
static void *k_memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = c;
    return s;
}

static void *k_memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static int k_snprintf(char *str, size_t size, const char *format, ...) {
    // Simplified snprintf - just handle basic format
    if (!str || size == 0) return 0;
    
    // For now, just copy the format string for simple cases
    size_t len = 0;
    const char *p = format;
    while (*p && len < size - 1) {
        str[len++] = *p++;
    }
    str[len] = '\0';
    return len;
}

static distributed_system_t dist_system;
static uint32_t message_sequence = 0;

void distributed_init(uint32_t node_id, node_type_t type, const char *name) {
    printk("[dist] Initializing distributed system...\n");
    
    // Initialize local system state
    k_memset(&dist_system, 0, sizeof(dist_system));
    
    dist_system.local_node_id = node_id;
    dist_system.num_nodes = 1;  // Start with self
    dist_system.num_migrations = 0;
    dist_system.is_master = (type == NODE_TYPE_MASTER);
    dist_system.master_node_id = dist_system.is_master ? node_id : 0;
    
    // Add self to node list
    kernel_node_t *self = &dist_system.nodes[0];
    self->node_id = node_id;
    self->ip_addr = 0x0A000001 + node_id; // 10.0.0.1 + node_id
    self->type = type;
    self->status = NODE_STATUS_ALIVE;
    self->load = 0;
    self->capabilities = (1 << type);
    self->last_heartbeat = 0; // Will be updated by timer
    // Simple string copy instead of k_snprintf
    const char *src = name;
    char *dst = self->name;
    int i = 0;
    while (*src && i < sizeof(self->name) - 1) {
        *dst++ = *src++;
        i++;
    }
    *dst = '\0';
    
    printk("[dist] Node initialized: ID=");
    printk("%u", node_id);
    printk(", Type=");
    printk(node_type_to_string(type));
    printk(", IP=10.0.0.");
    printk("%u", (node_id + 1));
    printk("\n");
    
    // Start discovery process
    send_discovery_request();
}

void distributed_tick(void) {
    static uint64_t last_heartbeat = 0;
    static uint64_t last_discovery = 0;
    uint64_t current_time = get_tick_count();
    
    // Send periodic heartbeats
    if (current_time - last_heartbeat > HEARTBEAT_INTERVAL) {
        send_heartbeat();
        last_heartbeat = current_time;
    }
    
    // Send periodic discovery requests if we have few nodes
    if (dist_system.num_nodes < 3 && 
        current_time - last_discovery > HEARTBEAT_INTERVAL * 2) {
        send_discovery_request();
        last_discovery = current_time;
    }
    
    // Check for dead nodes
    for (int i = 0; i < dist_system.num_nodes; i++) {
        kernel_node_t *node = &dist_system.nodes[i];
        if (node->node_id != dist_system.local_node_id &&
            current_time - node->last_heartbeat > HEARTBEAT_INTERVAL * 3) {
            node->status = NODE_STATUS_DEAD;
        }
    }
    
    // Process ongoing migrations
    migration_tick();
    
    // Update local load
    update_local_load();
}

bool distributed_add_node(uint32_t node_id, uint32_t ip_addr, node_type_t type) {
    if (dist_system.num_nodes >= MAX_KERNEL_NODES) {
        printk("[dist] Cannot add node %u: node table full\n", node_id);
        return false;
    }
    
    // Check if node already exists
    for (int i = 0; i < dist_system.num_nodes; i++) {
        if (dist_system.nodes[i].node_id == node_id) {
            // Update existing node
            dist_system.nodes[i].ip_addr = ip_addr;
            dist_system.nodes[i].type = type;
            dist_system.nodes[i].status = NODE_STATUS_ALIVE;
            dist_system.nodes[i].last_heartbeat = get_tick_count();
            printk("[dist] Updated existing node %u\n", node_id);
            return true;
        }
    }
    
    // Add new node
    kernel_node_t *new_node = &dist_system.nodes[dist_system.num_nodes];
    new_node->node_id = node_id;
    new_node->ip_addr = ip_addr;
    new_node->type = type;
    new_node->status = NODE_STATUS_ALIVE;
    new_node->load = 0;
    new_node->capabilities = (1 << type);
    new_node->last_heartbeat = get_tick_count();
    // Simple string copy for node name
    const char *prefix = "node-";
    char *dst = new_node->name;
    int i = 0;
    
    // Copy "node-" prefix
    while (*prefix && i < sizeof(new_node->name) - 2) {
        *dst++ = *prefix++;
        i++;
    }
    
    // Add simple node ID (just single digit for now)
    if (i < sizeof(new_node->name) - 1) {
        *dst++ = '0' + (node_id % 10);
        i++;
    }
    *dst = '\0';
    
    dist_system.num_nodes++;
    
    printk("[dist] Added new node: ID=");
    printk("%u", node_id);
    printk(", Type=");
    printk(node_type_to_string(type));
    printk(", IP=10.0.0.");
    printk("%u", (ip_addr & 0xFF));
    printk("\n");
    
    return true;
}

kernel_node_t *distributed_find_node(uint32_t node_id) {
    for (int i = 0; i < dist_system.num_nodes; i++) {
        if (dist_system.nodes[i].node_id == node_id) {
            return &dist_system.nodes[i];
        }
    }
    return NULL;
}

kernel_node_t *distributed_find_best_node(node_type_t type) {
    kernel_node_t *best_node = NULL;
    uint32_t lowest_load = 100;
    
    for (int i = 0; i < dist_system.num_nodes; i++) {
        kernel_node_t *node = &dist_system.nodes[i];
        
        // Skip dead nodes and self
        if (node->status != NODE_STATUS_ALIVE || 
            node->node_id == dist_system.local_node_id) {
            continue;
        }
        
        // Check if node has required capability
        if (!(node->capabilities & (1 << type))) {
            continue;
        }
        
        // Find node with lowest load
        if (node->load < lowest_load) {
            lowest_load = node->load;
            best_node = node;
        }
    }
    
    return best_node;
}

void distributed_send_message(uint32_t target_node, dist_msg_type_t type, 
                             const void *data, uint32_t size) {
    if (size > sizeof(((dist_message_t*)0)->data)) {
        printk("[dist] Message too large: %u bytes\n", size);
        return;
    }
    
    dist_message_t msg;
    msg.type = type;
    msg.source_node = dist_system.local_node_id;
    msg.target_node = target_node;
    msg.sequence = message_sequence++;
    msg.data_size = size;
    
    if (data && size > 0) {
        k_memcpy(msg.data, data, size);
    }
    
    kernel_node_t *target = distributed_find_node(target_node);
    if (!target) {
        // In multi-node setups, we should try to discover unknown nodes
        printk("[dist] Cannot send message: unknown target node %u (attempting discovery)\n", target_node);
        return;
    }
    
    // Send via network (simplified - in real implementation would use proper networking)
    printk("[dist] Sending message type %d to node %u\n", type, target_node);
    
    // For now, simulate network transmission
    // In a real implementation, this would use the network driver
    // to send UDP packets to the target node's IP address
}

void send_discovery_request(void) {
    printk("[dist] Broadcasting discovery request\n");
    
    // Always try discovery - we might have multiple nodes
    // Broadcast discovery to all possible nodes
    for (uint32_t i = 1; i <= 4; i++) {
        if (i != dist_system.local_node_id) {
            distributed_send_message(i, MSG_DISCOVERY_REQUEST, NULL, 0);
        }
    }
}

void send_heartbeat(void) {
    struct {
        uint32_t load;
        uint32_t capabilities;
        node_status_t status;
    } heartbeat_data;
    
    heartbeat_data.load = calculate_node_load(dist_system.local_node_id);
    heartbeat_data.capabilities = dist_system.nodes[0].capabilities;
    heartbeat_data.status = NODE_STATUS_ALIVE;
    
    // Send heartbeat to all known nodes
    for (int i = 1; i < dist_system.num_nodes; i++) {
        kernel_node_t *node = &dist_system.nodes[i];
        if (node->status == NODE_STATUS_ALIVE) {
            distributed_send_message(node->node_id, MSG_HEARTBEAT, 
                                   &heartbeat_data, sizeof(heartbeat_data));
        }
    }
}

void update_local_load(void) {
    // Calculate load based on active tasks and system state
    uint32_t active_tasks = get_active_task_count();
    uint32_t memory_used = get_memory_usage_percent();
    
    // Simple load calculation (can be improved)
    uint32_t load = (active_tasks * 20) + (memory_used / 4);
    if (load > 100) load = 100;
    
    dist_system.nodes[0].load = load;
}

uint32_t calculate_node_load(uint32_t node_id) {
    kernel_node_t *node = distributed_find_node(node_id);
    return node ? node->load : 100; // Unknown nodes considered fully loaded
}

bool migrate_process_to_node(uint32_t process_id, uint32_t target_node) {
    printk("[dist] Starting migration of process %u to node %u\n", 
           process_id, target_node);
    
    migrated_process_t *migration = NULL;
    for (int i = 0; i < MAX_MIGRATED_PROCESSES; i++) {
        if (dist_system.migrations[i].state == MIGRATION_IDLE) {
            migration = &dist_system.migrations[i];
            break;
        }
    }
    
    if (!migration) {
        printk("[dist] No available migration slots\n");
        return false;
    }
    
    // Check if target node exists and is capable
    kernel_node_t *target = distributed_find_node(target_node);
    if (!target || target->status != NODE_STATUS_ALIVE) {
        printk("[dist] Target node %u not available\n", target_node);
        return false;
    }
    
    // Initialize migration
    migration->process_id = process_id;
    migration->source_node = dist_system.local_node_id;
    migration->target_node = target_node;
    migration->state = MIGRATION_PREPARING;
    migration->data_size = 0;
    migration->process_data = NULL;
    migration->start_time = get_tick_count();
    
    // Send migration request
    struct {
        uint32_t process_id;
        uint32_t estimated_size;
    } request_data;
    
    request_data.process_id = process_id;
    request_data.estimated_size = 4096; // Estimated process state size
    
    distributed_send_message(target_node, MSG_MIGRATION_REQUEST, 
                           &request_data, sizeof(request_data));
    
    dist_system.num_migrations++;
    return true;
}

void migration_tick(void) {
    for (int i = 0; i < MAX_MIGRATED_PROCESSES; i++) {
        migrated_process_t *migration = &dist_system.migrations[i];
        
        if (migration->state == MIGRATION_IDLE) continue;
        
        uint64_t elapsed = get_tick_count() - migration->start_time;
        
        // Timeout handling
        if (elapsed > 30000) { // 30 second timeout
            printk("[dist] Migration timeout for process %u\n", migration->process_id);
            migration->state = MIGRATION_FAILED;
        }
    }
}

const char *node_type_to_string(node_type_t type) {
    switch (type) {
        case NODE_TYPE_COMPUTE: return "COMPUTE";
        case NODE_TYPE_GRAPHICS: return "GRAPHICS";
        case NODE_TYPE_STORAGE: return "STORAGE";
        case NODE_TYPE_MASTER: return "MASTER";
        default: return "UNKNOWN";
    }
}

const char *node_status_to_string(node_status_t status) {
    switch (status) {
        case NODE_STATUS_UNKNOWN: return "UNKNOWN";
        case NODE_STATUS_ALIVE: return "ALIVE";
        case NODE_STATUS_BUSY: return "BUSY";
        case NODE_STATUS_DEAD: return "DEAD";
        default: return "INVALID";
    }
}

const char *migration_state_to_string(migration_state_t state) {
    switch (state) {
        case MIGRATION_IDLE: return "IDLE";
        case MIGRATION_PREPARING: return "PREPARING";
        case MIGRATION_TRANSFERRING: return "TRANSFERRING";
        case MIGRATION_COMPLETING: return "COMPLETING";
        case MIGRATION_FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

void print_distributed_status(void) {
    printk("\n=== Distributed System Status ===\n");
    printk("Local Node: %u (%s)\n", dist_system.local_node_id,
           dist_system.is_master ? "MASTER" : "WORKER");
    printk("Connected Nodes: %d\n", dist_system.num_nodes);
    
    for (int i = 0; i < dist_system.num_nodes; i++) {
        kernel_node_t *node = &dist_system.nodes[i];
        printk("  Node %u: %s %s Load=%u%% IP=10.0.0.%u\n",
               node->node_id,
               node_type_to_string(node->type),
               node_status_to_string(node->status),
               node->load,
               (node->ip_addr & 0xFF));
    }
    
    printk("Active Migrations: %d\n", dist_system.num_migrations);
    for (int i = 0; i < MAX_MIGRATED_PROCESSES; i++) {
        migrated_process_t *migration = &dist_system.migrations[i];
        if (migration->state != MIGRATION_IDLE) {
            printk("  Process %u: %u->%u (%s)\n",
                   migration->process_id,
                   migration->source_node,
                   migration->target_node,
                   migration_state_to_string(migration->state));
        }
    }
    printk("================================\n\n");
}

// Helper functions for distributed raytracer
uint32_t get_local_node_id(void) {
    return dist_system.local_node_id;
}

int get_distributed_node_count(void) {
    return dist_system.num_nodes;
}

uint32_t get_best_compute_node_id(void) {
    kernel_node_t *best_node = NULL;
    uint32_t lowest_load = 100;
    
    // Find the compute node with the lowest load
    for (int i = 0; i < dist_system.num_nodes; i++) {
        kernel_node_t *node = &dist_system.nodes[i];
        
        if (node->type == NODE_TYPE_COMPUTE && 
            node->status == NODE_STATUS_ALIVE &&
            node->load < lowest_load &&
            node->node_id != dist_system.local_node_id) {
            
            best_node = node;
            lowest_load = node->load;
        }
    }
    
    return best_node ? best_node->node_id : 0;
}

uint32_t get_current_process_id(void) {
    // Return current task ID (simplified)
    extern int current_task;
    return (uint32_t)current_task;
}