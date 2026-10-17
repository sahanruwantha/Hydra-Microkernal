#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *ptr, int value, size_t n);

#endif
