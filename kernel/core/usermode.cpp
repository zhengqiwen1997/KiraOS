#include "core/usermode.hpp"
#include "core/io.hpp"
#include "arch/x86/tss.hpp"

namespace kira::system {

void UserMode::switch_to_user_mode(void* function, u32 user_stack) {
    // Debug: Show we're attempting true user mode switch
    volatile u16* vga = (volatile u16*)0xB8000;
    vga[16 * 80 + 0] = 0x0C00 | 'R';  // Red 'R' for Ring 3
    vga[16 * 80 + 1] = 0x0C00 | 'I';  // Red 'I'
    vga[16 * 80 + 2] = 0x0C00 | 'N';  // Red 'N'
    vga[16 * 80 + 3] = 0x0C00 | 'G';  // Red 'G'
    vga[16 * 80 + 4] = 0x0C00 | '3';  // Red '3'
    
    // Set up TSS with proper kernel stack for system call returns
    u32 kernel_stack = get_current_kernel_stack();
    TSSManager::set_kernel_stack(kernel_stack);
    
    // Prepare true user mode transition via IRET
    // Stack frame that IRET expects (pushed in reverse order):
    // [SS]     <- User stack segment (Ring 3)
    // [ESP]    <- User stack pointer  
    // [EFLAGS] <- Processor flags with interrupts enabled
    // [CS]     <- User code segment (Ring 3)
    // [EIP]    <- User function address
    
    u32 user_cs = 0x1B;      // User code selector (0x18 | 3 = Ring 3)
    u32 user_ss = 0x23;      // User data selector (0x20 | 3 = Ring 3)
    u32 user_eflags = 0x202; // Interrupts enabled, IOPL=0
    
    // Switch to user mode using IRET
    // This is the critical part - we're actually switching privilege levels
    asm volatile (
        // Push the IRET frame in reverse order
        "pushl %0\n\t"          // Push user SS (stack segment)
        "pushl %1\n\t"          // Push user ESP (stack pointer)
        "pushl %2\n\t"          // Push EFLAGS 
        "pushl %3\n\t"          // Push user CS (code segment)
        "pushl %4\n\t"          // Push user EIP (instruction pointer)
        
        // Set up user data segments before the switch
        "movw $0x23, %%ax\n\t"  // Load user data segment selector
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        
        // Perform the actual privilege level switch
        "iret\n\t"              // Interrupt return - switches to Ring 3!
        :
        : "r"(user_ss),         // %0 - User stack segment
          "r"(user_stack),      // %1 - User stack pointer
          "r"(user_eflags),     // %2 - Flags register
          "r"(user_cs),         // %3 - User code segment  
          "r"((u32)function)    // %4 - User function address
        : "eax", "memory"
    );
    
    // This code should NEVER be reached!
    // The user program will return to kernel via system calls (INT 0x80)
    // If we get here, something went very wrong
    vga[22 * 80 + 6] = 0x4F00 | 'E';  // White on red 'E' for ERROR
    vga[22 * 80 + 7] = 0x4F00 | 'R';  // White on red 'R'
    vga[22 * 80 + 8] = 0x4F00 | 'R';  // White on red 'R'
}

u32 UserMode::setup_user_stack(u32 stack_base, u32 stack_size) {
    // User stack grows downward, so return top of stack
    return stack_base + stack_size - 16;  // Leave some space for alignment
}

bool UserMode::is_user_mode() {
    u16 cs;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    
    // Check if we're in Ring 3 (lowest 2 bits = 11b = 3)
    return (cs & 0x03) == 0x03;
}

u32 UserMode::get_current_kernel_stack() {
    u32 esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    return esp;
}

} // namespace kira::system 