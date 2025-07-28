#include "process.h"
#include "heap.h"
#include "monitor.h"
#include "sched.h"
#include "string.h"
#include <stddef.h>

extern uint32_t system_ticks;

static uint32_t next_pid = 1;

// Helper: find process by PID
static process_t* find_process(uint32_t pid) {
    extern process_t* process_list;
    if (!process_list) return NULL;
    process_t* p = process_list;
    do {
        if (p->pid == pid) return p;
        p = p->next;
    } while (p != process_list);
    return NULL;
}

process_t* shell_proc = NULL;

void process_init() {
    next_pid = 1;
}

process_t* process_create(void (*entry)(void), uint32_t stack_size) {
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) return 0;
    proc->pid = next_pid++;
    proc->stack_size = stack_size;
    proc->stack = (uint8_t*)kmalloc(stack_size);
    if (!proc->stack) {
        kfree(proc);
        return 0;
    }
    
    // Zero the stack
    for (uint32_t i = 0; i < stack_size; ++i) proc->stack[i] = 0;
    
    // Set up initial stack frame for context switch
    uint32_t* stack_top = (uint32_t*)(proc->stack + stack_size);
    
    // Push the entry point as return address
    *(--stack_top) = (uint32_t)entry;
    
    // Push dummy values for saved registers (ebp, ebx, esi, edi)
    *(--stack_top) = 0; // ebp
    *(--stack_top) = 0; // ebx  
    *(--stack_top) = 0; // esi
    *(--stack_top) = 0; // edi
    
    // Save the stack pointer
    proc->context.esp = (uint32_t)stack_top;
    proc->context.eip = (uint32_t)entry;
    
    proc->priority = 1;
    proc->time_slice = 5;
    proc->sleep_until = 0;
    proc->state = TASK_READY;
    proc->next = 0;
    // IPC queue init
    proc->msg_head = 0;
    proc->msg_tail = 0;
    memset(proc->msg_queue, 0, sizeof(proc->msg_queue));
    return proc;
}

void process_yield() {
    scheduler_tick();
}

void process_exit() {
    extern process_t* scheduler_current();
    process_t* cur = scheduler_current();
    cur->state = TASK_TERMINATED;
    monitor_write("[Process] Exiting: ");
    monitor_write_dec(cur->pid);
    monitor_write("\n");
    process_yield();
    while (1) { __asm__ volatile("hlt"); }
}

void process_sleep(uint32_t ticks) {
    extern uint32_t system_ticks;
    extern process_t* scheduler_current();
    process_t* cur = scheduler_current();
    cur->sleep_until = system_ticks + ticks;
    cur->state = TASK_BLOCKED;
    process_yield();
} 

int send_message(uint32_t dest_pid, const void* msg, size_t len) {
    process_t* dest = find_process(dest_pid);
    if (!dest) return -1;
    int next_head = (dest->msg_head + 1) % MSG_QUEUE_SIZE;
    if (next_head == dest->msg_tail) return -2; // Full
    message_t* m = &dest->msg_queue[dest->msg_head];
    m->from_pid = scheduler_current()->pid;
    m->len = (len > MSG_DATA_SIZE) ? MSG_DATA_SIZE : len;
    memcpy(m->data, msg, m->len);
    dest->msg_head = next_head;
    // If dest is blocked waiting for a message, wake it up
    if (dest->state == TASK_BLOCKED) dest->state = TASK_READY;
    return 0;
}

int receive_message(message_t* out_msg) {
    process_t* cur = scheduler_current();
    if (cur->msg_head == cur->msg_tail) return -1; // Empty
    message_t* m = &cur->msg_queue[cur->msg_tail];
    *out_msg = *m;
    cur->msg_tail = (cur->msg_tail + 1) % MSG_QUEUE_SIZE;
    return 0;
} 