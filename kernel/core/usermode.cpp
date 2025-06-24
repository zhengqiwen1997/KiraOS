#include "core/usermode.hpp"
#include "core/io.hpp"
#include "arch/x86/tss.hpp"

namespace kira::system {

void UserMode::switch_to_user_mode(void* function, u32 user_stack) {
    // Debug: Show we're trying user mode
    volatile u16* vga = (volatile u16*)0xB8000;
    vga[20 * 80 + 0] = 0x0E00 | 'T';  // Yellow 'T'
    vga[20 * 80 + 1] = 0x0E00 | 'R';  // Yellow 'R' 
    vga[20 * 80 + 2] = 0x0E00 | 'Y';  // Yellow 'Y'
    
    // For now, let's use a hybrid approach that's safer
    // Call user function in kernel mode but with proper system call interface
    
    typedef void (*UserFunction)();
    UserFunction user_func = (UserFunction)function;
    
    // Set up user segments (but stay in kernel mode)
    asm volatile (
        "mov $0x23, %%ax\n\t"      // User data selector
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        :
        :
        : "eax"
    );
    
    // Call user function
    user_func();
    
    // Restore kernel segments
    asm volatile (
        "mov $0x10, %%ax\n\t"      // Kernel data selector
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        :
        :
        : "eax"
    );
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