#ifndef _GDT_H
#define _GDT_H

#include <stdint.h>

/* Segment selector definitions */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS   0x1b
#define USER_DS   0x23

/* GDT entry structure */
typedef struct {
    uint16_t limit_low;     /* Limit */
    uint16_t base_low;      /* Base */
    uint8_t base_middle;    /* Base */
    uint8_t access;         /* Access flags */
    uint8_t granularity;    /* Granularity */
    uint8_t base_high;      /* Base */
} __attribute__((packed)) gdt_entry_t;

/* GDT pointer structure */
typedef struct {
    uint16_t limit;         /* Limit */
    uint32_t base;          /* Base */
} __attribute__((packed)) gdt_ptr_t;

void gdt_init();
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

#endif /* _GDT_H */
