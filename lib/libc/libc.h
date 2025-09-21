#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>
#include <stdarg.h>

// String functions
size_t strlen(const char *str);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, size_t n);
char *strchr(const char *str, int c);

// Memory functions
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *ptr, int value, size_t n);
int memcmp(const void *ptr1, const void *ptr2, size_t n);

// Memory allocation
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t num, size_t size);
void *realloc(void *ptr, size_t size);

// Standard I/O
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int puts(const char *str);
int getchar(void);
int putchar(int c);

// Utility functions
int atoi(const char *str);
char *itoa(int value, char *str, int base);
int abs(int x);
void exit(int status);

#endif // LIBC_H
