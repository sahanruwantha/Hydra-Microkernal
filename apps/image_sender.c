#include "../lib/libc/libc.h"
#include "../lib/libhydra/hydra.h"
#include "../lib/libnet/libnet.h"
#include "raytracer/raytracer.h"

#define DEFAULT_PORT 8080
#define CHUNK_SIZE 512

// Simple HTTP-like response for serving images
void send_http_response(int client_sock, const char *content_type, 
                       const char *data, int data_size) {
    char header[256];
    int header_len = sprintf(header, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n", 
        content_type, data_size);
    
    // Send header
    send(client_sock, header, header_len, 0);
    
    // Send data in chunks
    int sent = 0;
    while (sent < data_size) {
        int chunk_size = (data_size - sent > CHUNK_SIZE) ? CHUNK_SIZE : (data_size - sent);
        int result = send(client_sock, data + sent, chunk_size, 0);
        if (result <= 0) {
            printf("Failed to send data chunk at offset %d\n", sent);
            break;
        }
        sent += result;
        printf("Sent %d/%d bytes\n", sent, data_size);
    }
}

void image_server_demo(void) {
    printf("\n*** Phase 6 - Image Server Demo ***\n");
    
    // Initialize networking
    printf("Initializing network stack...\n");
    netlib_init();
    
    // Create TCP socket
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        printf("Failed to create server socket\n");
        return;
    }
    
    // Bind to port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    server_addr.sin_addr = 0;  // INADDR_ANY
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to bind to port %d\n", DEFAULT_PORT);
        close_user_socket(server_sock);
        return;
    }
    
    // Listen for connections
    if (listen(server_sock, 5) < 0) {
        printf("Failed to listen on socket\n");
        close_user_socket(server_sock);
        return;
    }
    
    printf("Image server listening on port %d\n", DEFAULT_PORT);
    printf("Connect with: curl http://localhost:%d/image.ppm\n", DEFAULT_PORT);
    
    // Generate raytraced image once
    printf("Pre-generating raytraced image...\n");
    raytracer_init();
    
    int image_size = IMAGE_WIDTH * IMAGE_HEIGHT * 3;
    uint8_t *image_buffer = (uint8_t*)malloc(image_size);
    
    if (!image_buffer) {
        printf("Failed to allocate image buffer\n");
        close_user_socket(server_sock);
        return;
    }
    
    render_scene(image_buffer);
    
    // Convert to PPM
    int ppm_buffer_size = image_size + 256;
    char *ppm_buffer = (char*)malloc(ppm_buffer_size);
    int ppm_size;
    
    if (!ppm_buffer) {
        printf("Failed to allocate PPM buffer\n");
        free(image_buffer);
        close_user_socket(server_sock);
        return;
    }
    
    convert_to_ppm(image_buffer, ppm_buffer, &ppm_size);
    free(image_buffer);  // Don't need raw image anymore
    
    printf("Image ready for serving (%d bytes)\n", ppm_size);
    
    // Accept connections (simplified - handle one at a time)
    int connection_count = 0;
    const int MAX_CONNECTIONS = 3;  // Limit for demo
    
    while (connection_count < MAX_CONNECTIONS) {
        printf("\nWaiting for connection %d/%d...\n", connection_count + 1, MAX_CONNECTIONS);
        
        struct sockaddr_in client_addr;
        uint32_t client_addr_len = sizeof(client_addr);
        
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            printf("Failed to accept connection\n");
            continue;
        }
        
        printf("Client connected! Serving image...\n");
        
        // Read HTTP request (simplified - we don't parse it)
        char request_buffer[512];
        int request_len = recv(client_sock, request_buffer, sizeof(request_buffer) - 1, 0);
        if (request_len > 0) {
            request_buffer[request_len] = '\0';
            printf("Received request: %.50s...\n", request_buffer);
        }
        
        // Send the raytraced image as PPM
        send_http_response(client_sock, "image/x-portable-pixmap", ppm_buffer, ppm_size);
        
        printf("Image sent to client\n");
        close_user_socket(client_sock);
        connection_count++;
    }
    
    printf("Demo complete - served %d images\n", connection_count);
    
    // Cleanup
    free(ppm_buffer);
    close_user_socket(server_sock);
}

void udp_image_broadcast_demo(void) {
    printf("\n*** UDP Image Broadcast Demo ***\n");
    
    // Create UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        printf("Failed to create UDP socket\n");
        return;
    }
    
    struct sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(8081);
    broadcast_addr.sin_addr = inet_addr("255.255.255.255");  // Broadcast
    
    printf("Broadcasting image data via UDP on port 8081...\n");
    
    // Generate a small test pattern instead of full raytraced image
    const int test_width = 16;
    const int test_height = 12;
    const int test_size = test_width * test_height * 3;
    
    uint8_t *test_image = (uint8_t*)malloc(test_size);
    if (!test_image) {
        printf("Failed to allocate test image buffer\n");
        close_user_socket(udp_sock);
        return;
    }
    
    // Generate simple test pattern
    int pixel_index = 0;
    for (int y = 0; y < test_height; y++) {
        for (int x = 0; x < test_width; x++) {
            // Create a simple gradient pattern
            uint8_t r = (x * 255) / test_width;
            uint8_t g = (y * 255) / test_height;
            uint8_t b = ((x + y) * 255) / (test_width + test_height);
            
            test_image[pixel_index++] = r;
            test_image[pixel_index++] = g;
            test_image[pixel_index++] = b;
        }
    }
    
    // Create PPM header
    char ppm_header[64];
    int header_len = sprintf(ppm_header, "P6\n%d %d\n255\n", test_width, test_height);
    
    // Send header first
    printf("Broadcasting PPM header (%d bytes)...\n", header_len);
    sendto(udp_sock, ppm_header, header_len, 0, 
           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    
    // Send image data in small chunks
    int sent = 0;
    int chunk_size = 64;  // Small chunks for UDP
    
    while (sent < test_size) {
        int current_chunk = (test_size - sent > chunk_size) ? chunk_size : (test_size - sent);
        
        int result = sendto(udp_sock, test_image + sent, current_chunk, 0,
                           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        
        if (result <= 0) {
            printf("Failed to send UDP chunk at offset %d\n", sent);
            break;
        }
        
        sent += result;
        printf("Broadcasted %d/%d bytes\n", sent, test_size);
        
        // Small delay between chunks to avoid overwhelming the network
        // (In a real OS, we'd use proper timing)
        for (volatile int i = 0; i < 100000; i++) { }
    }
    
    printf("UDP broadcast complete - sent %d bytes total\n", sent + header_len);
    
    free(test_image);
    close_user_socket(udp_sock);
}

void image_sender_demo(void) {
    printf("\n*** Phase 6 - Image Network Demo ***\n");
    printf("This demo shows:\n");
    printf("1. HTTP server serving raytraced images\n");
    printf("2. UDP broadcast of test images\n\n");
    
    // First run the HTTP server demo
    image_server_demo();
    
    // Then run the UDP broadcast demo
    udp_image_broadcast_demo();
    
    printf("\n*** Image network demos completed ***\n");
}

// Combined demo function that can be called from kernel
void phase6_demo(void) {
    printf("\n=== PHASE 6 - APPLICATIONS ===\n");
    printf("Running both raytracer and image network demos\n\n");
    
    // Run raytracer demo first
    raytracer_demo();
    
    // Run image network demo
    image_sender_demo();
    
    printf("\n=== PHASE 6 COMPLETE ===\n");
}
