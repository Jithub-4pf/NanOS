#include <stddef.h>
#include <stdint.h>
#include "monitor.h"

// Externally defined by linker
extern uint8_t _end;

#define HEAP_SIZE (512 * 1024) // 512 KiB heap for filesystem support
#define HEAP_ALIGNMENT 8

typedef struct heap_block {
    size_t size;
    int free;
    struct heap_block* next;
} heap_block_t;

#define BLOCK_SIZE (sizeof(heap_block_t))

static uint8_t* heap_start = NULL;
static uint8_t* heap_end = NULL;
static heap_block_t* free_list = NULL;

static void split_block(heap_block_t* block, size_t size) {
    if (block->size >= size + BLOCK_SIZE + HEAP_ALIGNMENT) {
        heap_block_t* new_block = (heap_block_t*)((uint8_t*)block + BLOCK_SIZE + size);
        new_block->size = block->size - size - BLOCK_SIZE;
        new_block->free = 1;
        new_block->next = block->next;
        block->size = size;
        block->next = new_block;
    }
}

void heap_init(void) {
    heap_start = (uint8_t*)&_end;
    // Align heap_start
    uintptr_t aligned = ((uintptr_t)heap_start + (HEAP_ALIGNMENT-1)) & ~(HEAP_ALIGNMENT-1);
    heap_start = (uint8_t*)aligned;
    heap_end = heap_start + HEAP_SIZE;
    free_list = (heap_block_t*)heap_start;
    free_list->size = HEAP_SIZE - BLOCK_SIZE;
    free_list->free = 1;
    free_list->next = NULL;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    // Align size
    size = (size + (HEAP_ALIGNMENT-1)) & ~(HEAP_ALIGNMENT-1);
    heap_block_t* curr = free_list;
    while (curr) {
        if (curr->free && curr->size >= size) {
            split_block(curr, size);
            curr->free = 0;
            return (void*)((uint8_t*)curr + BLOCK_SIZE);
        }
        curr = curr->next;
    }
    monitor_write("[kmalloc] Out of heap memory!\n");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - BLOCK_SIZE);
    block->free = 1;
    // Coalesce adjacent free blocks
    heap_block_t* curr = free_list;
    while (curr) {
        if (curr->free && curr->next && curr->next->free) {
            curr->size += BLOCK_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void heap_stats(size_t* total, size_t* used, size_t* free) {
    if (total) *total = HEAP_SIZE;
    size_t u = 0, f = 0;
    heap_block_t* curr = free_list;
    while (curr) {
        if (curr->free) f += curr->size;
        else u += curr->size;
        curr = curr->next;
    }
    if (used) *used = u;
    if (free) *free = f;
}

// For future: kfree, free-list, etc. 