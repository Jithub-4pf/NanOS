#ifndef SCHED_H
#define SCHED_H

#include "process.h"

extern struct process* process_list;
extern struct process* current;

void scheduler_init();
void scheduler_add(process_t* proc);
void scheduler_tick(); // Called from timer IRQ
process_t* scheduler_current();
void scheduler_maybe_resched();
void timer_init();

#endif 