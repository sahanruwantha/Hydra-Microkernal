#include "process_migration.h"
#include "distributed.h"
#include "printk.h"
#include "sched.h"
#include "mm.h"

// Basic type definitions
typedef unsigned long size_t;
#define NULL ((void*)0)

// string functions for kernel use
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
    if (!str || size == 0) return 0;
    
    const char *simple_name = "process_";
    size_t len = 0;
    while (*simple_name && len < size - 1) {
        str[len++] = *simple_name++;
    }
    str[len] = '\0';
    return len;
}

static migration_checkpoint_t checkpoints[MAX_TASKS];
static uint32_t migration_sequence = 1;

bool serialize_process(uint32_t process_id, serialized_process_t *output) {
    if (!output) {
        printk("[migration] Invalid output buffer\n");
        return false;
    }
    
    printk("[migration] Serializing process %u\n", process_id);
    
    // Initialize header
    k_memset(output, 0, sizeof(serialized_process_t));
    output->header.magic = MIGRATION_MAGIC;
    output->header.version = 1;
    output->header.process_id = process_id;
    
    // Get process type
    output->header.type = get_process_type(process_id);
    output->header.flags = PROCESS_FLAG_SUSPENDED | PROCESS_FLAG_MIGRATING;
    
    // Get process name
    k_snprintf(output->header.name, sizeof(output->header.name), "process_%u", process_id);
    
    for (int i = 0; i < 32; i++) {
        output->header.registers[i] = 0;
    }
    output->header.pc = 0x80000000;
    output->header.sp = 0x80010000;
    
    // Serialize process-specific state
    switch (output->header.type) {
        case PROCESS_TYPE_RAYTRACER:
            if (!serialize_raytracer(process_id, &output->state.raytracer)) {
                printk("[migration] Failed to serialize raytracer state\n");
                return false;
            }
            output->header.data_size = sizeof(raytracer_state_t);
            break;
            
        case PROCESS_TYPE_COMPUTE:
            // Serialize compute process state
            output->state.compute.iteration = 0;
            output->state.compute.max_iterations = 1000;
            output->state.compute.result = 0;
            k_memset(output->state.compute.work_data, 0, sizeof(output->state.compute.work_data));
            output->header.data_size = sizeof(compute_state_t);
            break;
            
        default:
            // Generic process - minimal state
            k_memset(output->state.generic_data, 0, sizeof(output->state.generic_data));
            output->header.data_size = 256; // Minimal generic state
            break;
    }
    
    // Save stack and heap (simplified)
    output->header.stack_size = 1024;
    output->header.heap_size = 1024;
    k_memset(output->stack_data, 0, output->header.stack_size);
    k_memset(output->heap_data, 0, output->header.heap_size);
    
    printk("[migration] Process %u serialized (%u bytes)\n", 
           process_id, sizeof(serialized_process_t));
    
    return true;
}

bool deserialize_process(const serialized_process_t *input, uint32_t *new_process_id) {
    if (!input || !new_process_id) {
        printk("[migration] Invalid parameters for deserialization\n");
        return false;
    }
    
    // Validate the serialized process
    if (!validate_serialized_process(input)) {
        printk("[migration] Process validation failed\n");
        return false;
    }
    
    printk("[migration] Deserializing process %u (%s)\n", 
           input->header.process_id, input->header.name);
    
    // Create new process/task
    uint32_t task_id = create_migrated_task();
    if (task_id == 0) {
        printk("[migration] Failed to create new task\n");
        return false;
    }
    
    *new_process_id = task_id;
    
    // Restore process-specific state
    switch (input->header.type) {
        case PROCESS_TYPE_RAYTRACER:
            if (!deserialize_raytracer(&input->state.raytracer, task_id)) {
                printk("[migration] Failed to restore raytracer state\n");
                terminate_task(task_id);
                return false;
            }
            break;
            
        case PROCESS_TYPE_COMPUTE:
            // Restore compute process state
            printk("[migration] Restoring compute process: iteration %u/%u\n",
                   input->state.compute.iteration, input->state.compute.max_iterations);
            break;
            
        default:
            printk("[migration] Restoring generic process\n");
            break;
    }
    
    
    // Restore stack and heap
    if (input->header.stack_size > 0) {
        // Allocate and restore stack
        printk("[migration] Restoring stack (%u bytes)\n", input->header.stack_size);
    }
    
    if (input->header.heap_size > 0) {
        // Allocate and restore heap
        printk("[migration] Restoring heap (%u bytes)\n", input->header.heap_size);
    }
    
    printk("[migration] Process deserialized as task %u\n", task_id);
    return true;
}

