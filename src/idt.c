#include <stdint.h>
#include "idt.h"
#include "io.h"
#include "monitor.h"
#include "driver.h"
#include "keyboard.h"

// Define the Interrupt Descriptor Table (IDT)
struct idt_entry_t idt[IDT_ENTRIES];
struct idt_entry_t idt_entries[IDT_ENTRIES];

isr_t interrupt_handlers[256];

// Custom string comparison for freestanding kernel
static int str_eq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        ++a; ++b;
    }
    return *a == *b;
}

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void irq48();

// Load the IDT pointer
struct idt_ptr_t idt_ptr = {
    .limit = sizeof(idt) - 1,
    .base = (uint32_t)&idt
};

void register_interrupt_handler(uint8_t n, isr_t handler)
{
    interrupt_handlers[n] = handler;
}

#define MAX_DRIVERS 16
static driver_t* driver_registry[MAX_DRIVERS];
static int driver_count = 0;

void register_driver(driver_t* drv) {
    if (driver_count < MAX_DRIVERS) {
        driver_registry[driver_count++] = drv;
    }
}

// Define an IRQ handler function
void irq_handler_next(registers_t regs)
{
    // Send an EOI (end of interrupt) signal to the PICs.
    if (regs.int_no >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);

    // Modular driver IRQ dispatch
    if (regs.int_no == 33) { // IRQ1: Keyboard
        for (int i = 0; i < driver_count; ++i) {
            if (driver_registry[i]->irq_handler && driver_registry[i]->name && str_eq(driver_registry[i]->name, "keyboard")) {
                driver_registry[i]->irq_handler();
            }
        }
    } else if (interrupt_handlers[regs.int_no] != 0) {
        isr_t handler = interrupt_handlers[regs.int_no];
        handler(regs);
    } else if (regs.int_no != 32) {
        monitor_write("Recieved IRQ: ");
        monitor_write_dec(regs.int_no); monitor_write("\n");
    }
    
    if (regs.int_no >= 8 && regs.int_no <= 15) {
        outb(PIC2_COMMAND, 0x20);
    }
    outb(PIC1_COMMAND, 0x20);
    __asm__ __volatile__("sti");
    return;
}


// This gets called from our ASM interrupt handler stub.
void isr_handler_next(registers_t regs)
{
    if (regs.int_no != 13) {
        if (interrupt_handlers[regs.int_no] != 0) {
            isr_t handler = interrupt_handlers[regs.int_no];
            handler(regs);
        } else {
            monitor_write("Recieved unhandled ISR: ");
            monitor_write_dec(regs.int_no); monitor_write("\n");
        }
    } 
    
} 


void idt_load()
{
    asm volatile("lidt %0" : : "m"(idt_ptr));
}

// Add robust default exception handler
static void default_exception_handler(registers_t regs) {
    monitor_write("[EXCEPTION] CPU Exception: ");
    monitor_write_dec(regs.int_no);
    monitor_write("\nSystem Halted.\n");
    for(;;); // Halt the system
}

void idt_init(void)
{   
    // Disable interrupts
    __asm__ __volatile__("cli");

    // Setup the PIC(s)
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);

    // Print a message to indicate that IDT is being loaded
    monitor_write("Loading IDT\n");

    // Initialize the IDT pointer
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;

    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
    idt_set_gate(80, (uint32_t)irq48, 0x08, 0x8E);

    // Send initialization control word 1 and 2 to both PICs
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // Initialization Control Word 1
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // Send initialization control word 3 to both PICs
    outb(PIC1_DATA, 0x20); // Initialization Control Word 3: IRQ 0-7 map to IDT entries 0x20-0x27
    io_wait();
    outb(PIC2_DATA, 0x28); // Initialization Control Word 3: IRQ 8-15 map to IDT entries 0x28-0x2F
    io_wait();
    

    // From @Michael_Petch on StackOverflow :D
    outb(PIC1_DATA, 4); io_wait(); outb(PIC2_DATA, 2); io_wait();

    
    // Send initialization control word 4 to both PICs
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

       // Enable keyboard interrupts
    outb(0x21, inb(0x21) & ~(1 << 1));

    // Disable the PIT
    outb(0x43, 0x30); // send the control word to channel 0
    outb(0x40, 0x00); // send the initial count value (LSB)
    outb(0x40, 0x00); // send the initial count value (MSB)
    outb(0x21, 0xFC); // mask IRQ 0 and IRQ 1 (disable the PIT)

    // Register default handlers for all CPU exceptions (0-31)
    for (int i = 0; i < 32; ++i) {
        register_interrupt_handler(i, default_exception_handler);
    }

    // Load IDT
    idt_load();

    // Ensure IRQ1 (keyboard) is unmasked after all PIC setup
    outb(0x21, inb(0x21) & ~(1 << 1));

    monitor_write("IDT loaded\n");
    return;
}




void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    // Set the base address
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;

    // Set the selector
    idt[num].sel = sel;

    // Set the always0 field
    idt[num].always0 = 0;

    // Set the flags
    idt[num].flags = flags;
}

