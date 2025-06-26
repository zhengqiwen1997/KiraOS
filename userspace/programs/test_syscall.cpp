#include "libkira.hpp"

namespace kira::usermode {

// System call test program
void user_test_syscall() {
    static bool first_call = true;
    
    if (first_call) {
        first_call = false;
        
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