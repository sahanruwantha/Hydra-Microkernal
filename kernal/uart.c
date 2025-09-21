#include "uart.h"

#define UART0_BASE 0x10000000UL   // QEMU virt machine UART0 (NS16550A)
#define UART_THR   0x00           // Transmit Holding Reg
#define UART_RBR   0x00           // Receive Buffer Reg
#define UART_IER   0x01           // Interrupt Enable Reg
#define UART_LSR   0x05           // Line Status Reg
#define UART_LSR_THRE 0x20        // Transmit Holding Register Empty
#define UART_LSR_DR   0x01        // Data Ready
#define UART_IER_RDI  0x01        // Enable Received Data Interrupt

// Circular buffer for interrupt-driven input
static char input_buffer[UART_BUFFER_SIZE];
static volatile int buffer_head = 0;
static volatile int buffer_tail = 0;
static volatile int buffer_count = 0;

static inline void write_reg(unsigned long addr, unsigned char value) {
    *(volatile unsigned char *)(UART0_BASE + addr) = value;
}

static inline unsigned char read_reg(unsigned long addr) {
    return *(volatile unsigned char *)(UART0_BASE + addr);
}

void uart_init(void) {
    // Initialize input buffer
    buffer_head = 0;
    buffer_tail = 0;
    buffer_count = 0;
    
    // Enable receive data interrupt
    write_reg(UART_IER, UART_IER_RDI);
}

void uart_putc(char c) {
    // Wait until THR is empty
    while ((read_reg(UART_LSR) & UART_LSR_THRE) == 0);
    write_reg(UART_THR, c);

    // Handle newline for terminal readability
    if (c == '\n') {
        uart_putc('\r');
    }
}

char uart_getc(void) {
    // Read from interrupt-driven buffer
    if (buffer_count == 0) {
        return 0; // No data available
    }
    
    char c = input_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % UART_BUFFER_SIZE;
    buffer_count--;
    
    return c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

int uart_has_data(void) {
    return buffer_count > 0;
}

void uart_interrupt_handler(void) {
    // Check if data is available
    while (read_reg(UART_LSR) & UART_LSR_DR) {
        char c = read_reg(UART_RBR);
        
        // Add to buffer if not full
        if (buffer_count < UART_BUFFER_SIZE) {
            input_buffer[buffer_head] = c;
            buffer_head = (buffer_head + 1) % UART_BUFFER_SIZE;
            buffer_count++;
        }
        // If buffer is full, drop the character (or could overwrite oldest)
    }
}
