#include "core/usermode.hpp"
#include "core/io.hpp"
#include "arch/x86/tss.hpp"

// Declare external assembly function
extern "C" void usermode_switch_asm(kira::system::u32 user_ss, kira::system::u32 user_esp, kira::system::u32 user_eflags, kira::system::u32 user_cs, kira::system::u32 user_eip);

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
    
    // Debug: Show function address being passed
    u32 func_addr = (u32)function;
    vga[16 * 80 + 6] = 0x0F00 | 'F';  // White 'F'
    vga[16 * 80 + 7] = 0x0F00 | ':';  // White ':'
    // Show all 8 hex digits of function address
    for (int i = 0; i < 8; i++) {
        u32 digit = (func_addr >> ((7-i) * 4)) & 0xF;
        char hex_char = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        vga[16 * 80 + 8 + i] = 0x0F00 | hex_char;
    }
    
    // Show CS and SS values in the same line (moved further right to avoid overlap)
    vga[16 * 80 + 20] = 0x0A00 | 'C';  // Green 'C'
    vga[16 * 80 + 21] = 0x0F00 | ('0' + ((user_cs >> 4) & 0xF));
    vga[16 * 80 + 22] = 0x0F00 | ('0' + (user_cs & 0xF));
    vga[16 * 80 + 23] = 0x0A00 | 'S';  // Green 'S'
    vga[16 * 80 + 24] = 0x0F00 | ('0' + ((user_ss >> 4) & 0xF));
    vga[16 * 80 + 25] = 0x0F00 | ('0' + (user_ss & 0xF));
    
    // Debug: Show the exact SS value being passed (full hex)
    vga[16 * 80 + 27] = 0x0E00 | 'S';  // Yellow 'S'
    vga[16 * 80 + 28] = 0x0E00 | 'S';  // Yellow 'S'
    vga[16 * 80 + 29] = 0x0F00 | ':';  // White ':'
    vga[16 * 80 + 30] = 0x0F00 | ('0' + ((user_ss >> 4) & 0xF));
    vga[16 * 80 + 31] = 0x0F00 | ('0' + (user_ss & 0xF));
    
    // Call the assembly function to perform the switch
    usermode_switch_asm(user_ss, user_stack, user_eflags, user_cs, (u32)function);
    
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