#include "system/types.hpp"

namespace kira::kernel {

using namespace kira::system;

// Kernel main function
void main(volatile unsigned short* vga) noexcept {
    // Display main kernel messages
    const char* messages[] = {
        "KiraOS C++ Kernel v1.0",
        "=======================",
        "",
        "Kernel initialization complete.",
        "Memory management: Not implemented",
        "System ready."
    };
    
    // Display messages starting from line 2
    for (int msg_idx = 0; msg_idx < 6; msg_idx++) {
        const char* msg = messages[msg_idx];
        volatile unsigned short* pos = vga + ((msg_idx + 2) * 80); // Start from line 2
        
        for (int i = 0; msg[i] != '\0'; i++) {
            pos[i] = 0x1F00 | msg[i]; // White on blue (matching entry point style)
        }
    }
    
    // Kernel main loop - just halt and wait
    while (true) {
        asm volatile("hlt");
    }
}

} // namespace kira::kernel 