#include "core/usermode.hpp"
#include "core/io.hpp"
#include "arch/x86/tss.hpp"

// Declare external assembly function
extern "C" void usermode_switch_asm(kira::system::u32 user_ss, kira::system::u32 user_esp, kira::system::u32 user_eflags, kira::system::u32 user_cs, kira::system::u32 user_eip);

namespace kira::system {

void UserMode::switch_to_user_mode(void* function, u32 user_stack) {
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
    
    // Call the assembly function to perform the switch
    usermode_switch_asm(user_ss, user_stack, user_eflags, user_cs, (u32)function);
    
    // This code should NEVER be reached!
    // The user program will return to kernel via system calls (INT 0x80)
    // If we get here, something went very wrong
    volatile u16* vga = (volatile u16*)0xB8000;
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