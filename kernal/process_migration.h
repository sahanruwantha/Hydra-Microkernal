#ifndef PROCESS_MIGRATION_H
#define PROCESS_MIGRATION_H

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

#define MAX_SERIALIZED_SIZE 8192
#define MIGRATION_MAGIC 0xDEADBEEF

// Process types that can be migrated
typedef enum {
    PROCESS_TYPE_RAYTRACER,
    PROCESS_TYPE_COMPUTE,
    PROCESS_TYPE_NETWORK_CLIENT,
    PROCESS_TYPE_GENERIC
} process_type_t;

// Process state flags
#define PROCESS_FLAG_RUNNING     0x01
#define PROCESS_FLAG_SUSPENDED   0x02
#define PROCESS_FLAG_WAITING     0x04
#define PROCESS_FLAG_MIGRATING   0x08

// Serialized process header
typedef struct {
    uint32_t magic;             // Validation magic number
    uint32_t version;           // Serialization version
    uint32_t process_id;        // Original process ID
    process_type_t type;        // Process type
    uint32_t flags;             // Process state flags
    uint32_t data_size;         // Size of process-specific data
    uint32_t stack_size;        // Size of saved stack
    uint32_t heap_size;         // Size of saved heap
    uint64_t registers[32];     // Saved CPU registers
    uint64_t pc;                // Program counter
    uint64_t sp;                // Stack pointer
    char name[64];              // Process name
} __attribute__((packed)) process_header_t;

// Raytracer-specific state
typedef struct {
    uint32_t current_frame;
    uint32_t total_frames;
    uint32_t width;
    uint32_t height;
    uint32_t samples_per_pixel;
    uint32_t max_depth;
    float camera_pos[3];
    float camera_target[3];
    uint32_t num_spheres;
    uint8_t scene_data[2048];   // Serialized scene
    uint32_t render_progress;   // Percentage complete
} __attribute__((packed)) raytracer_state_t;

// Generic compute state
typedef struct {
    uint32_t iteration;
    uint32_t max_iterations;
    uint64_t result;
    uint8_t work_data[1024];
} __attribute__((packed)) compute_state_t;

// Complete serialized process
typedef struct {
    process_header_t header;
    union {
        raytracer_state_t raytracer;
        compute_state_t compute;
        uint8_t generic_data[4096];
    } state;
    uint8_t stack_data[2048];   // Saved stack contents
    uint8_t heap_data[2048];    // Saved heap contents
} __attribute__((packed)) serialized_process_t;

// Migration checkpoint - for saving state during migration
typedef struct {
    uint32_t process_id;
    uint64_t checkpoint_time;
    serialized_process_t data;
    bool is_valid;
} migration_checkpoint_t;

// Function declarations

// Process serialization
bool serialize_process(uint32_t process_id, serialized_process_t *output);
bool deserialize_process(const serialized_process_t *input, uint32_t *new_process_id);

// Raytracer-specific serialization
bool serialize_raytracer(uint32_t process_id, raytracer_state_t *output);
bool deserialize_raytracer(const raytracer_state_t *input, uint32_t process_id);

// Process state management
bool suspend_process_for_migration(uint32_t process_id);
bool resume_migrated_process(uint32_t process_id);
bool terminate_migrated_process(uint32_t process_id);

// Migration checkpoints
bool create_migration_checkpoint(uint32_t process_id);
bool restore_migration_checkpoint(uint32_t process_id);
void clear_migration_checkpoint(uint32_t process_id);

// Process state queries
process_type_t get_process_type(uint32_t process_id);
bool can_process_migrate(uint32_t process_id);
uint32_t estimate_migration_size(uint32_t process_id);

// Validation and integrity
bool validate_serialized_process(const serialized_process_t *process);
uint32_t calculate_process_checksum(const serialized_process_t *process);

// Migration protocol messages
typedef enum {
    MIGRATION_MSG_REQUEST,      // Request to migrate a process
    MIGRATION_MSG_ACCEPT,       // Accept migration request
    MIGRATION_MSG_REJECT,       // Reject migration request
    MIGRATION_MSG_PREPARE,      // Prepare for incoming process
    MIGRATION_MSG_DATA,         // Process data transfer
    MIGRATION_MSG_COMPLETE,     // Migration completed
    MIGRATION_MSG_ABORT,        // Abort migration
    MIGRATION_MSG_VERIFY        // Verify successful migration
} migration_msg_type_t;

typedef struct {
    migration_msg_type_t type;
    uint32_t process_id;
    uint32_t data_size;
    uint32_t sequence;
    uint32_t total_chunks;
    uint32_t chunk_index;
    uint8_t data[1024];
} __attribute__((packed)) migration_message_t;

// Migration protocol functions
void handle_distributed_migration_request(uint32_t source_node, const migration_message_t *msg);
void handle_migration_accept(uint32_t source_node, const migration_message_t *msg);
void handle_migration_data(uint32_t source_node, const migration_message_t *msg);
void handle_migration_complete(uint32_t source_node, const migration_message_t *msg);

// Utility functions
const char *process_type_to_string(process_type_t type);
const char *migration_msg_type_to_string(migration_msg_type_t type);
void print_process_state(const serialized_process_t *process);

#endif // PROCESS_MIGRATION_H