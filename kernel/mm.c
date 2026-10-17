#include "mm.h"
#include "printk.h"
#include "irq.h"

// Heap management
static struct mem_block *heap_head = 0;
static char *heap_start;
static char *heap_end;

#define PAGE_POOL_MAX_PAGES 4096
static unsigned char page_bitmap[PAGE_POOL_MAX_PAGES];
static unsigned int page_count;
static unsigned int pages_used;
static char *page_pool_start;
static char *page_pool_end;

static void page_allocator_init(void) {
    page_pool_start = &__page_pool_start;
    page_pool_end = &__page_pool_end;
    page_count = (unsigned int)((page_pool_end - page_pool_start) / PAGE_SIZE);
    if (page_count > PAGE_POOL_MAX_PAGES) page_count = PAGE_POOL_MAX_PAGES;
    pages_used = 0;
    for (unsigned int i = 0; i < page_count; i++) page_bitmap[i] = 0;
    printk("[mm] Page pool: 0x%lx - 0x%lx (%u pages)\n",
           (unsigned long)page_pool_start, (unsigned long)page_pool_end,
           page_count);
}

void mm_init(void) {
    // Use linker script symbols to determine heap bounds
    heap_start = &__heap_start;
    heap_end = &__heap_end;
    
    // Align heap start to 8 bytes
    heap_start = (char *)(((unsigned long)heap_start + 7) & ~7);
    
    printk("[mm] Heap: 0x%lx - 0x%lx (%d KB)\n", 
           (unsigned long)heap_start, 
           (unsigned long)heap_end,
           (int)(heap_end - heap_start) / 1024);
    
    // Initialize first free block
    heap_head = (struct mem_block *)heap_start;
    heap_head->size = heap_end - heap_start - sizeof(struct mem_block);
    heap_head->free = 1;
    heap_head->next = 0;

    page_allocator_init();
    
    printk("[mm] Memory manager initialized.\n");
}

void *page_alloc_n(unsigned int count) {
    if (count == 0 || count > page_count) return 0;
    irq_state_t irq = irq_save();
    unsigned int run = 0;
    unsigned int start = 0;
    for (unsigned int i = 0; i < page_count; i++) {
        if (!page_bitmap[i]) {
            if (run == 0) start = i;
            run++;
            if (run == count) {
                for (unsigned int j = start; j < start + count; j++)
                    page_bitmap[j] = 1;
                pages_used += count;
                unsigned char *result =
                    (unsigned char *)page_pool_start + start * PAGE_SIZE;
                for (unsigned long j = 0; j < (unsigned long)count * PAGE_SIZE; j++)
                    result[j] = 0;
                irq_restore(irq);
                return result;
            }
        } else {
            run = 0;
        }
    }
    irq_restore(irq);
    printk("[mm] Out of physical pages: requested %u, free %u\n",
           count, page_count - pages_used);
    return 0;
}

void *page_alloc(void) { return page_alloc_n(1); }

void page_free_n(void *page, unsigned int count) {
    if (!page || count == 0) return;
    unsigned long address = (unsigned long)page;
    unsigned long base = (unsigned long)page_pool_start;
    unsigned long end = (unsigned long)page_pool_end;
    if (address < base || address >= end || (address & (PAGE_SIZE - 1)) != 0) {
        printk("[mm] Warning: invalid page free 0x%lx\n", address);
        return;
    }
    unsigned int first = (unsigned int)((address - base) / PAGE_SIZE);
    if (count > page_count - first) {
        printk("[mm] Warning: page free range overflow\n");
        return;
    }
    irq_state_t irq = irq_save();
    for (unsigned int i = first; i < first + count; i++) {
        if (!page_bitmap[i]) {
            irq_restore(irq);
            printk("[mm] Warning: double page free at 0x%lx\n",
                   base + (unsigned long)i * PAGE_SIZE);
            return;
        }
    }
    for (unsigned int i = first; i < first + count; i++) page_bitmap[i] = 0;
    pages_used -= count;
    irq_restore(irq);
}

void page_free(void *page) { page_free_n(page, 1); }

unsigned int page_free_count(void) {
    irq_state_t irq = irq_save();
    unsigned int result = page_count - pages_used;
    irq_restore(irq);
    return result;
}

int page_allocator_validate(void) {
    irq_state_t irq = irq_save();
    unsigned int counted = 0;
    for (unsigned int i = 0; i < page_count; i++) {
        if (page_bitmap[i] > 1) {
            irq_restore(irq);
            return -1;
        }
        counted += page_bitmap[i];
    }
    int valid = counted == pages_used && pages_used <= page_count;
    irq_restore(irq);
    return valid ? 0 : -1;
}

void *kmalloc(unsigned int size) {
    if (!heap_head) {
        printk("[mm] Error: heap not initialized\n");
        return 0;
    }
    
    if (size == 0) return 0;
    if (size > (unsigned int)-1 - 7) {
        printk("[mm] Allocation size overflow\n");
        return 0;
    }

    irq_state_t irq = irq_save();

    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    struct mem_block *current = heap_head;
    struct mem_block *best_fit = 0;
    
    // First-fit allocation strategy
    while (current) {
        if (current->free && current->size >= size) {
            best_fit = current;
            break;
        }
        current = current->next;
    }
    
    if (!best_fit) {
        irq_restore(irq);
        printk("[mm] Out of memory: requested %d bytes\n", size);
        return 0;
    }
    
    // Split block if it's significantly larger than needed
    if (best_fit->size > size + sizeof(struct mem_block) + 32) {
        struct mem_block *new_block = (struct mem_block *)
            ((char *)best_fit + sizeof(struct mem_block) + size);
        
        new_block->size = best_fit->size - size - sizeof(struct mem_block);
        new_block->free = 1;
        new_block->next = best_fit->next;
        
        best_fit->size = size;
        best_fit->next = new_block;
    }
    
    best_fit->free = 0;
    
    void *result = (char *)best_fit + sizeof(struct mem_block);
    irq_restore(irq);
    return result;
}
