#include <stdint.h>
#include <stddef.h>

typedef uint32_t paddr_t;
#define PAGE_SIZE 4096
    
void *memset(void *buf, int c, size_t n);
paddr_t alloc_pages(uint32_t);

