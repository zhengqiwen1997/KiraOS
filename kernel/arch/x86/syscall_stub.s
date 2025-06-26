.section .text
.global syscall_stub

# System call interrupt handler stub
# Saves all registers and calls the C handler
syscall_stub:
    # Save the system call arguments in a safe way
    # EAX = syscall number, EBX = arg1, ECX = arg2, EDX = arg3
    
    # Save all registers first
    pusha                    # Save all general-purpose registers (EAX is now at ESP+28)
    push %ds                 # Save data segment
    push %es                 # Save extra segment  
    push %fs                 # Save FS segment
    push %gs                 # Save GS segment
    
    # Set up kernel segments WITHOUT touching the general registers
    push %eax                # Temporarily save EAX
    mov $0x10, %ax           # Load kernel data segment selector
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    pop %eax                 # Restore EAX with syscall number
    
    # Now EAX has the correct syscall number again
    # Push arguments for C function call
    push %edx                # Push arg3
    push %ecx                # Push arg2
    push %ebx                # Push arg1
    push %eax                # Push syscall number
    call syscall_handler     # Call C handler
    add $16, %esp            # Clean up arguments (4 args * 4 bytes)
    
    # Restore segments and registers
    pop %gs                  # Restore segments
    pop %fs
    pop %es
    pop %ds
    popa                     # Restore all general-purpose registers
    iret                     # Return from interrupt