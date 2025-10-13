#ifndef DISTRIBUTED_RAYTRACER_H
#define DISTRIBUTED_RAYTRACER_H

#include <stdint.h>
#include <stdbool.h>

// Function declarations for distributed raytracer
void distributed_raytracer_init(uint32_t node_id, uint32_t render_id);
void distributed_raytracer_main(void);
bool distributed_raytracer_render_chunk(void);

// Migration support functions
bool save_raytracer_checkpoint(void);
bool restore_raytracer_checkpoint(uint32_t new_node_id);
bool should_migrate_during_render(void);

// Output and status
void save_raytracer_output(void);
void print_raytracer_status(void);

// Migration interface
bool raytracer_can_migrate(void);
uint32_t raytracer_get_migration_size(void);
bool raytracer_serialize_for_migration(void *buffer, uint32_t buffer_size);
bool raytracer_deserialize_from_migration(const void *buffer, uint32_t buffer_size, uint32_t new_node_id);

#endif // DISTRIBUTED_RAYTRACER_H