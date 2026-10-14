#include "vm.h"
#include "printk.h"

#define PT_ENTRIES 512
#define VPN_MASK   0x1ffUL

static pagetable_t kernel_root;
static unsigned long kernel_satp_value;

static void free_table_tree(pagetable_t table, int level);

static unsigned long align_down(unsigned long value) {
    return value & ~(PAGE_SIZE - 1);
}

static unsigned long align_up(unsigned long value) {
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static pagetable_t pte_table(pte_t pte) {
    return (pagetable_t)((pte >> 10) << 12);
}

static pte_t make_table_pte(pagetable_t table) {
    return (((unsigned long)table >> 12) << 10) | PTE_V;
}

static pte_t make_leaf_pte(unsigned long pa, unsigned long flags) {
    return ((pa >> 12) << 10) | flags | PTE_V | PTE_A | PTE_D;
}

static pte_t *walk_create(pagetable_t root, unsigned long va) {
    pagetable_t table = root;
    for (int level = 2; level > 0; level--) {
        unsigned long index = (va >> (12 + level * 9)) & VPN_MASK;
        pte_t pte = table[index];
        if (!(pte & PTE_V)) {
            pagetable_t next = (pagetable_t)page_alloc();
            if (!next) return 0;
            table[index] = make_table_pte(next);
            table = next;
        } else {
            if (pte & (PTE_R | PTE_W | PTE_X)) return 0;
            table = pte_table(pte);
        }
    }
    return &table[(va >> 12) & VPN_MASK];
}

static int map_page(pagetable_t root, unsigned long va, unsigned long pa,
                    unsigned long flags) {
    pte_t *pte = walk_create(root, va);
    if (!pte) return -1;
    *pte = make_leaf_pte(pa, flags);
    return 0;
}

static int map_range(pagetable_t root, unsigned long start,
                     unsigned long end, unsigned long flags) {
    start = align_down(start);
    end = align_up(end);
    for (unsigned long address = start; address < end; address += PAGE_SIZE) {
        if (map_page(root, address, address, flags) < 0) return -1;
    }
    return 0;
}

static pagetable_t build_kernel_space(void) {
    pagetable_t root = (pagetable_t)page_alloc();
    if (!root) return 0;

    if (map_range(root, (unsigned long)&__kernel_text_start,
                  (unsigned long)&__kernel_text_end, PTE_R | PTE_X) < 0 ||
        map_range(root, (unsigned long)&__user_text_start,
                  (unsigned long)&__user_text_end, PTE_R | PTE_X) < 0 ||
        map_range(root, (unsigned long)&__kernel_data_start,
                  (unsigned long)&__kernel_data_end, PTE_R | PTE_W) < 0 ||
        map_page(root, 0x10000000UL, 0x10000000UL, PTE_R | PTE_W) < 0) {
        free_table_tree(root, 2);
        return 0;
    }
    for (unsigned long address = 0x10001000UL;
         address <= 0x10008000UL; address += PAGE_SIZE) {
        if (map_page(root, address, address, PTE_R | PTE_W) < 0) {
            free_table_tree(root, 2);
            return 0;
        }
    }
    return root;
}

void vm_activate(unsigned long satp) {
    asm volatile("csrw satp, %0\n sfence.vma zero, zero"
                 : : "r"(satp) : "memory");
}

unsigned long vm_current_satp(void) {
    unsigned long satp;
    asm volatile("csrr %0, satp" : "=r"(satp));
    return satp;
}

unsigned long vm_kernel_satp(void) { return kernel_satp_value; }

int vm_init(void) {
    kernel_root = build_kernel_space();
    if (!kernel_root) {
        printk("[vm] Failed to construct kernel address space\n");
        return -1;
    }
    kernel_satp_value = SATP_MODE_SV39 | ((unsigned long)kernel_root >> 12);
    vm_activate(kernel_satp_value);
    printk("[vm] Sv39 enabled: kernel RX, data RW/NX, MMIO supervisor-only\n");
    return 0;
}

unsigned long vm_create_user_space(void *user_data_pages,
                                   unsigned int data_pages,
                                   void *user_stack_pages,
                                   unsigned int stack_pages) {
    if (!user_data_pages || data_pages == 0 ||
        !user_stack_pages || stack_pages == 0) return 0;
    pagetable_t root = build_kernel_space();
    if (!root) return 0;

    unsigned long code_start = (unsigned long)&__user_text_start;
    unsigned long code_end = (unsigned long)&__user_text_end;
    for (unsigned long pa = code_start, va = USER_CODE_BASE;
         pa < code_end; pa += PAGE_SIZE, va += PAGE_SIZE) {
        if (map_page(root, va, pa, PTE_R | PTE_X | PTE_U) < 0) goto fail;
    }
    unsigned long data_pa = (unsigned long)user_data_pages;
    for (unsigned int i = 0; i < data_pages; i++) {
        if (map_page(root, USER_DATA_BASE + (unsigned long)i * PAGE_SIZE,
                     data_pa + (unsigned long)i * PAGE_SIZE,
                     PTE_R | PTE_W | PTE_U) < 0) goto fail;
    }
    unsigned long stack_pa = (unsigned long)user_stack_pages;
    for (unsigned int i = 0; i < stack_pages; i++) {
        if (map_page(root, USER_STACK_BASE + (unsigned long)i * PAGE_SIZE,
                     stack_pa + (unsigned long)i * PAGE_SIZE,
                     PTE_R | PTE_W | PTE_U) < 0) goto fail;
    }
    return SATP_MODE_SV39 | ((unsigned long)root >> 12);

fail:
    free_table_tree(root, 2);
    return 0;
}

static void free_table_tree(pagetable_t table, int level) {
    if (!table || level < 0) return;
    for (int i = 0; i < PT_ENTRIES; i++) {
        pte_t pte = table[i];
        if (!(pte & PTE_V) || (pte & (PTE_R | PTE_W | PTE_X))) continue;
        free_table_tree(pte_table(pte), level - 1);
    }
    page_free(table);
}

void vm_destroy_user_space(unsigned long satp) {
    if (!satp || satp == kernel_satp_value || satp == vm_current_satp()) return;
    pagetable_t root =
        (pagetable_t)((satp & ((1UL << 44) - 1)) << 12);
    free_table_tree(root, 2);
}

unsigned long vm_user_entry(const void *physical_entry) {
    return USER_CODE_BASE + (unsigned long)physical_entry -
           (unsigned long)&__user_text_start;
}

int vm_translate(unsigned long satp, unsigned long va,
                 unsigned long *pa, unsigned long *flags) {
    pagetable_t table = (pagetable_t)((satp & ((1UL << 44) - 1)) << 12);
    for (int level = 2; level >= 0; level--) {
        pte_t pte = table[(va >> (12 + level * 9)) & VPN_MASK];
        if (!(pte & PTE_V)) return -1;
        if (pte & (PTE_R | PTE_W | PTE_X)) {
            unsigned long page_pa = (pte >> 10) << 12;
            unsigned long offset_mask = (1UL << (12 + level * 9)) - 1;
            if (pa) *pa = page_pa | (va & offset_mask);
            if (flags) *flags = pte & 0xff;
            return 0;
        }
        table = pte_table(pte);
    }
    return -1;
}

int vm_validate(void) {
    unsigned long pa, flags;
    if (!kernel_satp_value || vm_current_satp() != kernel_satp_value) return -1;
    if (vm_translate(kernel_satp_value, (unsigned long)&vm_init, &pa, &flags) < 0 ||
        !(flags & PTE_X) || (flags & (PTE_W | PTE_U))) return -1;
    if (vm_translate(kernel_satp_value, (unsigned long)&kernel_root, &pa, &flags) < 0 ||
        !(flags & PTE_W) || (flags & (PTE_X | PTE_U))) return -1;
    if (vm_translate(kernel_satp_value, 0x10000000UL, &pa, &flags) < 0 ||
        !(flags & PTE_W) || (flags & PTE_U)) return -1;
    return page_allocator_validate();
}
