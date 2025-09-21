#include <stddef.h>

// Forward declarations for kernel memory functions
extern void *kmalloc(size_t size);
extern void kfree(void *ptr);

// User-space malloc - wrapper around kernel malloc
void *malloc(size_t size) {
    return kmalloc(size);
}

// User-space free - wrapper around kernel free
void free(void *ptr) {
    kfree(ptr);
}

// Calloc - allocate and zero memory
void *calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = malloc(total_size);
    
    if (ptr) {
        // Zero the memory
        char *byte_ptr = (char*)ptr;
        for (size_t i = 0; i < total_size; i++) {
            byte_ptr[i] = 0;
        }
    }
    
    return ptr;
}

// Realloc - resize memory block
void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    // Allocate new memory
    void *new_ptr = malloc(size);
    if (!new_ptr) {
        return NULL;
    }
    
    // Copy old data (we don't know the old size, so we'll copy up to the new size)
    // This is a simplified implementation
    char *old_bytes = (char*)ptr;
    char *new_bytes = (char*)new_ptr;
    for (size_t i = 0; i < size; i++) {
        new_bytes[i] = old_bytes[i];
    }
    
    free(ptr);
    return new_ptr;
}

// String to integer conversion
int atoi(const char *str) {
    int result = 0;
    int sign = 1;
    
    // Skip whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}

// Integer to string conversion
char *itoa(int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int tmp_value;
    
    // Validate base
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    
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
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);
    
    *ptr-- = '\0';
    
    // Reverse the string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

// Absolute value
int abs(int x) {
    return (x < 0) ? -x : x;
}

// Exit function
void exit(int status) {
    // Call system exit
    extern void syscall_exit(void);
    syscall_exit();
    
    // Should never reach here
    while (1);
}