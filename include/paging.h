#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE 4096
#define PAGE_TABLE_ENTRIES 1024
#define PAGE_DIRECTORY_ENTRIES 1024

// Page table/directory entry flags
#define PAGE_PRESENT    0x1
#define PAGE_RW         0x2
#define PAGE_USER       0x4

// Page directory and table entry types
typedef uint32_t page_table_entry_t;
typedef uint32_t page_directory_entry_t;

typedef struct {
    page_table_entry_t entries[PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

typedef struct {
    page_directory_entry_t entries[PAGE_DIRECTORY_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_directory_t;

// Paging initialization
void paging_init(void);
// Page fault handler registration
void paging_install_page_fault_handler(void);
// Map a single page at a given virtual address to a physical address
void paging_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
// Helper to get the page table for a given virtual address
page_table_t* paging_get_page_table(uint32_t virt_addr, int create);

#endif // PAGING_H 