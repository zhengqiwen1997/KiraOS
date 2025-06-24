.section .text
.global syscall_stub

# System call interrupt handler stub
# Saves all registers and calls the C handler
syscall_stub:
    pusha                    # Save all general-purpose registers
    push %ds                 # Save data segment
    push %es                 # Save extra segment  
    push %fs                 # Save FS segment
    push %gs                 # Save GS segment
    
    mov $0x10, %ax           # Load kernel data segment
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    push %edx                # Push arg3
    push %ecx                # Push arg2
    push %ebx                # Push arg1
    push %eax                # Push syscall number
    call syscall_handler     # Call C handler
    add $16, %esp            # Clean up arguments
    
    pop %gs                  # Restore segments
    pop %fs
    pop %es
    pop %ds
    popa                     # Restore registers
    iret                     # Return from interrupt 