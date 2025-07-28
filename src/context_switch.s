.global context_switch
.type context_switch, @function

# void context_switch(context_t* old, context_t* new)
# Arguments: old context at 4(%esp), new context at 8(%esp)
context_switch:
    movl 4(%esp), %eax      # Get old context pointer
    movl 8(%esp), %edx      # Get new context pointer
    
    # Save current context
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    
    # Save current stack pointer to old context
    movl %esp, 12(%eax)     # Save ESP to old->esp
    
    # Load new context
    movl 12(%edx), %esp     # Load ESP from new->esp
    
    # Restore registers
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    
    ret                     # Return will pop the return address from new stack 