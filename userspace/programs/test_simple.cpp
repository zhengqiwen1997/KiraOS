#include "libkira.hpp"

namespace kira::usermode {

// Simple test user program that doesn't access kernel memory
void user_test_simple() {
    // Use only stack variables - no static variables that would be in kernel memory
    u32 local_count = 1;
    
    // Test 1: Check current privilege level
    u16 cs_register;
    asm volatile("mov %%cs, %0" : "=r"(cs_register));
    u8 privilege_level = cs_register & 0x3;  // Bottom 2 bits = CPL (Current Privilege Level)
    
    // Simple loop that doesn't access memory outside user space
    for (u32 i = 0; i < 1000; i++) {
        local_count++;
        asm volatile("nop");
    }
    
    // Infinite loop to keep the user process running
    while (true) {
        for (u32 i = 0; i < 100000; i++) {
            asm volatile("nop");
        }
        local_count++;
    }
}

} // namespace kira::usermode 