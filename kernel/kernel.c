#include "../include/common.h"
#include "../include/kernel.h"
#include "../include/mem_allc.h"

#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_HEADER_FLAGS 0x00000003
#define MULTIBOOT_HEADER_CHECKSUM -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

#define PROCESS_READY 0
#define PROCESS_RUNNING 1

typedef struct {
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
    uint32_t state;
    uint32_t stack[256];
} Process;

Process process_a, process_b;
Process* current_process;

__attribute__((section(".multiboot")))
const uint32_t multiboot_header[] = {
    MULTIBOOT_HEADER_MAGIC,
    MULTIBOOT_HEADER_FLAGS,
    MULTIBOOT_HEADER_CHECKSUM
};

void process_a_function(void);
void process_b_function(void);
void init_process(Process* process, void (*func)(void));
void switch_process(void);

extern void context_switch(uint32_t* old_esp, uint32_t* new_esp);

void delay(int count) {
    for(volatile int i = 0; i < count * 100000; i++) { }
}

void process_a_function(void) {
    terminal_writestring("Process A started\n");
    while(1) {
        terminal_writestring("A");
        delay(1);
        switch_process();
    }
}

void process_b_function(void) {
    terminal_writestring("Process B started\n");
    while(1) {
        terminal_writestring("B");
        delay(1);
        switch_process();
    }
}

void init_process(Process* process, void (*func)(void)) {
    uint32_t* stack_ptr = &process->stack[255];
    
    *--stack_ptr = (uint32_t)(uintptr_t)func;  
    *--stack_ptr = 0;                          
    *--stack_ptr = 0;                          
    *--stack_ptr = 0;                          
    
    process->esp = (uint32_t)(uintptr_t)stack_ptr;
    process->ebp = (uint32_t)(uintptr_t)(stack_ptr + 1);  
    process->eip = (uint32_t)(uintptr_t)func;
    process->state = PROCESS_READY;
}

void switch_process(void) {
    Process* old_process = current_process;
    
    if (current_process == &process_a) {
        current_process = &process_b;
        terminal_writestring("\nSwitching to B\n");
    } else {
        current_process = &process_a;
        terminal_writestring("\nSwitching to A\n");
    }
    
    context_switch(&old_process->esp, &current_process->esp);
}

void kernel_main(void) {
    terminal_initialize();
    terminal_writestring("Kernel initialized\n");
    
    init_process(&process_a, process_a_function);
    init_process(&process_b, process_b_function);
    
    terminal_writestring("Processes initialized\n");
    
    current_process = &process_a;
    terminal_writestring("Starting process A...\n");
    
    process_a_function();
    
    terminal_writestring("!!! Kernel main returned !!!\n");
    PANIC("Kernel main returned!");
}
