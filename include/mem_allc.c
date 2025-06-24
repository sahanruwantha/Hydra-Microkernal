#include "mem_allc.h"
#include "kernel.h"
#include "common.h"

extern char __heap_start[], __heap_end[];

void *memset(void *buf, int c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--)
        *p++ = c;
    return buf;
}

paddr_t alloc_pages(uint32_t n) {
    static paddr_t next_paddr;
    if (next_paddr == 0) {
        next_paddr = (paddr_t)(uintptr_t)__heap_start;
    }
    
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t)(uintptr_t)__heap_end)
        PANIC("out of memory");

    memset((void *)(uintptr_t)paddr, 0, n * PAGE_SIZE);
    return paddr;
}
