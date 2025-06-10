#include "system/types.hpp"

// VGA text mode constants
namespace {
    constexpr kira::system::u32 VGA_MEMORY_ADDR = 0xB8000;
}

// Simple VGA output for kernel main
static void kernel_vga_write_string(const char* str, int line) {
    kira::system::u16* vga = reinterpret_cast<kira::system::u16*>(VGA_MEMORY_ADDR);
    
    // Clear the line first
    int line_start = line * 80;
    for (int i = 0; i < 80; ++i) {
        vga[line_start + i] = 0x0F20; // White space
    }
    
    // Write string to the specified line
    int pos = 0;
    while (str[pos] != '\0' && pos < 80) {
        vga[line_start + pos] = 0x0F00 | str[pos]; // White on black
        pos++;
    }
}

namespace kira::kernel {

using namespace kira::system;

// Kernel main function
void main() noexcept {
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    
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
            pos[i] = 0x0F00 | msg[i]; // White on black
        }
    }
    
    // Kernel main loop - just halt and wait
    while (true) {
        asm volatile("hlt");
    }
}

} // namespace kira::kernel 