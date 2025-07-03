#include "libkira.hpp"

namespace kira::usermode {

// System call test program
void user_test_syscall() {
    static bool firstCall = true;
    
    if (firstCall) {
        firstCall = false;
        
        // Test system call
        UserAPI::write(20, 0, "Single syscall test");
        
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