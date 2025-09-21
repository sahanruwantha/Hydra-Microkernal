#ifndef TIMER_H
#define TIMER_H

void timer_init(void);
void enable_interrupts(void);
void trap_handler(unsigned long cause, unsigned long epc, unsigned long tval);
unsigned long get_timer_ticks(void);

#endif
