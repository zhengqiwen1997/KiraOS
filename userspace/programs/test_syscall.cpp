#include "libkira.hpp"

namespace kira::usermode {

// System call test program
void user_test_syscall() {
    static bool firstCall = true;
    
    if (firstCall) {
        firstCall = false;
        
        // Demonstrate the enhanced UserAPI functions
        UserAPI::print_colored("=== USER PROGRAM STARTED ===", Colors::YELLOW_ON_BLUE);
        UserAPI::println("Testing enhanced UserAPI functions:");
        
        // Test basic print
        UserAPI::print("* Basic print: ");
        UserAPI::println("Hello, World!");
        
        // Test colored output
        UserAPI::print("* Colored text: ");
        UserAPI::print_colored("SUCCESS", Colors::GREEN_ON_BLUE);
        UserAPI::print(" | ");
        UserAPI::print_colored("WARNING", Colors::YELLOW_ON_BLUE);
        UserAPI::print(" | ");
        UserAPI::print_colored("ERROR", Colors::RED_ON_BLUE);
        UserAPI::println("");
        
        // Test multiple lines
        UserAPI::println("* Testing multiple lines:");
        UserAPI::println("  Line 1");
        UserAPI::println("  Line 2");
        UserAPI::println("  Line 3");
        
        // Test process info
        u32 pid = UserAPI::get_pid();
        UserAPI::print("* Process ID: ");
        
        // Simple number to string conversion
        char pidStr[10];
        int pos = 0;
        u32 temp = pid;
        
        if (temp == 0) {
            pidStr[pos++] = '0';
        } else {
            while (temp > 0) {
                pidStr[pos++] = '0' + (temp % 10);
                temp /= 10;
            }
        }
        
        // Reverse the string
        for (int i = 0; i < pos / 2; i++) {
            char swap = pidStr[i];
            pidStr[i] = pidStr[pos - 1 - i];
            pidStr[pos - 1 - i] = swap;
        }
        pidStr[pos] = '\0';
        
        UserAPI::print_colored(pidStr, Colors::MAGENTA_ON_BLUE);
        UserAPI::println("");
        
        UserAPI::print_colored("=== USER PROGRAM FINISHED ===", Colors::YELLOW_ON_BLUE);
        
        // Exit the program properly
        UserAPI::exit();
    }
}

} // namespace kira::usermode 