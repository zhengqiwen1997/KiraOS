#include "libkira.hpp"

namespace kira::usermode {

// Simple test user program that exercises sleep to validate resume point
void user_test_sleep() {
    UserAPI::print_colored("[user_test_sleep] start\n", Colors::CYAN_ON_BLUE);
    for (u32 i = 0; i < 5; i++) {
        UserAPI::printf("loop=%u before sleep\n", i);
        UserAPI::sleep(100);
        UserAPI::printf("loop=%u after sleep\n", i);
    }
    UserAPI::print_colored("[user_test_sleep] exit", Colors::CYAN_ON_BLUE);
    UserAPI::exit();
}

// Simple baseline program retained
void user_test_simple() {
    // Use only stack variables - no static variables that would be in kernel memory
    u32 count = 1;
    
    // Test 1: Check current privilege level
    u16 cs_register;
    asm volatile("mov %%cs, %0" : "=r"(cs_register));
    u8 privilege_level = cs_register & 0x3;  // Bottom 2 bits = CPL (Current Privilege Level)
    
    // Simple loop that doesn't access memory outside user space
    for (u32 i = 0; i < 1000; i++) {
        count++;
        asm volatile("nop");
    }
    
    // Infinite loop to keep the user process running
    while (true) {
        for (u32 i = 0; i < 100000; i++) {
            asm volatile("nop");
        }
        count++;
    }
}

} // namespace kira::usermode 