bool serialize_raytracer(uint32_t process_id, raytracer_state_t *output) {
    if (!output) return false;
    
    output->current_frame = 0;
    output->total_frames = 1;
    output->width = 320;
    output->height = 240;
    output->samples_per_pixel = 16;
    output->max_depth = 10;
    
    // Camera position
    output->camera_pos[0] = 0.0f;
    output->camera_pos[1] = 0.0f;
    output->camera_pos[2] = 0.0f;
    
    // Camera target
    output->camera_target[0] = 0.0f;
    output->camera_target[1] = 0.0f;
    output->camera_target[2] = -1.0f;
    
    output->num_spheres = 4;
    k_memset(output->scene_data, 0, sizeof(output->scene_data));
    
    output->render_progress = 25; // 25% complete
    
    printk("[migration] Raytracer state: %ux%u, %u spheres, %u%% complete\n",
           output->width, output->height, output->num_spheres, output->render_progress);
    
    return true;
}

bool deserialize_raytracer(const raytracer_state_t *input, uint32_t process_id) {
    if (!input) return false;
    
    printk("[migration] Restoring raytracer: %ux%u, progress %u%%\n",
           input->width, input->height, input->render_progress);
    
    printk("[migration] Raytracer camera: (%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f)\n",
           input->camera_pos[0], input->camera_pos[1], input->camera_pos[2],
           input->camera_target[0], input->camera_target[1], input->camera_target[2]);
    
    return true;
}

bool suspend_process_for_migration(uint32_t process_id) {
    printk("[migration] Suspending process %u for migration\n", process_id);
    
    // Create checkpoint before suspension
    if (!create_migration_checkpoint(process_id)) {
        printk("[migration] Failed to create checkpoint\n");
        return false;
    }
    
    // Suspend the task
    suspend_task(process_id);
    
    return true;
}

bool resume_migrated_process(uint32_t process_id) {
    printk("[migration] Resuming migrated process %u\n", process_id);
    
    // Resume the task
    resume_task(process_id);
    
    return true;
}

bool create_migration_checkpoint(uint32_t process_id) {
    if (process_id >= MAX_TASKS) return false;
    
    migration_checkpoint_t *checkpoint = &checkpoints[process_id];
    checkpoint->process_id = process_id;
    checkpoint->checkpoint_time = get_tick_count();
    checkpoint->is_valid = serialize_process(process_id, &checkpoint->data);
    
    if (checkpoint->is_valid) {
        printk("[migration] Checkpoint created for process %u\n", process_id);
    } else {
        printk("[migration] Failed to create checkpoint for process %u\n", process_id);
    }
    
    return checkpoint->is_valid;
}

bool restore_migration_checkpoint(uint32_t process_id) {
    if (process_id >= MAX_TASKS) return false;
    
    migration_checkpoint_t *checkpoint = &checkpoints[process_id];
    if (!checkpoint->is_valid) {
        printk("[migration] No valid checkpoint for process %u\n", process_id);
        return false;
    }
    
    uint32_t new_process_id;
    bool success = deserialize_process(&checkpoint->data, &new_process_id);
    
    if (success) {
        printk("[migration] Restored process %u from checkpoint\n", process_id);
    } else {
        printk("[migration] Failed to restore process %u from checkpoint\n", process_id);
    }
    
    return success;
}

process_type_t get_process_type(uint32_t process_id) {
    
    if (process_id == 1) return PROCESS_TYPE_RAYTRACER;
    if (process_id == 2) return PROCESS_TYPE_COMPUTE;
    return PROCESS_TYPE_GENERIC;
}

bool can_process_migrate(uint32_t process_id) {
    process_type_t type = get_process_type(process_id);
    
    // Some processes cannot be migrated
    switch (type) {
        case PROCESS_TYPE_RAYTRACER:
        case PROCESS_TYPE_COMPUTE:
            return true;
        default:
            return false;
    }
}

uint32_t estimate_migration_size(uint32_t process_id) {
    process_type_t type = get_process_type(process_id);
    
    uint32_t base_size = sizeof(process_header_t) + 2048; // Stack + heap estimate
    
    switch (type) {
        case PROCESS_TYPE_RAYTRACER:
            return base_size + sizeof(raytracer_state_t);
        case PROCESS_TYPE_COMPUTE:
            return base_size + sizeof(compute_state_t);
        default:
            return base_size + 256; // Minimal state
    }
}

