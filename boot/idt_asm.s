.extern irq_handler_next
.extern isr_handler_next
.globl isr_handler
.globl irq_handler

.macro ISR_NOERRCODE num
    .global isr\num
isr\num:
    cli
    push $0
    push $\num
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
    .global isr\num
isr\num:
    cli
    push $\num
    jmp isr_common_stub
.endm

.macro IRQ num, code
    .global irq\num
irq\num:
    cli
    push $0
    push $\code
    jmp irq_common_stub
.endm

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47
IRQ 48, 80

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31
ISR_NOERRCODE 32


isr_common_stub:
    pusha                   # Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax

    push %ds                # Save the data segment descriptor
    mov $KERNEL_DS, %ax     # Load the kernel data segment descriptor
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call isr_handler_next

    pop %ds                 # Restore the original data segment descriptor
    popa                    # Pops eax, ecx, edx, ebx, esp, ebp, esi, edi
    add $8, %esp            # Cleans up the pushed error code and pushed ISR number
    sti
    iret                    # Pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

.equ KERNEL_DS, 0x10          # Define the kernel data segment descriptor


irq_common_stub:
    pusha                    # Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    movw %ds, %ax           # Lower 16-bits of eax = ds.
    pushl %eax              # save the data segment descriptor

    movw $0x10, %ax         # load the kernel data segment descriptor
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    call irq_handler_next

    popl %ebx               # reload the original data segment descriptor
    movw %bx, %ds
    movw %bx, %es
    movw %bx, %fs
    movw %bx, %gs

    popal                   # Pops edi,esi,ebp...
    addl $8, %esp           # Cleans up the pushed error code and pushed ISR number
    sti
    iret                    # pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP



