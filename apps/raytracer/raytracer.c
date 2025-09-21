#include "raytracer.h"

// Scene data
sphere_t spheres[MAX_SPHERES];
int num_spheres = 0;

void raytracer_init(void) {
    printf("Initializing raytracer...\n");
    setup_default_scene();
}

void setup_default_scene(void) {
    num_spheres = 4;
    
    // Large red sphere
    spheres[0].center = (vec3_t){FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(-5.0f)};
    spheres[0].radius = FLOAT_TO_FIXED(1.0f);
    spheres[0].color = 0x00FF4040;  // Red
    
    // Small green sphere (left)
    spheres[1].center = (vec3_t){FLOAT_TO_FIXED(-2.0f), FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(-3.0f)};
    spheres[1].radius = FLOAT_TO_FIXED(0.5f);
    spheres[1].color = 0x0040FF40;  // Green
    
    // Small blue sphere (right)
    spheres[2].center = (vec3_t){FLOAT_TO_FIXED(2.0f), FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(-4.0f)};
    spheres[2].radius = FLOAT_TO_FIXED(0.7f);
    spheres[2].color = 0x004040FF;  // Blue
    
    // Ground sphere (very large, acts as plane)
    spheres[3].center = (vec3_t){FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(-1000.5f), FLOAT_TO_FIXED(-5.0f)};
    spheres[3].radius = FLOAT_TO_FIXED(1000.0f);
    spheres[3].color = 0x00808080;  // Gray
    
    printf("Scene setup complete: %d spheres\n", num_spheres);
}

// Vector operations
vec3_t vec3_add(vec3_t a, vec3_t b) {
    return (vec3_t){a.x + b.x, a.y + b.y, a.z + b.z};
}

vec3_t vec3_sub(vec3_t a, vec3_t b) {
    return (vec3_t){a.x - b.x, a.y - b.y, a.z - b.z};
}

vec3_t vec3_mul_scalar(vec3_t v, fixed_t s) {
    return (vec3_t){FIXED_MUL(v.x, s), FIXED_MUL(v.y, s), FIXED_MUL(v.z, s)};
}

fixed_t vec3_dot(vec3_t a, vec3_t b) {
    return FIXED_MUL(a.x, b.x) + FIXED_MUL(a.y, b.y) + FIXED_MUL(a.z, b.z);
}

// Simple square root approximation for fixed point
fixed_t fixed_sqrt(fixed_t x) {
    if (x <= 0) return 0;
    
    fixed_t result = x;
    fixed_t last;
    
    // Newton's method iteration (simplified)
    for (int i = 0; i < 8; i++) {
        last = result;
        result = (result + FIXED_DIV(x, result)) >> 1;
        if (result == last) break;
    }
    
    return result;
}

vec3_t vec3_normalize(vec3_t v) {
    fixed_t len_sq = vec3_dot(v, v);
    if (len_sq == 0) return v;
    
    fixed_t len = fixed_sqrt(len_sq);
    return (vec3_t){FIXED_DIV(v.x, len), FIXED_DIV(v.y, len), FIXED_DIV(v.z, len)};
}

ray_t create_ray(vec3_t origin, vec3_t direction) {
    ray_t ray;
    ray.origin = origin;
    ray.direction = vec3_normalize(direction);
    return ray;
}

hit_t intersect_sphere(ray_t ray, sphere_t sphere) {
    hit_t hit = {0};
    
    vec3_t oc = vec3_sub(ray.origin, sphere.center);
    fixed_t a = vec3_dot(ray.direction, ray.direction);
    fixed_t b = FIXED_MUL(FLOAT_TO_FIXED(2.0f), vec3_dot(oc, ray.direction));
    fixed_t c = vec3_dot(oc, oc) - FIXED_MUL(sphere.radius, sphere.radius);
    
    fixed_t discriminant = FIXED_MUL(b, b) - FIXED_MUL(FIXED_MUL(FLOAT_TO_FIXED(4.0f), a), c);
    
    if (discriminant < 0) {
        return hit;  // No intersection
    }
    
    fixed_t sqrt_disc = fixed_sqrt(discriminant);
    fixed_t t1 = FIXED_DIV(-b - sqrt_disc, FIXED_MUL(FLOAT_TO_FIXED(2.0f), a));
    fixed_t t2 = FIXED_DIV(-b + sqrt_disc, FIXED_MUL(FLOAT_TO_FIXED(2.0f), a));
    
    fixed_t t = (t1 > 0) ? t1 : t2;
    if (t <= 0) return hit;
    
    hit.hit = 1;
    hit.distance = t;
    hit.point = vec3_add(ray.origin, vec3_mul_scalar(ray.direction, t));
    hit.normal = vec3_normalize(vec3_sub(hit.point, sphere.center));
    hit.color = sphere.color;
    
    return hit;
}

