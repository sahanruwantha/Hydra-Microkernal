#include <stdarg.h>
#include <stddef.h>

// Forward declarations for syscalls
extern void syscall_write(const char *str, size_t len);
extern int syscall_read(char *buffer, size_t max_len);

// Simple integer to string conversion
static char *itoa(int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int tmp_value;
    
    // Handle negative numbers for base 10
    if (value < 0 && base == 10) {
        *ptr++ = '-';
        value = -value;
        ptr1++;
    }
    
    // Convert number to string (in reverse order)
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);
    
    *ptr-- = '\0';
    
    // Reverse the string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

// Simple printf implementation
int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    char *buf_ptr = buffer;
    const char *fmt_ptr = format;
    int count = 0;
    
    while (*fmt_ptr) {
        if (*fmt_ptr == '%' && *(fmt_ptr + 1)) {
            fmt_ptr++; // Skip '%'
            
            switch (*fmt_ptr) {
                case 'd': {
                    int val = va_arg(args, int);
                    char num_str[32];
                    itoa(val, num_str, 10);
                    char *num_ptr = num_str;
                    while (*num_ptr && (buf_ptr - buffer) < sizeof(buffer) - 1) {
                        *buf_ptr++ = *num_ptr++;
                        count++;
                    }
                    break;
                }
                case 'x': {
                    int val = va_arg(args, int);
                    char num_str[32];
                    itoa(val, num_str, 16);
                    char *num_ptr = num_str;
                    while (*num_ptr && (buf_ptr - buffer) < sizeof(buffer) - 1) {
                        *buf_ptr++ = *num_ptr++;
                        count++;
                    }
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char*);
                    if (str) {
                        while (*str && (buf_ptr - buffer) < sizeof(buffer) - 1) {
                            *buf_ptr++ = *str++;
                            count++;
                        }
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    if ((buf_ptr - buffer) < sizeof(buffer) - 1) {
                        *buf_ptr++ = c;
                        count++;
                    }
                    break;
                }
                case '%':
                    if ((buf_ptr - buffer) < sizeof(buffer) - 1) {
                        *buf_ptr++ = '%';
                        count++;
                    }
                    break;
                default:
                    // Unknown format specifier, just copy as is
                    if ((buf_ptr - buffer) < sizeof(buffer) - 2) {
                        *buf_ptr++ = '%';
                        *buf_ptr++ = *fmt_ptr;
                        count += 2;
                    }
                    break;
            }
        } else {
            if ((buf_ptr - buffer) < sizeof(buffer) - 1) {
                *buf_ptr++ = *fmt_ptr;
                count++;
            }
        }
        fmt_ptr++;
    }
    
    *buf_ptr = '\0';
    syscall_write(buffer, buf_ptr - buffer);
    
    va_end(args);
    return count;
}

// Simple puts implementation
int puts(const char *str) {
    int count = 0;
    while (*str) {
        str++;
        count++;
    }
    syscall_write(str - count, count);
    syscall_write("\n", 1);
    return count + 1;
}

// Simple getchar implementation
int getchar(void) {
    char c;
    if (syscall_read(&c, 1) == 1) {
        return (int)c;
    }
    return -1; // EOF
}

// Simple putchar implementation
int putchar(int c) {
    char ch = (char)c;
    syscall_write(&ch, 1);
    return c;
}

// Simple sprintf implementation
int sprintf(char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    char *buf_ptr = str;
    const char *fmt_ptr = format;
    int count = 0;
    
    while (*fmt_ptr) {
        if (*fmt_ptr == '%' && *(fmt_ptr + 1)) {
            fmt_ptr++; // Skip '%'
            
            switch (*fmt_ptr) {
                case 'd': {
                    int val = va_arg(args, int);
                    char num_str[32];
                    itoa(val, num_str, 10);
                    char *num_ptr = num_str;
                    while (*num_ptr) {
                        *buf_ptr++ = *num_ptr++;
                        count++;
                    }
                    break;
                }
                case 'x': {
                    int val = va_arg(args, int);
                    char num_str[32];
                    itoa(val, num_str, 16);
                    char *num_ptr = num_str;
                    while (*num_ptr) {
                        *buf_ptr++ = *num_ptr++;
                        count++;
                    }
                    break;
                }
                case 's': {
                    char *string = va_arg(args, char*);
                    if (string) {
                        while (*string) {
                            *buf_ptr++ = *string++;
                            count++;
                        }
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    *buf_ptr++ = c;
                    count++;
                    break;
                }
                case '%':
                    *buf_ptr++ = '%';
                    count++;
                    break;
                default:
                    // Unknown format specifier, just copy as is
                    *buf_ptr++ = '%';
                    *buf_ptr++ = *fmt_ptr;
                    count += 2;
                    break;
            }
        } else {
            *buf_ptr++ = *fmt_ptr;
            count++;
        }
        fmt_ptr++;
    }
    
    *buf_ptr = '\0';
    
    va_end(args);
    return count;
}