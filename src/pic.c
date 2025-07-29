#include "io.h"

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND (PIC1)
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND (PIC2)
#define PIC2_DATA    (PIC2 + 1)

// ICW1: Initialization control word 1
#define ICW1_ICW4 0x01
#define ICW1_SINGLE 0x02
#define ICW1_INTERVAL4 0x04
#define ICW1_LEVEL 0x08

// ICW4: Initialization control word 4
#define ICW4_8086 0x01

void pic_init(void) {
    // Initialize PIC1 and PIC2
    outb(PIC1_COMMAND, 0x11); // ICW1: Begin initialization
    outb(PIC2_COMMAND, 0x11); // ICW1: Begin initialization

    outb(PIC1_DATA, 0x20);    // ICW2: Master PIC offset
    outb(PIC2_DATA, 0x28);    // ICW2: Slave PIC offset

    outb(PIC1_DATA, 0x04);    // ICW3: Tell Master PIC there is a Slave PIC at IRQ2
    outb(PIC2_DATA, 0x02);    // ICW3: Tell Slave PIC its cascade identity

    outb(PIC1_DATA, ICW4_8086); // ICW4: 8086 mode
    outb(PIC2_DATA, ICW4_8086); // ICW4: 8086 mode

    // Set the PICs to operate in 8086/88 mode
    outb(PIC1_DATA, 0x00);    // OCW1: All IRQs masked
    outb(PIC2_DATA, 0x00);    // OCW1: All IRQs masked
}

// Function to send End of Interrupt (EOI) to PICs
void pic_send_eoi(int irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20); // Send EOI to Slave PIC
    }
    outb(PIC1_COMMAND, 0x20); // Send EOI to Master PIC
}
