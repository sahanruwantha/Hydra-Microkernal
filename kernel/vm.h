#ifndef VM_H
#define VM_H

#include "mm.h"

typedef unsigned long pte_t;
typedef pte_t *pagetable_t;

#define SATP_MODE_SV39       (8UL << 60)
#define PTE_V                (1UL << 0)
#define PTE_R                (1UL << 1)
#define PTE_W                (1UL << 2)
#define PTE_X                (1UL << 3)
#define PTE_U                (1UL << 4)
#define PTE_G                (1UL << 5)
#define PTE_A                (1UL << 6)
#define PTE_D                (1UL << 7)

#define USER_CODE_BASE       0x40000000UL
#define USER_DATA_BASE       0x40001000UL
#define USER_DATA_PAGES      2U
#define USER_GUARD_BASE      0x40003000UL
#define USER_STACK_BASE      0x40004000UL
#define USER_STACK_PAGES     2U

extern char __kernel_text_start;
extern char __kernel_text_end;
extern char __kernel_data_start;
extern char __kernel_data_end;
extern char __user_text_start;
extern char __user_text_end;

int vm_init(void);
unsigned long vm_kernel_satp(void);
unsigned long vm_current_satp(void);
void vm_activate(unsigned long satp);
unsigned long vm_create_user_space(void *user_data_pages,
                                   unsigned int data_pages,
                                   void *user_stack_pages,
                                   unsigned int stack_pages);
void vm_destroy_user_space(unsigned long satp);
unsigned long vm_user_entry(const void *physical_entry);
int vm_translate(unsigned long satp, unsigned long va,
                 unsigned long *pa, unsigned long *flags);
int vm_validate(void);

#endif
