#include "sched.h"
#include "monitor.h"
#include "io.h"
#include "idt.h"
#include "process.h"
#include "heap.h"
#include <stddef.h>

#define PIT_FREQ 100
#define PIT_BASE_FREQ 1193182

process_t* process_list = 0;
process_t* current = 0;
static volatile int need_resched = 0;
uint32_t system_ticks = 0;

static void timer_irq_handler(registers_t regs) {
    system_ticks++;
    need_resched = 1;
}

void timer_init() {
    uint16_t divisor = PIT_BASE_FREQ / PIT_FREQ;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    outb(0x21, inb(0x21) & ~1);
    register_interrupt_handler(32, timer_irq_handler);
}

void scheduler_init() {
    process_list = 0;
    current = 0;
    need_resched = 0;
    system_ticks = 0;
}

void scheduler_add(process_t* proc) {
    if (!process_list) {
        process_list = proc;
        proc->next = proc;
    } else {
        process_t* tail = process_list;
        while (tail->next != process_list) tail = tail->next;
        tail->next = proc;
        proc->next = process_list;
    }
    proc->state = TASK_READY;
}

process_t* scheduler_current() {
    return current;
}

static void cleanup_terminated() {
    if (!process_list) return;
    process_t* prev = process_list;
    process_t* p = process_list->next;
    do {
        if (p->state == TASK_TERMINATED) {
            prev->next = p->next;
            if (p == process_list) process_list = p->next;
            if (p == current) current = p->next;
            kfree(p->stack);
            kfree(p);
            p = prev->next;
        } else {
            prev = p;
            p = p->next;
        }
    } while (p != process_list);
}

void scheduler_tick() {
    cleanup_terminated();
    if (!process_list) return;
    if (!current) {
        current = process_list;
        current->state = TASK_RUNNING;
        current->time_slice = 5;
        // Force immediate execution by falling through to normal scheduling
    }
    // Wake up sleeping processes
    process_t* p = process_list;
    do {
        if (p->state == TASK_BLOCKED && p->sleep_until <= system_ticks) {
            p->state = TASK_READY;
        }
        p = p->next;
    } while (p != process_list);
    // Find highest-priority ready process with time_slice > 0
    process_t* best = NULL;
    p = current;
    do {
        if (p->state == TASK_READY && p->time_slice > 0) {
            if (!best || p->priority > best->priority) best = p;
        }
        p = p->next;
    } while (p != current);
    if (!best) {
        // Reset all time slices and pick any ready process
        p = process_list;
        do {
            if (p->state == TASK_READY) p->time_slice = 5;
            p = p->next;
        } while (p != process_list);
        p = current;
        do {
            if (p->state == TASK_READY && p->time_slice > 0) {
                if (!best || p->priority > best->priority) best = p;
            }
            p = p->next;
        } while (p != current);
    }
    if (best && best != current) {
        process_t* old = current;
        old->state = TASK_READY;
        best->state = TASK_RUNNING;
        best->time_slice--;
        current = best;
        // Only print if process actually changes
        //monitor_write("[Scheduler] Switching from process ");
        //monitor_write_dec(old->pid);
        //monitor_write(" to process ");
        //monitor_write_dec(current->pid);
        //monitor_write("\n");
        context_switch(&old->context, &current->context);
    } else if (best) {
        best->time_slice--;
    }
}

void scheduler_maybe_resched() {
    if (need_resched) {
        need_resched = 0;
        process_yield();
    }
} 