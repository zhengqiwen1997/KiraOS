.section .text
.global usermode_switch_asm

# void usermode_switch_asm(u32 user_ss, u32 user_esp, u32 user_eflags, u32 user_cs, u32 user_eip)
usermode_switch_asm:
    # Function parameters are on the stack:
    # [ESP+4]  = user_ss
    # [ESP+8]  = user_esp  
    # [ESP+12] = user_eflags
    # [ESP+16] = user_cs
    # [ESP+20] = user_eip
    
    # Load parameters into registers (explicitly 32-bit)
    movl 4(%esp), %eax    # user_ss (32-bit)
    movl 8(%esp), %ebx    # user_esp (32-bit)
    movl 12(%esp), %ecx   # user_eflags (32-bit)
    movl 16(%esp), %edx   # user_cs (32-bit)
    movl 20(%esp), %esi   # user_eip (32-bit)
    
    # Build IRET frame on stack (in reverse order: SS, ESP, EFLAGS, CS, EIP)
    pushl %eax            # Push user SS (32-bit)
    pushl %ebx            # Push user ESP (32-bit)
    pushl %ecx            # Push user EFLAGS (32-bit)
    pushl %edx            # Push user CS (32-bit)
    pushl %esi            # Push user EIP (32-bit)
    
    # Set up user data segments
    mov $0x23, %ax       # User data segment selector
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    # Perform the privilege level switch
    iret                 # This should jump to user mode!
    
    # Should never reach here
    hlt 