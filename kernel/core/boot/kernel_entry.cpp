#include "core/types.hpp"
#include "display/vga.hpp"

using namespace kira::system;
using namespace kira::display;

// Global variables to store memory map info from bootloader

namespace kira::system {
    extern u32 gMemoryMapAddr;
    extern u32 gMemoryMapCount;
}

// Forward declaration of main kernel function
namespace kira::kernel {
    void main(volatile unsigned short* vgaBuffer) noexcept;
}

// Helper function to display hex number
void display_hex(volatile unsigned short* pos, u32 value, int& offset) {
    for (int i = 7; i >= 0; i--) {
        u32 digit = (value >> (i * 4)) & 0xF;
        char hex_char = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        pos[offset++] = 0x1F00 | hex_char;
    }
}

// Helper function to display decimal number
void display_decimal(volatile unsigned short* pos, u32 value, int& offset) {
    if (value == 0) {
        pos[offset++] = 0x1F00 | '0';
        return;
    }
    
    // Convert to string (simple method for small numbers)
    char buffer[12];
    int i = 0;
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Display in reverse order
    for (int j = i - 1; j >= 0; j--) {
        pos[offset++] = 0x1F00 | buffer[j];
    }
}

// Helper function to set cursor position
void set_cursor_position(u8 row, u8 col) {
    u16 position = row * 80 + col;
    
    // Set cursor position via VGA registers
    asm volatile(
        "mov $0x3D4, %%dx\n"     // VGA index register
        "mov $0x0F, %%al\n"      // Cursor location low byte index
        "out %%al, %%dx\n"       // Write index
        "inc %%dx\n"             // VGA data register (0x3D5)
        "mov %0, %%al\n"         // Low byte of position
        "out %%al, %%dx\n"       // Write low byte
        
        "dec %%dx\n"             // Back to index register
        "mov $0x0E, %%al\n"      // Cursor location high byte index
        "out %%al, %%dx\n"       // Write index
        "inc %%dx\n"             // VGA data register
        "mov %1, %%al\n"         // High byte of position
        "out %%al, %%dx"         // Write high byte
        :
        : "r"((u8)(position & 0xFF)), "r"((u8)((position >> 8) & 0xFF))
        : "eax", "edx"
    );
}

// This is called from the custom bootloader (stage2.asm)
extern "C" __attribute__((section(".text._start"))) void _start() {
    u32 memoryMapAddr = 0;
    u32 memoryMapCount = 0;
    
    asm volatile(
        "mov %%ebx, %0\n"
        "mov %%edi, %1"
        : "=r"(memoryMapAddr), "=r"(memoryMapCount)
        :
        : "ebx", "edi"
    );
    
    // Initialize VGA buffer
    // volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    
    // // Clear screen with dark blue background
    // for (int i = 0; i < 2000; i++) {
    //     vga[i] = 0x1F20; // White on blue background, space character
    // }
    
    // Display kernel entry message at top (Line 0)
    // const char* msg = "!!!KiraOS Kernel - Memory Map Analysis!!!";
    // volatile unsigned short* pos = vga;
    // for (int i = 0; msg[i] != '\0'; i++) {
    //     pos[i] = 0x1F00 | msg[i]; // White on blue
    // }
    
    // // Display memory map summary on line 1
    // const char* summary = "Found ";
    // volatile unsigned short* line1 = vga + 80;
    // int pos_idx = 0;
    
    // for (int i = 0; summary[i] != '\0'; i++) {
    //     line1[pos_idx++] = 0x1F00 | summary[i];
    // }
    
    // display_decimal(line1, memoryMapCount, pos_idx);
    
    // const char* regions_text = " memory regions at 0x";
    // for (int i = 0; regions_text[i] != '\0'; i++) {
    //     line1[pos_idx++] = 0x1F00 | regions_text[i];
    // }
    
    // display_hex(line1, memoryMapAddr, pos_idx);
    
    // Store memory map info in global variables for MemoryManager to access later
    gMemoryMapAddr = memoryMapAddr;
    gMemoryMapCount = memoryMapCount;
    
    // Call the main kernel function with VGA buffer for compatibility
    kira::kernel::main((volatile unsigned short*)VGA_BUFFER);
    
    // Should never reach here
    while (true) {
        asm volatile("hlt");
    }
}

// This is called from the legacy bootloader (stage2.asm) - separate entry point
extern "C" __attribute__((section(".text.legacy_start"))) void legacy_start() {
    // CRITICAL: Read registers from bootloader IMMEDIATELY before any other code!
    u32 memory_map_addr = 0;
    u32 memory_map_count = 0;
    
    asm volatile(
        "mov %%ebx, %0\n"
        "mov %%edi, %1"
        : "=r"(memory_map_addr), "=r"(memory_map_count)
        :
        : "ebx", "edi"
    );
    
    // Initialize VGA buffer
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    
    // Clear screen with dark blue background
    for (int i = 0; i < 2000; i++) {
        vga[i] = 0x1F20; // White on blue background, space character
    }
    
    // Display kernel entry message at top (Line 0)
    const char* msg = "!!!KiraOS Kernel - LEGACY BOOT - Memory Map Analysis!!!";
    volatile unsigned short* pos = vga;
    for (int i = 0; msg[i] != '\0'; i++) {
        pos[i] = 0x1F00 | msg[i]; // White on blue
    }
    
    // Display memory map summary on line 1
    const char* summary = "LEGACY: Found ";
    volatile unsigned short* line1 = vga + 80;
    int pos_idx = 0;
    
    for (int i = 0; summary[i] != '\0'; i++) {
        line1[pos_idx++] = 0x1F00 | summary[i];
    }
    
    display_decimal(line1, memory_map_count, pos_idx);
    
    const char* regions_text = " memory regions at 0x";
    for (int i = 0; regions_text[i] != '\0'; i++) {
        line1[pos_idx++] = 0x1F00 | regions_text[i];
    }
    
    display_hex(line1, memory_map_addr, pos_idx);
    
    // Store memory map info in global variables for MemoryManager to access later
    gMemoryMapAddr = memory_map_addr;
    gMemoryMapCount = memory_map_count;
    
    // Call the main kernel function with VGA buffer for compatibility
    kira::kernel::main((volatile unsigned short*)VGA_BUFFER);
    
    // Should never reach here
    while (true) {
        asm volatile("hlt");
    }
} 