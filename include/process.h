#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

#define MSG_QUEUE_SIZE 8
#define MSG_DATA_SIZE 32

typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

typedef struct context {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t eip, eflags;
} context_t;

typedef struct message {
    uint32_t from_pid;
    uint32_t len;
    char data[MSG_DATA_SIZE];
} message_t;

typedef struct process {
    uint32_t pid;
    context_t context;
    uint8_t* stack;
    uint32_t stack_size;
    int priority;           // Higher = more important
    int time_slice;         // Ticks left in this quantum
    uint32_t sleep_until;   // Tick to wake up at (if sleeping)
    task_state_t state;
    struct process* next;
    // IPC message queue
    message_t msg_queue[MSG_QUEUE_SIZE];
    int msg_head;
    int msg_tail;
} process_t;

extern struct process* shell_proc;

void process_init();
process_t* process_create(void (*entry)(void), uint32_t stack_size);
void process_yield();
void process_exit();
void process_sleep(uint32_t ticks);
int send_message(uint32_t dest_pid, const void* msg, size_t len);
int receive_message(message_t* out_msg);

#ifdef __cplusplus
extern "C" {
#endif
void context_switch(context_t* old, context_t* new);
#ifdef __cplusplus
}
#endif

#endif 