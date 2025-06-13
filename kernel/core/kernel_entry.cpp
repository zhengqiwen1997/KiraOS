#include "system/types.hpp"

// Forward declaration of main kernel function
namespace kira::kernel {
    void main(volatile unsigned short* vga_buffer) noexcept;
}

// This is called from the custom bootloader (stage2.asm)
extern "C" __attribute__((section(".text._start"))) void _start() {
    // Initialize VGA buffer
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    
    // Clear screen with dark blue background
    for (int i = 0; i < 2000; i++) {
        vga[i] = 0x1F20; // White on blue background, space character
    }
    
    // Display kernel entry message at top
    const char* msg = "!!!KiraOS Kernel - Entry Point Reached!!!";
    volatile unsigned short* pos = vga;
    for (int i = 0; msg[i] != '\0'; i++) {
        pos[i] = 0x1F00 | msg[i]; // White on blue
    }
    
    // Call the main kernel function with VGA buffer
    kira::kernel::main(vga);
    
    // If kernel main returns, halt
    while (true) {
        asm volatile("hlt");
    }
} 