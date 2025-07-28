#include "paging.h"
#include "monitor.h" // For output
#include <stdint.h>
#include "idt.h"
#include "physmem.h"
#include "heap.h"

// Extern symbols from linker script
extern uint32_t _start;
extern uint32_t _end;

#define KERNEL_HEAP_SIZE   (512 * 1024)
extern unsigned char stack_top[];

static page_directory_t __attribute__((aligned(PAGE_SIZE))) kernel_page_directory;
static page_table_t __attribute__((aligned(PAGE_SIZE))) kernel_page_tables[PAGE_DIRECTORY_ENTRIES];

// Forward declaration for page fault handler
static void page_fault_handler(registers_t regs);

// Helper to load CR3
static inline void load_cr3(uint32_t pd_phys_addr) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(pd_phys_addr));
}

// Helper to enable paging
static inline void enable_paging(void) {
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // Set PG bit
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
}

void paging_init(void) {
    // Calculate memory to map: kernel, heap, stack, and a buffer for future growth
    uint32_t mem_end = (uint32_t)&_end;
    uint32_t heap_end = mem_end + KERNEL_HEAP_SIZE;
    uint32_t stack_top_addr = (uint32_t)stack_top;
    // Stack grows down 16 KiB from stack_top
    uint32_t stack_bottom_addr = stack_top_addr - 16 * 1024;
    // Find the highest address needed
    uint32_t max_addr = heap_end;
    if (stack_top_addr > max_addr) max_addr = stack_top_addr;
    if (stack_bottom_addr > max_addr) max_addr = stack_bottom_addr;
    // Add a buffer for future allocations (e.g., 1 MiB)
    max_addr += 0x100000;
    // Round up to next 4 MiB boundary for page directory
    if (max_addr & 0x3FFFFF) max_addr = (max_addr & ~0x3FFFFF) + 0x400000;
    uint32_t num_pages = (max_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t num_tables = (num_pages + PAGE_TABLE_ENTRIES - 1) / PAGE_TABLE_ENTRIES;

    // Zero page directory and tables
    for (int i = 0; i < PAGE_DIRECTORY_ENTRIES; ++i) {
        for (int j = 0; j < PAGE_TABLE_ENTRIES; ++j) {
            kernel_page_tables[i].entries[j] = 0;
        }
        kernel_page_directory.entries[i] = 0;
    }

    // Identity map all required memory
    for (uint32_t i = 0; i < num_pages; ++i) {
        uint32_t table_idx = i / PAGE_TABLE_ENTRIES;
        uint32_t page_idx = i % PAGE_TABLE_ENTRIES;
        uint32_t phys_addr = i * PAGE_SIZE;
        kernel_page_tables[table_idx].entries[page_idx] = (phys_addr & 0xFFFFF000) | PAGE_PRESENT | PAGE_RW;
    }
    // Point directory entries to tables
    for (uint32_t i = 0; i < num_tables; ++i) {
        kernel_page_directory.entries[i] = ((uint32_t)&kernel_page_tables[i]) | PAGE_PRESENT | PAGE_RW;
    }

    // Load page directory
    load_cr3((uint32_t)&kernel_page_directory);
    // Enable paging
    enable_paging();
}

// Helper to get the page table for a given virtual address
page_table_t* paging_get_page_table(uint32_t virt_addr, int create) {
    uint32_t dir_idx = (virt_addr >> 22) & 0x3FF;
    if (!(kernel_page_directory.entries[dir_idx] & PAGE_PRESENT)) {
        if (!create) return NULL;
        // Allocate a new page table (identity-mapped for now)
        page_table_t* new_table = (page_table_t*)kmalloc(sizeof(page_table_t));
        if (!new_table) return NULL;
        for (int i = 0; i < PAGE_TABLE_ENTRIES; ++i) new_table->entries[i] = 0;
        kernel_page_directory.entries[dir_idx] = ((uint32_t)new_table) | PAGE_PRESENT | PAGE_RW;
    }
    return (page_table_t*)(kernel_page_directory.entries[dir_idx] & 0xFFFFF000);
}

// Map a single page at a given virtual address to a physical address
void paging_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    page_table_t* table = paging_get_page_table(virt_addr, 1);
    if (!table) return;
    uint32_t tbl_idx = (virt_addr >> 12) & 0x3FF;
    table->entries[tbl_idx] = (phys_addr & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;
    // Invalidate TLB for this page
    __asm__ volatile ("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

// Page fault handler (ISR 14)
static void page_fault_handler(registers_t regs) {
    uint32_t faulting_addr;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(faulting_addr));
    monitor_write("[PAGE FAULT] at address: 0x");
    monitor_write_hex(faulting_addr);
    monitor_write(", error code: 0x");
    monitor_write_hex(regs.err_code);
    monitor_write("\n");
    // If the faulting address is in the dynamic region (e.g., >= 0xC0000000), map a new page
    if (faulting_addr >= 0xC0000000) {
        uintptr_t phys = phys_alloc_page();
        if (phys == 0) {
            monitor_write("[paging] Out of memory for dynamic page!\n");
            for (;;) { __asm__ volatile ("hlt"); }
        }
        paging_map_page(faulting_addr, phys, PAGE_RW);
        monitor_write("[paging] Mapped new page for dynamic region.\n");
        return;
    }
    // Otherwise, halt
    for (;;) { __asm__ volatile ("hlt"); }
}

// Assembly stub for ISR 14 (to be added to IDT)
extern void isr14_stub(void);

// Register page fault handler in IDT
void paging_install_page_fault_handler(void) {
    register_interrupt_handler(14, page_fault_handler);
} 