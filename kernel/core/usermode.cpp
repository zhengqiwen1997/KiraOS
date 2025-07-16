#include "core/usermode.hpp"

// Declare external assembly function
extern "C" void usermode_switch_asm(kira::system::u32 user_ss, kira::system::u32 user_esp, kira::system::u32 user_eflags, kira::system::u32 user_cs, kira::system::u32 user_eip);

namespace kira::system {

void UserMode::switch_to_user_mode(void* function, u32 userStack) {
    // Set up user mode segments
    u16 userDs = USER_DATA_SELECTOR;
    u16 userCs = USER_CODE_SELECTOR;
    u16 userSs = USER_DATA_SELECTOR;
    u32 userEflags = 0x202;  // Enable interrupts in user mode
    
    // Load user data segments
    asm volatile (
        "mov %0, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        :
        : "m"(userDs)
        : "eax"
    );
    
    // Call assembly function to switch to user mode
    // This will perform an IRET to enter Ring 3
    usermode_switch_asm(userSs, userStack, userEflags, userCs, (u32)function);
    
    // Should never reach here
    for(;;) {
        asm volatile("hlt");
    }
}

u32 UserMode::setup_user_stack(u32 stackBase, u32 stackSize) {
    // Return stack top (stack grows downward)
    return stackBase + stackSize - 16;  // Leave some space for alignment
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