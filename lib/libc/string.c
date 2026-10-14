#include <stddef.h>

typedef unsigned long alias_word_t __attribute__((__may_alias__));

// String length
size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// String copy
char *strcpy(char *dest, const char *src) {
    char *original_dest = dest;
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}

// String copy with length limit
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

// String concatenation
char *strcat(char *dest, const char *src) {
    char *original_dest = dest;
    while (*dest != '\0') {
        dest++;
    }
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}

// String comparison
int strcmp(const char *str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

// String comparison with length limit
int strncmp(const char *str1, const char *str2, size_t n) {
    while (n && *str1 && (*str1 == *str2)) {
        str1++;
        str2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

// Find character in string
char *strchr(const char *str, int c) {
    while (*str != '\0') {
        if (*str == c) {
            return (char*)str;
        }
        str++;
    }
    if (c == '\0') {
        return (char*)str;
    }
    return NULL;
}

// Memory copy
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;

#if defined(MEMOPS_WORD)
    /* RISC-V handles aligned XLEN loads/stores much more efficiently than
     * the original byte-at-a-time loop. Fall back to bytes when the source
     * and destination do not share word alignment. */
    const size_t word_size = sizeof(unsigned long);
    if ((((unsigned long)d ^ (unsigned long)s) & (word_size - 1)) == 0) {
        while (n && ((unsigned long)d & (word_size - 1))) {
            *d++ = *s++;
            n--;
        }
        alias_word_t *dw = (alias_word_t *)d;
        const alias_word_t *sw = (const alias_word_t *)s;
        while (n >= word_size) {
            *dw++ = *sw++;
            n -= word_size;
        }
        d = (unsigned char *)dw;
        s = (const unsigned char *)sw;
    }
    while (n--) {
        *d++ = *s++;
    }
#else
    while (n--) *d++ = *s++;
#endif
    return dest;
}

// Memory set
void *memset(void *ptr, int value, size_t n) {
    unsigned char *p = (unsigned char*)ptr;
    unsigned char val = (unsigned char)value;

#if defined(MEMOPS_WORD)
    const size_t word_size = sizeof(unsigned long);
    while (n && ((unsigned long)p & (word_size - 1))) {
        *p++ = val;
        n--;
    }
    if (n >= word_size) {
        unsigned long pattern = val;
        pattern |= pattern << 8;
        pattern |= pattern << 16;
        if (word_size == 8) pattern |= pattern << 32;
        alias_word_t *pw = (alias_word_t *)p;
        while (n >= word_size) {
            *pw++ = pattern;
            n -= word_size;
        }
        p = (unsigned char *)pw;
    }
    while (n--) {
        *p++ = val;
    }
#else
    while (n--) *p++ = val;
#endif
    return ptr;
}

// Memory comparison
int memcmp(const void *ptr1, const void *ptr2, size_t n) {
    const unsigned char *p1 = (const unsigned char*)ptr1;
    const unsigned char *p2 = (const unsigned char*)ptr2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}
