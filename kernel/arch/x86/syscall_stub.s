.section .text
.global syscall_stub
.global resume_from_syscall_stack

# System call interrupt handler stub
# Saves all registers and calls the C handler
syscall_stub:
    # Save the system call arguments in a safe way
    # EAX = syscall number, EBX = arg1, ECX = arg2, EDX = arg3
    
    # Save all registers first
    pusha                    # Save all general-purpose registers
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
    # Preserve current kernel frame ESP (after pusha/segments) in a register
    mov %esp, %edi           # %edi holds pointer to kernel frame (for resume)
    
    # Push arguments for C function call in cdecl order:
    # Last pushed becomes first parameter. Signature: (syscall_num, arg1, arg2, arg3, kernel_frame_esp)
    push %edi                # 5th param: kernel_frame_esp
    push %edx                # 4th param: arg3
    push %ecx                # 3rd param: arg2
    push %ebx                # 2nd param: arg1
    push %eax                # 1st param: syscall number
    call syscall_handler     # Call C handler
    add $20, %esp            # Clean up arguments (5 args * 4 bytes)
    
    # Let's test different offsets to find where EAX is actually saved
    # Current stack: [GS] [FS] [ES] [DS] [pusha registers...]
    # pusha saves: EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX (in that order, EAX is last)
    # So after segments: [GS] [FS] [ES] [DS] [EDI] [ESI] [EBP] [ESP] [EBX] [EDX] [ECX] [EAX]
    # EAX should be at offset: 4*4 (segments) + 7*4 (other registers) = 16 + 28 = 44 bytes
    
    # Let's try the correct offset
    mov %eax, 44(%esp)       # Store return value in saved EAX location (corrected offset)
    
    # Restore segments and registers
    pop %gs                  # Restore segments
    pop %fs
    pop %es
    pop %ds
    popa                     # Restore all general-purpose registers
    
    iret                     # Return from interrupt


# Resume helper: switch to a saved kernel stack from a prior syscall and iret
# void resume_from_syscall_stack(u32 new_esp)
resume_from_syscall_stack:
    # Args: [ESP+4] = new_esp, [ESP+8] = eax_return
    # Load return value into ECX
    mov 8(%esp), %ecx
    # Load new ESP from first argument
    mov 4(%esp), %esp
    # Store return value into saved EAX slot in pusha area (offset 44)
    mov %ecx, 44(%esp)
    # Epilogue expected after syscall handler:
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret
