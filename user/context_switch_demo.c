#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

// Global variable to track turns
volatile int turn = 0;

// Thread function for printing 'a'
void* print_a(void* arg) {
    for(int i = 0; i < 5; i++) {
        while(turn != 0); // Wait for turn
        printf("a\n");
        fflush(stdout);
        turn = 1;
        usleep(100000); // Sleep for 100ms to make switching visible
    }
    return NULL;
}

// Thread function for printing 'b'
void* print_b(void* arg) {
    for(int i = 0; i < 5; i++) {
        while(turn != 1); // Wait for turn
        printf("b\n");
        fflush(stdout);
        turn = 0;
        usleep(100000); // Sleep for 100ms to make switching visible
    }
    return NULL;
}

int main() {
    pthread_t thread1, thread2;
    
    // Create two threads
    pthread_create(&thread1, NULL, print_a, NULL);
    pthread_create(&thread2, NULL, print_b, NULL);
    
    // Wait for both threads to complete
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    return 0;
} 