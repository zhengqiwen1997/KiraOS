#include "system/types.hpp"

// VGA text mode constants
namespace {
    constexpr kira::system::u32 VGA_MEMORY_ADDR = 0xB8000;
    constexpr kira::system::u8 VGA_COLOR = 0x0F; // White text on black background
    constexpr int VGA_WIDTH = 80;
    constexpr int VGA_HEIGHT = 25;
}

// Simple VGA text output using inline assembly for reliability
static void simple_vga_write_string(const char* str) {
    // Use inline assembly to write directly to VGA memory
    kira::system::u16* vga = reinterpret_cast<kira::system::u16*>(VGA_MEMORY_ADDR);
    
    // Clear first line
    for (int i = 0; i < VGA_WIDTH; ++i) {
        vga[i] = 0x0F20; // White space
    }
    
    // Write string to first line
    int pos = 0;
    while (str[pos] != '\0' && pos < VGA_WIDTH) {
        vga[pos] = 0x0F00 | str[pos]; // White on black
        pos++;
    }
}

// Global cursor position (shared with other kernel files)
int vga_cursor_x = 0;
int vga_cursor_y = 0;

static void vga_write_char(char c) {
    auto* vga_buffer = reinterpret_cast<kira::system::u16*>(VGA_MEMORY_ADDR);
    
    if (c == '\n') {
        vga_cursor_x = 0;
        vga_cursor_y++;
        if (vga_cursor_y >= VGA_HEIGHT) {
            vga_cursor_y = VGA_HEIGHT - 1;
        }
        return;
    }
    
    if (vga_cursor_x < VGA_WIDTH && vga_cursor_y < VGA_HEIGHT) {
        int pos = vga_cursor_y * VGA_WIDTH + vga_cursor_x;
        vga_buffer[pos] = (VGA_COLOR << 8) | c;
        vga_cursor_x++;
        if (vga_cursor_x >= VGA_WIDTH) {
            vga_cursor_x = 0;
            vga_cursor_y++;
            if (vga_cursor_y >= VGA_HEIGHT) {
                vga_cursor_y = VGA_HEIGHT - 1;
            }
        }
    }
}

static void vga_write_string(const char* str) {
    for (int i = 0; str[i] != '\0'; ++i) {
        vga_write_char(str[i]);
    }
}

// Forward declaration of main kernel function
namespace kira::kernel {
    void main() noexcept;
}

// This is called from the custom bootloader (stage2.asm)
extern "C" __attribute__((section(".text._start"))) void _start() {
    // Clear VGA screen and show kernel status
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    
    // Clear screen with dark blue background
    for (int i = 0; i < 2000; i++) {
        vga[i] = 0x1F20; // White on blue background, space character
    }
    
    // Display kernel entry message at top
    const char* msg = "KiraOS Kernel - Entry Point Reached";
    volatile unsigned short* pos = vga + (0 * 80); // Line 0
    for (int i = 0; msg[i] != '\0'; i++) {
        pos[i] = 0x1F00 | msg[i]; // White on blue
    }
    
    // Call the main kernel function
    kira::kernel::main();
    
    // If kernel main returns, halt
    while (true) {
        asm volatile("hlt");
    }
} 