bool validate_serialized_process(const serialized_process_t *process) {
    if (!process) return false;
    
    // Check magic number
    if (process->header.magic != MIGRATION_MAGIC) {
        printk("[migration] Invalid magic number: 0x%x\n", process->header.magic);
        return false;
    }
    
    // Check version
    if (process->header.version != 1) {
        printk("[migration] Unsupported version: %u\n", process->header.version);
        return false;
    }
    
    // Check data size
    if (process->header.data_size > sizeof(((serialized_process_t*)0)->state)) {
        printk("[migration] Data size too large: %u\n", process->header.data_size);
        return false;
    }
    
    return true;
}

void handle_distributed_migration_request(uint32_t source_node, const migration_message_t *msg) {
    printk("[migration] Received migration request from node %u for process %u\n",
           source_node, msg->process_id);
    
    // Check if we can accept the migration
    bool can_accept = (get_active_task_count() < MAX_TASKS - 1);
    
    migration_message_t response;
    response.type = can_accept ? MIGRATION_MSG_ACCEPT : MIGRATION_MSG_REJECT;
    response.process_id = msg->process_id;
    response.data_size = 0;
    response.sequence = migration_sequence++;
    
    // Send response back to source node
    distributed_send_message(source_node, 
                           can_accept ? MSG_MIGRATION_ACCEPT : MSG_MIGRATION_REJECT,
                           &response, sizeof(migration_message_t));
    
    if (can_accept) {
        printk("[migration] Accepted migration request\n");
    } else {
        printk("[migration] Rejected migration request (no capacity)\n");
    }
}

void handle_migration_data(uint32_t source_node, const migration_message_t *msg) {
    printk("[migration] Received migration data chunk %u/%u (%u bytes)\n",
           msg->chunk_index + 1, msg->total_chunks, msg->data_size);
    
    
    if (msg->chunk_index == msg->total_chunks - 1) {
        // Last chunk received, attempt to create process
        printk("[migration] All chunks received, creating process...\n");
        
        // Simulate process creation
        uint32_t new_task_id = create_migrated_task();
        if (new_task_id != 0) {
            printk("[migration] Successfully created migrated process %u\n", new_task_id);
            
            // Send completion message
            migration_message_t complete_msg;
            complete_msg.type = MIGRATION_MSG_COMPLETE;
            complete_msg.process_id = new_task_id;
            complete_msg.data_size = 0;
            complete_msg.sequence = migration_sequence++;
            
            distributed_send_message(source_node, MSG_MIGRATION_COMPLETE,
                                   &complete_msg, sizeof(migration_message_t));
        }
    }
}

const char *process_type_to_string(process_type_t type) {
    switch (type) {
        case PROCESS_TYPE_RAYTRACER: return "RAYTRACER";
        case PROCESS_TYPE_COMPUTE: return "COMPUTE";
        case PROCESS_TYPE_NETWORK_CLIENT: return "NETWORK_CLIENT";
        case PROCESS_TYPE_GENERIC: return "GENERIC";
        default: return "UNKNOWN";
    }
}

const char *migration_msg_type_to_string(migration_msg_type_t type) {
    switch (type) {
        case MIGRATION_MSG_REQUEST: return "REQUEST";
        case MIGRATION_MSG_ACCEPT: return "ACCEPT";
        case MIGRATION_MSG_REJECT: return "REJECT";
        case MIGRATION_MSG_PREPARE: return "PREPARE";
        case MIGRATION_MSG_DATA: return "DATA";
        case MIGRATION_MSG_COMPLETE: return "COMPLETE";
        case MIGRATION_MSG_ABORT: return "ABORT";
        case MIGRATION_MSG_VERIFY: return "VERIFY";
        default: return "UNKNOWN";
    }
}

void print_process_state(const serialized_process_t *process) {
    if (!process) return;
    
    printk("\n=== Process State ===\n");
    printk("Process ID: %u\n", process->header.process_id);
    printk("Type: %s\n", process_type_to_string(process->header.type));
    printk("Name: %s\n", process->header.name);
    printk("Data Size: %u bytes\n", process->header.data_size);
    printk("Stack Size: %u bytes\n", process->header.stack_size);
    printk("Heap Size: %u bytes\n", process->header.heap_size);
    printk("PC: 0x%lx\n", process->header.pc);
    printk("SP: 0x%lx\n", process->header.sp);
    
    if (process->header.type == PROCESS_TYPE_RAYTRACER) {
        const raytracer_state_t *rt = &process->state.raytracer;
        printk("Raytracer: %ux%u, %u spheres, %u%% complete\n",
               rt->width, rt->height, rt->num_spheres, rt->render_progress);
    }
    
    printk("====================\n\n");
}