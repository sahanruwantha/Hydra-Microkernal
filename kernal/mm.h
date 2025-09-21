#ifndef MM_H
#define MM_H

// Memory block header for tracking allocations
struct mem_block {
    unsigned int size;
    int free;
    struct mem_block *next;
};

// External symbols from linker script
extern char __bss_end;
extern char __heap_start;
extern char __heap_end;
extern char _stack_top;

// Function declarations
void mm_init(void);
void *kmalloc(unsigned int size);
void kfree(void *ptr);
void mm_stats(void);

#endif // MM_H