hit_t trace_ray(ray_t ray) {
    hit_t closest_hit = {0};
    fixed_t closest_distance = FLOAT_TO_FIXED(1000.0f);
    
    for (int i = 0; i < num_spheres; i++) {
        hit_t hit = intersect_sphere(ray, spheres[i]);
        if (hit.hit && hit.distance < closest_distance) {
            closest_hit = hit;
            closest_distance = hit.distance;
        }
    }
    
    return closest_hit;
}

int shade_pixel(hit_t hit, vec3_t light_dir) {
    if (!hit.hit) {
        return 0x00202040;  // Sky color (dark blue)
    }
    
    // Simple diffuse lighting
    fixed_t dot_product = vec3_dot(hit.normal, light_dir);
    if (dot_product < 0) dot_product = 0;
    
    // Extract RGB components
    int r = (hit.color >> 16) & 0xFF;
    int g = (hit.color >> 8) & 0xFF;
    int b = hit.color & 0xFF;
    
    // Apply lighting (simplified)
    int intensity = FIXED_TO_FLOAT(dot_product) * 255;
    if (intensity > 255) intensity = 255;
    
    r = (r * intensity) / 255;
    g = (g * intensity) / 255;
    b = (b * intensity) / 255;
    
    // Add ambient lighting
    r += 30; if (r > 255) r = 255;
    g += 30; if (g > 255) g = 255;
    b += 30; if (b > 255) b = 255;
    
    return (r << 16) | (g << 8) | b;
}

void render_scene(uint8_t *image_buffer) {
    printf("Rendering %dx%d image...\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    
    vec3_t camera_pos = {FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(0.0f)};
    vec3_t light_dir = vec3_normalize((vec3_t){FLOAT_TO_FIXED(-0.5f), FLOAT_TO_FIXED(1.0f), FLOAT_TO_FIXED(-0.5f)});
    
    int pixel_index = 0;
    
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            // Convert pixel coordinates to world coordinates
            fixed_t px = FLOAT_TO_FIXED((float)(x - IMAGE_WIDTH/2) / (IMAGE_WIDTH/2));
            fixed_t py = FLOAT_TO_FIXED((float)(IMAGE_HEIGHT/2 - y) / (IMAGE_HEIGHT/2));
            fixed_t pz = FLOAT_TO_FIXED(-1.0f);  // Near plane
            
            vec3_t ray_dir = {px, py, pz};
            ray_t ray = create_ray(camera_pos, ray_dir);
            
            hit_t hit = trace_ray(ray);
            int color = shade_pixel(hit, light_dir);
            
            // Store RGB values in buffer
            image_buffer[pixel_index++] = (color >> 16) & 0xFF;  // R
            image_buffer[pixel_index++] = (color >> 8) & 0xFF;   // G
            image_buffer[pixel_index++] = color & 0xFF;          // B
        }
        
        // Progress indicator
        if (y % 8 == 0) {
            printf("Rendered line %d/%d\n", y, IMAGE_HEIGHT);
        }
    }
    
    printf("Rendering complete!\n");
}

void convert_to_ppm(uint8_t *image_buffer, char *ppm_buffer, int *ppm_size) {
    int header_len = sprintf(ppm_buffer, "P6\n%d %d\n255\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    
    int pixel_data_size = IMAGE_WIDTH * IMAGE_HEIGHT * 3;
    memcpy(ppm_buffer + header_len, image_buffer, pixel_data_size);
    
    *ppm_size = header_len + pixel_data_size;
    printf("PPM conversion complete, size: %d bytes\n", *ppm_size);
}

void raytracer_demo(void) {
    printf("\n*** Phase 6 - Raytracer Demo ***\n");
    
    raytracer_init();
    
    // Allocate image buffer (RGB, 3 bytes per pixel)
    int image_size = IMAGE_WIDTH * IMAGE_HEIGHT * 3;
    uint8_t *image_buffer = (uint8_t*)malloc(image_size);
    
    if (!image_buffer) {
        printf("Failed to allocate image buffer (%d bytes)\n", image_size);
        return;
    }
    
    render_scene(image_buffer);
    
    // Convert to PPM format for easy viewing/transmission
    int ppm_size;
    int ppm_buffer_size = image_size + 256;  // Extra space for header
    char *ppm_buffer = (char*)malloc(ppm_buffer_size);
    
    if (ppm_buffer) {
        convert_to_ppm(image_buffer, ppm_buffer, &ppm_size);
        printf("Raytraced image ready (%d bytes PPM format)\n", ppm_size);
        
        // Save first few bytes to show it's working
        printf("PPM header: ");
        for (int i = 0; i < 20 && i < ppm_size; i++) {
            char c = ppm_buffer[i];
            if (c >= 32 && c <= 126) {
                printf("%c", c);
            } else if (c == '\n') {
                printf("\\n");
            } else {
                printf("<%02x>", (unsigned char)c);
            }
        }
        printf("...\n");
        
        free(ppm_buffer);
    }
    
    free(image_buffer);
    printf("*** Raytracer demo completed ***\n");
}
