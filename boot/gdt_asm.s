.section .text
.global gdt_flush
gdt_flush:
    movl 4(%esp), %eax
    lgdt (%eax)                # Load the GDT descriptor
    mov $0x10, %ax           # Load the data segment selector (0x10) into %ax
    mov %ax, %ds             # Update the data segment register
    mov %ax, %es             # Update the extra segment register
    mov %ax, %fs             # Update the FS segment register
    mov %ax, %gs             # Update the GS segment register
    mov %ax, %ss             # Update the stack segment register
    ljmp $0x08, $next        # Long jump to the code segment selector (0x08)
next:
    ret
