#ifndef DISTRIBUTED_H
#define DISTRIBUTED_H

// Basic type definitions for cross-compilation (only if not already defined)
#ifndef UINT8_T_DEFINED
#define UINT8_T_DEFINED
typedef unsigned char uint8_t;
#endif

#ifndef UINT16_T_DEFINED
#define UINT16_T_DEFINED
typedef unsigned short uint16_t;
#endif

#ifndef UINT32_T_DEFINED
#define UINT32_T_DEFINED
typedef unsigned int uint32_t;
#endif

#ifndef UINT64_T_DEFINED
#define UINT64_T_DEFINED
typedef unsigned long uint64_t;
#endif

#ifndef BOOL_T_DEFINED
#define BOOL_T_DEFINED
typedef unsigned char bool;
#define true 1
#define false 0
#endif

#define MAX_KERNEL_NODES 8
#define MAX_MIGRATED_PROCESSES 16
#define DISCOVERY_PORT 7777
#define MIGRATION_PORT 7778
#define HEARTBEAT_INTERVAL 5000  // 5 seconds in ms

// Node types
typedef enum {
    NODE_TYPE_COMPUTE,   // Regular compute node
    NODE_TYPE_GRAPHICS,  // Graphics/rendering node
    NODE_TYPE_STORAGE,   // Storage node
    NODE_TYPE_MASTER     // Master coordinator
} node_type_t;

// Node status
typedef enum {
    NODE_STATUS_UNKNOWN,
    NODE_STATUS_ALIVE,
    NODE_STATUS_BUSY,
    NODE_STATUS_DEAD
} node_status_t;

// Kernel node information
typedef struct {
    uint32_t node_id;           // Unique node identifier
    uint32_t ip_addr;           // Network IP address
    node_type_t type;           // Node capabilities
    node_status_t status;       // Current status
    uint32_t load;              // Current load (0-100)
    uint32_t capabilities;      // Capability flags
    uint64_t last_heartbeat;    // Last heartbeat timestamp
    char name[32];              // Human-readable name
} kernel_node_t;

// Process migration state
typedef enum {
    MIGRATION_IDLE,
    MIGRATION_PREPARING,
    MIGRATION_TRANSFERRING,
    MIGRATION_COMPLETING,
    MIGRATION_FAILED
} migration_state_t;

// Migrated process information
typedef struct {
    uint32_t process_id;        // Original process ID
    uint32_t source_node;       // Source kernel node
    uint32_t target_node;       // Target kernel node
    migration_state_t state;    // Current migration state
    uint32_t data_size;         // Size of process data
    void *process_data;         // Serialized process state
    uint64_t start_time;        // Migration start time
} migrated_process_t;

// Distributed system state
typedef struct {
    uint32_t local_node_id;
    kernel_node_t nodes[MAX_KERNEL_NODES];
    int num_nodes;
    migrated_process_t migrations[MAX_MIGRATED_PROCESSES];
    int num_migrations;
    bool is_master;
    uint32_t master_node_id;
} distributed_system_t;

// Message types for distributed communication
typedef enum {
    MSG_DISCOVERY_REQUEST,
    MSG_DISCOVERY_RESPONSE,
    MSG_HEARTBEAT,
    MSG_MIGRATION_REQUEST,
    MSG_MIGRATION_ACCEPT,
    MSG_MIGRATION_REJECT,
    MSG_MIGRATION_DATA,
    MSG_MIGRATION_COMPLETE,
    MSG_LOAD_REPORT,
    MSG_SHUTDOWN_NOTIFY
} dist_msg_type_t;

// Distributed system message
typedef struct {
    dist_msg_type_t type;
    uint32_t source_node;
    uint32_t target_node;
    uint32_t sequence;
    uint32_t data_size;
    uint8_t data[512];  // Message payload
} __attribute__((packed)) dist_message_t;

// Function declarations
void distributed_init(uint32_t node_id, node_type_t type, const char *name);
void distributed_tick(void);
bool distributed_add_node(uint32_t node_id, uint32_t ip_addr, node_type_t type);
bool distributed_remove_node(uint32_t node_id);
kernel_node_t *distributed_find_node(uint32_t node_id);
kernel_node_t *distributed_find_best_node(node_type_t type);

// Process migration functions
bool migrate_process_to_node(uint32_t process_id, uint32_t target_node);
bool can_migrate_process(uint32_t process_id);
void migration_tick(void);

// Network communication
void distributed_send_message(uint32_t target_node, dist_msg_type_t type, 
                             const void *data, uint32_t size);
void distributed_handle_message(const dist_message_t *msg);

// Discovery and heartbeat
void send_discovery_request(void);
void send_heartbeat(void);
void handle_discovery_request(const dist_message_t *msg);
void handle_discovery_response(const dist_message_t *msg);
void handle_heartbeat(const dist_message_t *msg);

// Load balancing
void update_local_load(void);
uint32_t calculate_node_load(uint32_t node_id);
bool should_migrate_process(uint32_t process_id);

// Utility functions
const char *node_type_to_string(node_type_t type);
const char *node_status_to_string(node_status_t status);
const char *migration_state_to_string(migration_state_t state);
void print_distributed_status(void);

#endif // DISTRIBUTED_H