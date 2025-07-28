#include "physmem.h"
#include <stdint.h>
#include <stddef.h>
#include "monitor.h"

#define MAX_PHYS_MEM_BYTES (32 * 1024 * 1024) // 32 MiB max for now
#define BITS_PER_BYTE 8

static uint8_t *bitmap = 0;
static size_t total_pages = 0;
static size_t free_pages = 0;
static uintptr_t phys_start = 0;
static uintptr_t phys_end = 0;

// Helper macros
#define BITMAP_INDEX(addr)   (((addr) - phys_start) / PHYS_PAGE_SIZE)
#define BITMAP_BYTE(idx)     ((idx) / BITS_PER_BYTE)
#define BITMAP_BIT(idx)      ((idx) % BITS_PER_BYTE)

/** Set bit in bitmap (mark page as used) */
static void set_bit(size_t idx) {
    bitmap[BITMAP_BYTE(idx)] |= (1 << BITMAP_BIT(idx));
}
/** Clear bit in bitmap (mark page as free) */
static void clear_bit(size_t idx) {
    bitmap[BITMAP_BYTE(idx)] &= ~(1 << BITMAP_BIT(idx));
}
/** Test if bit is set (page used) */
static int test_bit(size_t idx) {
    return (bitmap[BITMAP_BYTE(idx)] >> BITMAP_BIT(idx)) & 1;
}

/**
 * Initialize the physical memory manager.
 * total_mem_bytes: total physical memory to manage (in bytes)
 * kernel_start, kernel_end: physical addresses of kernel image
 */
void physmem_init(uint32_t total_mem_bytes, uintptr_t kernel_start, uintptr_t kernel_end) {
    if (total_mem_bytes > MAX_PHYS_MEM_BYTES) total_mem_bytes = MAX_PHYS_MEM_BYTES;
    phys_start = 0x100000; // 1 MiB, where kernel is loaded
    phys_end = phys_start + total_mem_bytes;
    total_pages = (phys_end - phys_start) / PHYS_PAGE_SIZE;
    size_t bitmap_bytes = (total_pages + 7) / 8;

    // Place bitmap just after kernel_end (rounded up to next page)
    uintptr_t bitmap_addr = (kernel_end + PHYS_PAGE_SIZE - 1) & ~(PHYS_PAGE_SIZE - 1);
    bitmap = (uint8_t *)bitmap_addr;
    // Zero bitmap
    for (size_t i = 0; i < bitmap_bytes; ++i) bitmap[i] = 0;

    // Mark all pages as free
    free_pages = total_pages;

    // Mark kernel pages as used
    size_t kernel_start_idx = BITMAP_INDEX(kernel_start);
    size_t kernel_end_idx = BITMAP_INDEX((kernel_end + PHYS_PAGE_SIZE - 1) & ~(PHYS_PAGE_SIZE - 1));
    for (size_t i = kernel_start_idx; i < kernel_end_idx; ++i) {
        if (!test_bit(i)) { set_bit(i); free_pages--; }
    }
    // Mark bitmap pages as used
    size_t bitmap_start_idx = BITMAP_INDEX(bitmap_addr);
    size_t bitmap_end_idx = BITMAP_INDEX(bitmap_addr + bitmap_bytes);
    for (size_t i = bitmap_start_idx; i < bitmap_end_idx; ++i) {
        if (!test_bit(i)) { set_bit(i); free_pages--; }
    }
}

/**
 * Allocate a single 4 KiB physical page. Returns physical address, or 0 if out of memory.
 */
uintptr_t phys_alloc_page(void) {
    for (size_t i = 0; i < total_pages; ++i) {
        if (!test_bit(i)) {
            set_bit(i);
            free_pages--;
            return phys_start + i * PHYS_PAGE_SIZE;
        }
    }
    monitor_write("[physmem] ERROR: Out of physical memory!\n");
    return 0; // Out of memory
}

/**
 * Free a previously allocated 4 KiB physical page.
 * Warn if double-free or invalid address.
 */
void phys_free_page(uintptr_t addr) {
    if (addr < phys_start || addr >= phys_end) {
        monitor_write("[physmem] WARNING: Attempt to free invalid address: ");
        monitor_write_hex(addr);
        monitor_write("\n");
        return;
    }
    size_t idx = BITMAP_INDEX(addr);
    if (!test_bit(idx)) {
        monitor_write("[physmem] WARNING: Double free or page already free: ");
        monitor_write_hex(addr);
        monitor_write("\n");
        return;
    }
    clear_bit(idx);
    free_pages++;
}

/** Get number of free pages */
size_t phys_get_free_count(void) { return free_pages; }
/** Get total number of managed pages */
size_t phys_get_total_count(void) { return total_pages; }

/** Get the managed physical address range (start, end) */
void physmem_get_range(uintptr_t* start, uintptr_t* end) {
    if (start) *start = phys_start;
    if (end) *end = phys_end;
}

/** Reserve a region of physical memory (for future use with memory maps) */
void physmem_reserve_region(uintptr_t start, uintptr_t end) {
    if (start < phys_start) start = phys_start;
    if (end > phys_end) end = phys_end;
    size_t start_idx = BITMAP_INDEX(start);
    size_t end_idx = BITMAP_INDEX((end + PHYS_PAGE_SIZE - 1) & ~(PHYS_PAGE_SIZE - 1));
    for (size_t i = start_idx; i < end_idx; ++i) {
        if (!test_bit(i)) { set_bit(i); free_pages--; }
    }
} 