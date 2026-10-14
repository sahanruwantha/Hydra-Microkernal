#ifndef IRQ_H
#define IRQ_H

/* Single-hart interrupt critical sections. irq_save() atomically clears
 * supervisor interrupts and returns the prior sstatus value; irq_restore()
 * restores only the previous SIE state. Nesting is therefore naturally safe:
 * an inner section observes SIE clear and cannot re-enable it. */
typedef unsigned long irq_state_t;

#define SSTATUS_SIE_MASK (1UL << 1)

static inline irq_state_t irq_save(void) {
    irq_state_t previous;
    asm volatile("csrrc %0, sstatus, %1"
                 : "=r"(previous)
                 : "r"(SSTATUS_SIE_MASK)
                 : "memory");
    return previous;
}

static inline void irq_restore(irq_state_t previous) {
    if (previous & SSTATUS_SIE_MASK) {
        asm volatile("csrs sstatus, %0"
                     : : "r"(SSTATUS_SIE_MASK) : "memory");
    }
}

static inline int irq_enabled(void) {
    irq_state_t status;
    asm volatile("csrr %0, sstatus" : "=r"(status));
    return (status & SSTATUS_SIE_MASK) != 0;
}

#endif
