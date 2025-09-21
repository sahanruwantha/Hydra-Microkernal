#include "hydra.h"

// Forward declaration for malloc
extern void *kmalloc(size_t size);

// Capability permissions
#define CAP_READ    0x01
#define CAP_WRITE   0x02
#define CAP_EXECUTE 0x04
#define CAP_DELETE  0x08

// Capability structure (simplified)
struct capability {
    pid_t process_id;
    int permissions;
    uint64_t magic;  // Simple validation
};

#define CAPABILITY_MAGIC 0xDEADBEEFCAFEBABE

capability_t create_capability(pid_t process, int permissions) {
    struct capability *cap = (struct capability *)kmalloc(sizeof(struct capability));
    if (!cap) {
        return 0;
    }
    
    cap->process_id = process;
    cap->permissions = permissions;
    cap->magic = CAPABILITY_MAGIC;
    
    return (capability_t)cap;
}

int check_capability(capability_t cap, int operation) {
    struct capability *c = (struct capability *)cap;
    
    if (!c || c->magic != CAPABILITY_MAGIC) {
        return 0; // Invalid capability
    }
    
    return (c->permissions & operation) != 0;
}