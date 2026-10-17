#ifndef TIMER_H
#define TIMER_H

struct trap_frame;

void timer_init(void);
void enable_interrupts(void);
struct trap_frame *trap_handler(unsigned long cause, unsigned long epc,
                                unsigned long tval, struct trap_frame *frame);
unsigned long get_timer_ticks(void);

#endif
