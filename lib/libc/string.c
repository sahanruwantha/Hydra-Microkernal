#include <stddef.h>

typedef unsigned long alias_word_t __attribute__((__may_alias__));

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
