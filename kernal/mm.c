#include "mm.h"
#include "printk.h"

// Heap management
static struct mem_block *heap_head = 0;
static char *heap_start;
static char *heap_end;
static unsigned int total_allocated = 0;

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
    
    printk("[mm] Memory manager initialized.\n");
}

void *kmalloc(unsigned int size) {
    if (!heap_head) {
        printk("[mm] Error: heap not initialized\n");
        return 0;
    }
    
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
    total_allocated += best_fit->size;
    
    return (char *)best_fit + sizeof(struct mem_block);
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    struct mem_block *block = (struct mem_block *)((char *)ptr - sizeof(struct mem_block));
    
    if (block->free) {
        printk("[mm] Warning: double free detected\n");
        return;
    }
    
    block->free = 1;
    total_allocated -= block->size;
    
    // Coalesce with next block if it's free
    if (block->next && block->next->free) {
        block->size += block->next->size + sizeof(struct mem_block);
        block->next = block->next->next;
    }
    
    // Coalesce with previous block if it's free
    struct mem_block *current = heap_head;
    while (current && current->next != block) {
        current = current->next;
    }
    
    if (current && current->free) {
        current->size += block->size + sizeof(struct mem_block);
        current->next = block->next;
    }
}

void mm_stats(void) {
    int total_free = 0;
    int num_blocks = 0;
    int num_free_blocks = 0;
    
    struct mem_block *current = heap_head;
    while (current) {
        num_blocks++;
        if (current->free) {
            num_free_blocks++;
            total_free += current->size;
        }
        current = current->next;
    }
    
    printk("[mm] Stats: %d blocks (%d free), %d bytes allocated, %d bytes free\n",
           num_blocks, num_free_blocks, total_allocated, total_free);
}
