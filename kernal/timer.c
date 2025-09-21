#include "printk.h"
#include "sched.h"
#include "uart.h"

// RISC-V trap causes
#define CAUSE_TIMER_INTERRUPT 0x8000000000000007UL
#define CAUSE_ECALL_FROM_M    0x000000000000000BUL
#define CAUSE_UART_INTERRUPT  0x8000000000000009UL

// CLINT (Core Local Interruptor) registers for QEMU virt machine
#define CLINT_BASE 0x2000000UL
#define CLINT_MTIMECMP(hart) (CLINT_BASE + 0x4000 + 8 * (hart))
#define CLINT_MTIME (CLINT_BASE + 0xBFF8)

// Timer interval (approximately 100ms at 10MHz)
#define TIMER_INTERVAL 1000000

static unsigned long timer_ticks = 0;

// Read from memory-mapped register
static inline unsigned long read_reg(unsigned long addr) {
    return *(volatile unsigned long *)addr;
}

// Write to memory-mapped register
static inline void write_reg(unsigned long addr, unsigned long value) {
    *(volatile unsigned long *)addr = value;
}

// Enable machine timer interrupt
void timer_init(void) {
    // Set initial timer compare value (far in future to prevent immediate interrupts)
    unsigned long current_time = read_reg(CLINT_MTIME);
    write_reg(CLINT_MTIMECMP(0), current_time + (TIMER_INTERVAL * 10)); // Start with longer delay
    
    // Enable machine timer interrupt in mie
    unsigned long mie;
    asm volatile("csrr %0, mie" : "=r"(mie));
    mie |= (1UL << 7); // Set MTIE bit (bit 7)
    asm volatile("csrw mie, %0" : : "r"(mie));
    
    // Don't enable global interrupts yet - wait until system is fully initialized
    
    printk("[timer] Timer initialized with %d tick interval\n", TIMER_INTERVAL);
}

// Function to enable interrupts after full system initialization
void enable_interrupts(void) {
    // Set proper timer compare value
    unsigned long current_time = read_reg(CLINT_MTIME);
    write_reg(CLINT_MTIMECMP(0), current_time + TIMER_INTERVAL);
    
    // Enable global interrupts in mstatus
    asm volatile("csrsi mstatus, 0x8"); // Set MIE bit (bit 3)
    
    printk("[timer] Interrupts enabled\n");
}

// Main trap handler called from assembly
void trap_handler(unsigned long cause, unsigned long epc, unsigned long tval) {
    // Add debug output to see what traps are occurring
    static int trap_count = 0;
    static int in_trap = 0;
    
    // Prevent recursive traps
    if (in_trap) {
        printk("[trap] ERROR: Recursive trap detected!\n");
        asm volatile("csrci mstatus, 0x8"); // Disable interrupts
        while(1); // Halt
    }
    
    in_trap = 1;
    trap_count++;
    
    if (cause == CAUSE_TIMER_INTERRUPT) {
        // Handle timer interrupt
        timer_ticks++;
        
        // Set next timer interrupt
        unsigned long current_time = read_reg(CLINT_MTIME);
        write_reg(CLINT_MTIMECMP(0), current_time + TIMER_INTERVAL);
        
        // Limit debug output to prevent spam
        if (timer_ticks % 10 == 0) {
            printk("[trap] Timer tick %lu\n", timer_ticks);
        }
        
        // Call scheduler tick
        sched_tick();
    }
    else if (cause == CAUSE_ECALL_FROM_M) {
        // Handle ECALL (system call)
        printk("[trap] ECALL from M-mode at 0x%lx\n", epc);
        // Increment mepc to skip the ecall instruction
        asm volatile("csrw mepc, %0" : : "r"(epc + 4));
    }
    else if ((cause & 0x8000000000000000UL) && ((cause & 0xFF) == 9)) {
        // Handle external interrupt (UART)
        uart_interrupt_handler();
    }
    else {
        // Handle other traps
        printk("[trap] Unhandled trap #%d: cause=0x%lx, epc=0x%lx, tval=0x%lx\n", 
               trap_count, cause, epc, tval);
        
        // If we get too many unknown traps, something is wrong
        if (trap_count > 10) {
            printk("[trap] Too many unknown traps, disabling interrupts\n");
            asm volatile("csrci mstatus, 0x8"); // Clear MIE bit
        }
    }
    
    in_trap = 0;
}

unsigned long get_timer_ticks(void) {
    return timer_ticks;
}
