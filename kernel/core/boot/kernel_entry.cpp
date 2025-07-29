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

// This is called from the custom bootloader (stage2.asm)
extern "C" __attribute__((section(".text._start"))) void _start() {
    u32 memoryMapAddr = 0;
    u32 memoryMapCount = 0;
    u32 isDiskBoot = 0;
    
    asm volatile(
        "mov %%ebx, %0\n"
        "mov %%edi, %1\n"
        "mov %%ecx, %2"
        : "=r"(memoryMapAddr), "=r"(memoryMapCount), "=r"(isDiskBoot)
        :
        : "ebx", "edi", "ecx"
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