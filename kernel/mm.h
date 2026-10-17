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
extern char __page_pool_start;
extern char __page_pool_end;

#define PAGE_SIZE 4096UL

// Function declarations
void mm_init(void);
void *kmalloc(unsigned int size);
void *page_alloc(void);
void *page_alloc_n(unsigned int count);
void page_free(void *page);
void page_free_n(void *page, unsigned int count);
unsigned int page_free_count(void);
int page_allocator_validate(void);

#endif // MM_H
