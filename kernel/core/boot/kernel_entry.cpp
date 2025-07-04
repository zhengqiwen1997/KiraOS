#include "core/types.hpp"
#include "memory/memory.hpp"
#include "memory/memory_manager.hpp"
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
    // Get memory map from bootloader (but don't display the table)
    u32 memoryMapAddr = 0;
    u32 memoryMapCount = 0;
    
    asm volatile(
        "mov %%ebx, %0\n"
        "mov %%edi, %1"
        : "=r"(memoryMapAddr), "=r"(memoryMapCount)
        :
        : "ebx", "edi"
    );
    
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