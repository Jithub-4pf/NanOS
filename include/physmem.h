#ifndef PHYS_MEM_H
#define PHYS_MEM_H

#include <stdint.h>
#include <stddef.h>

// Page size (4 KiB)
#define PHYS_PAGE_SIZE 4096

/**
 * Initialize the physical memory manager.
 * total_mem_bytes: total physical memory to manage (in bytes)
 * kernel_start, kernel_end: physical addresses of kernel image
 */
void physmem_init(uint32_t total_mem_bytes, uintptr_t kernel_start, uintptr_t kernel_end);

/**
 * Allocate a single 4 KiB physical page. Returns physical address, or 0 if out of memory.
 */
uintptr_t phys_alloc_page(void);

/**
 * Free a previously allocated 4 KiB physical page.
 * Warns if double-free or invalid address.
 */
void phys_free_page(uintptr_t addr);

/** Get number of free pages */
size_t phys_get_free_count(void);
/** Get total number of managed pages */
size_t phys_get_total_count(void);

/** Get the managed physical address range (start, end) */
void physmem_get_range(uintptr_t* start, uintptr_t* end);

/** Reserve a region of physical memory (for future use with memory maps) */
void physmem_reserve_region(uintptr_t start, uintptr_t end);

#endif // PHYS_MEM_H 