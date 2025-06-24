#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* terminal_buffer;
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = 0x07; 
    terminal_buffer = (uint16_t*) 0xB8000;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = (uint16_t) ' ' | ((uint16_t) terminal_color << 8);
        }
    }
}


void terminal_putchar(char c) {
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = (uint16_t) c | ((uint16_t) terminal_color << 8);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = 0;
        }
    }
}

void terminal_writestring(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++)
        terminal_putchar(data[i]);
}

// Convert integer to hex string
static void int_to_hex(char* buf, unsigned int num) {
    const char hex_digits[] = "0123456789ABCDEF";
    int i = 0;
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        char rev[9];  // Max 8 hex digits for 32-bit int + null terminator
        int j = 0;
        while (num > 0) {
            rev[j++] = hex_digits[num & 0xF];
            num >>= 4;
        }
        while (--j >= 0) {
            buf[i++] = rev[j];
        }
    }
    buf[i] = '\0';
}

void terminal_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buf[32];  // Buffer for number conversion
    
    for (const char* p = format; *p != '\0'; p++) {
        if (*p != '%') {
            terminal_putchar(*p);
            continue;
        }
        
        // Handle format specifier
        p++;  // Skip '%'
        switch (*p) {
            case 'x': {  // Hex format
                unsigned int num = va_arg(args, unsigned int);
                int_to_hex(buf, num);
                terminal_writestring(buf);
                break;
            }
            case 's': {  // String format
                const char* str = va_arg(args, const char*);
                terminal_writestring(str);
                break;
            }
            case '%':  // Literal '%'
                terminal_putchar('%');
                break;
            default:
                terminal_putchar('%');
                if (*p) terminal_putchar(*p);
                break;
        }
    }
    
    va_end(args);
}
