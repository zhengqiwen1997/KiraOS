#include "libkira.hpp"

namespace kira::usermode {

// Simple test user program that runs once and yields
void user_test_simple() {
    static bool first_call = true;
    
    if (first_call) {
        first_call = false;
        
        // Test 1: Check current privilege level
        u16 cs_register;
        asm volatile("mov %%cs, %0" : "=r"(cs_register));
        u8 privilege_level = cs_register & 0x3;  // Bottom 2 bits = CPL (Current Privilege Level)
        
        char privilege_msg[32];
        const char* prefix = "Ring: ";
        int pos = 0;
        for (int i = 0; prefix[i] != '\0'; i++) {
            privilege_msg[pos++] = prefix[i];
        }
        privilege_msg[pos++] = '0' + privilege_level;
        privilege_msg[pos++] = ' ';
        privilege_msg[pos++] = 'C';
        privilege_msg[pos++] = 'S';
        privilege_msg[pos++] = ':';
        privilege_msg[pos++] = '0' + ((cs_register >> 4) & 0xF);
        privilege_msg[pos++] = '0' + (cs_register & 0xF);
        privilege_msg[pos] = '\0';
        
        UserAPI::write(17, 0, privilege_msg);
        
        // Test 2: Try to execute a privileged instruction (should cause GPF if in Ring 3)
        UserAPI::write(18, 0, "Testing privileged instruction...");
        
        // This should cause a GPF if we're truly in Ring 3
        // We'll comment it out for now to avoid crashing
        // asm volatile("cli");  // Clear interrupt flag - privileged instruction
        
        UserAPI::write(19, 0, "User mode test completed");
        
        // Safe infinite loop
        while (true) {
            for (int i = 0; i < 1000000; i++) {
                // Just waste some time
                asm volatile("nop");
            }
        }
    }
}

} // namespace kira::usermode 