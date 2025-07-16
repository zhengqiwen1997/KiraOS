#include "libkira.hpp"

namespace kira::usermode {

// System call test program
void user_test_syscall() {
    static bool firstCall = true;
    
    if (firstCall) {
        firstCall = false;
        
        // Debug: Try to write directly to VGA buffer to see if user program is running
        volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
        const char* msg = "USER PROGRAM RUNNING!";
        for (int i = 0; msg[i] != '\0'; i++) {
            vga[19 * 80 + i] = 0x4F00 | msg[i]; // White on red
        }
        
        // Test system call
        UserAPI::write(22, 2, "Single syscall test");
        
        // Exit the program properly
        UserAPI::exit();
    }
}

} // namespace kira::usermode 