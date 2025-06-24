#include <stdint.h>
#include <stddef.h>

// Matches the pushed register order in trap_entry.S
typedef struct trap_frame {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp;
    uint64_t rdx, rcx, rbx, rax;
} trap_frame_t;

// This gets called from assembly with a pointer to the trap frame
void handle_trap() {
    // For debugging: inspect or log register state
    // Replace this with real logic
    // Example: print the trap frame (requires serial output or framebuffer)
    
    // For now just halt
    while (1) {
        __asm__ volatile ("hlt");
    }
}
