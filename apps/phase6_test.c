#include "../lib/libc/libc.h"
#include "../lib/libhydra/hydra.h"
#include "raytracer/raytracer.h"

// Simple test to verify Phase 6 functionality
void test_raytracer_standalone(void) {
    printf("=== Testing Raytracer Components ===\n");
    
    // Test vector operations
    vec3_t v1 = {FLOAT_TO_FIXED(1.0f), FLOAT_TO_FIXED(2.0f), FLOAT_TO_FIXED(3.0f)};
    vec3_t v2 = {FLOAT_TO_FIXED(2.0f), FLOAT_TO_FIXED(1.0f), FLOAT_TO_FIXED(1.0f)};
    
    vec3_t v_add = vec3_add(v1, v2);
    printf("Vector addition: (%d, %d, %d)\n", 
           FIXED_TO_FLOAT(v_add.x), FIXED_TO_FLOAT(v_add.y), FIXED_TO_FLOAT(v_add.z));
    
    fixed_t dot = vec3_dot(v1, v2);
    printf("Dot product: %d\n", FIXED_TO_FLOAT(dot));
    
    // Test scene initialization
    setup_default_scene();
    printf("Scene initialized with %d spheres\n", num_spheres);
    
    // Test small render (reduced size for demonstration)
    const int test_width = 8;
    const int test_height = 6;
    uint8_t *test_buffer = (uint8_t*)malloc(test_width * test_height * 3);
    
    if (test_buffer) {
        printf("Rendering %dx%d test image...\n", test_width, test_height);
        
        vec3_t camera_pos = {FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(0.0f), FLOAT_TO_FIXED(0.0f)};
        vec3_t light_dir = vec3_normalize((vec3_t){FLOAT_TO_FIXED(-0.5f), FLOAT_TO_FIXED(1.0f), FLOAT_TO_FIXED(-0.5f)});
        
        int pixel_index = 0;
        for (int y = 0; y < test_height; y++) {
            for (int x = 0; x < test_width; x++) {
                fixed_t px = FLOAT_TO_FIXED((float)(x - test_width/2) / (test_width/2));
                fixed_t py = FLOAT_TO_FIXED((float)(test_height/2 - y) / (test_height/2));
                fixed_t pz = FLOAT_TO_FIXED(-1.0f);
                
                vec3_t ray_dir = {px, py, pz};
                ray_t ray = create_ray(camera_pos, ray_dir);
                
                hit_t hit = trace_ray(ray);
                int color = shade_pixel(hit, light_dir);
                
                test_buffer[pixel_index++] = (color >> 16) & 0xFF;  // R
                test_buffer[pixel_index++] = (color >> 8) & 0xFF;   // G
                test_buffer[pixel_index++] = color & 0xFF;          // B
            }
        }
        
        printf("Test render complete! Sample pixels:\n");
        for (int i = 0; i < 6 && i < test_width * test_height; i++) {
            int r = test_buffer[i*3];
            int g = test_buffer[i*3+1];
            int b = test_buffer[i*3+2];
            printf("  Pixel %d: RGB(%d,%d,%d)\n", i, r, g, b);
        }
        
        free(test_buffer);
    }
    
    printf("=== Raytracer test completed ===\n");
}

void test_image_formats(void) {
    printf("=== Testing Image Format Support ===\n");
    
    // Test PPM generation
    const int test_w = 4, test_h = 3;
    uint8_t test_data[] = {
        255, 0, 0,    0, 255, 0,    0, 0, 255,    255, 255, 0,
        255, 0, 255,  0, 255, 255,  128, 128, 128, 255, 255, 255,
        0, 0, 0,      64, 64, 64,   192, 192, 192, 255, 128, 64
    };
    
    char ppm_buffer[256];
    int header_len = sprintf(ppm_buffer, "P6\n%d %d\n255\n", test_w, test_h);
    memcpy(ppm_buffer + header_len, test_data, sizeof(test_data));
    
    printf("PPM format test: %d byte header + %d byte data = %d total\n", 
           header_len, (int)sizeof(test_data), header_len + (int)sizeof(test_data));
    printf("PPM header: %.20s...\n", ppm_buffer);
    
    printf("=== Image format test completed ===\n");
}

void phase6_unit_tests(void) {
    printf("\n*** PHASE 6 - UNIT TESTS ***\n");
    
    test_raytracer_standalone();
    test_image_formats();
    
    printf("\n*** PHASE 6 UNIT TESTS COMPLETED ***\n");
}
