#ifndef HEAP_H
#define HEAP_H
#include <stddef.h>
void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
void heap_stats(size_t* total, size_t* used, size_t* free);
#endif // HEAP_H 