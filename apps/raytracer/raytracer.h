#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "../../lib/libc/libc.h"
#include "../../lib/libhydra/hydra.h"
#include "../../lib/libnet/libnet.h"

// Fixed-point math for performance (16.16 format)
typedef int32_t fixed_t;
#define FIXED_SHIFT 16
#define FIXED_ONE (1 << FIXED_SHIFT)
#define FLOAT_TO_FIXED(f) ((fixed_t)((f) * FIXED_ONE))
#define FIXED_TO_FLOAT(f) ((float)(f) / FIXED_ONE)
#define FIXED_MUL(a, b) (((int64_t)(a) * (b)) >> FIXED_SHIFT)
#define FIXED_DIV(a, b) (((int64_t)(a) << FIXED_SHIFT) / (b))

// Vector3 structure using fixed-point
typedef struct {
    fixed_t x, y, z;
} vec3_t;

// Ray structure
typedef struct {
    vec3_t origin;
    vec3_t direction;
} ray_t;

// Sphere structure
typedef struct {
    vec3_t center;
    fixed_t radius;
    int color;  // RGB packed as 0x00RRGGBB
} sphere_t;

// Hit information
typedef struct {
    int hit;
    fixed_t distance;
    vec3_t point;
    vec3_t normal;
    int color;
} hit_t;

// Image dimensions
#define IMAGE_WIDTH 64
#define IMAGE_HEIGHT 48
#define MAX_SPHERES 8

// Scene data
extern sphere_t spheres[MAX_SPHERES];
extern int num_spheres;

// Function prototypes
void raytracer_init(void);
void setup_default_scene(void);
vec3_t vec3_add(vec3_t a, vec3_t b);
vec3_t vec3_sub(vec3_t a, vec3_t b);
vec3_t vec3_mul_scalar(vec3_t v, fixed_t s);
fixed_t vec3_dot(vec3_t a, vec3_t b);
vec3_t vec3_normalize(vec3_t v);
ray_t create_ray(vec3_t origin, vec3_t direction);
hit_t intersect_sphere(ray_t ray, sphere_t sphere);
hit_t trace_ray(ray_t ray);
int shade_pixel(hit_t hit, vec3_t light_dir);
void render_scene(uint8_t *image_buffer);
void convert_to_ppm(uint8_t *image_buffer, char *ppm_buffer, int *ppm_size);
void raytracer_demo(void);

#endif // RAYTRACER_H
