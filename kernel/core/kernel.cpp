#include "system/types.hpp"
#include "system/memory_manager.hpp"

namespace kira::kernel {

using namespace kira::system;

// Kernel main function
void main(volatile unsigned short* vga) noexcept {
    
    // Display main kernel messages below the memory map analysis
    const char* messages[] = {
        "KiraOS C++ Kernel v1.0",
        "=======================",
        "Memory Manager Test:"
    };
    
    // Display messages starting from line 16 (after memory map analysis)
    for (int msg_idx = 0; msg_idx < 3; msg_idx++) {
        const char* msg = messages[msg_idx];
        volatile unsigned short* pos = vga + ((msg_idx + 16) * 80); // Start from line 16
        
        for (int i = 0; msg[i] != '\0'; i++) {
            pos[i] = 0x1F00 | msg[i]; // White on blue (matching entry point style)
        }
    }
    
    // Debug: Show step-by-step memory manager initialization
    volatile unsigned short* debug_line1 = vga + (19 * 80); // Line 19
    const char* debug_msg1 = "Step 1: Getting MemoryManager...";
    for (int i = 0; debug_msg1[i] != '\0'; i++) {
        debug_line1[i] = 0x1E00 | debug_msg1[i]; // Yellow on blue
    }
    
    // Test the stub MemoryManager (no serial output, just VGA)
    auto& memory_manager = MemoryManager::get_instance();
    
    // Debug: Show that we got the instance
    volatile unsigned short* debug_line2 = vga + (20 * 80); // Line 20
    const char* debug_msg2 = "Step 2: Instance obtained";
    for (int i = 0; debug_msg2[i] != '\0'; i++) {
        debug_line2[i] = 0x1E00 | debug_msg2[i]; // Yellow on blue
    }
    
    // Debug: Show memory map info
    volatile unsigned short* debug_line3 = vga + (21 * 80); // Line 21
    const char* debug_msg3 = "Step 3: Calling allocate...";
    for (int i = 0; debug_msg3[i] != '\0'; i++) {
        debug_line3[i] = 0x1E00 | debug_msg3[i]; // Yellow on blue
    }
    
    // Minimal memory test to stay within code size limits
    void* page1 = memory_manager.allocate_physical_page();
    void* page2 = memory_manager.allocate_physical_page();
    
    // Free page2 and allocate again to test reuse
    memory_manager.free_physical_page(page2);
    void* page3 = memory_manager.allocate_physical_page();
    
    // Simple display: just show if reuse worked
    volatile unsigned short* test_line = vga + (22 * 80);
    if (page3 == page2) {
        // Success - we got the same page back (stack works!)
        const char* msg = "STACK OK";
        for (int i = 0; msg[i] != '\0'; i++) {
            test_line[i] = 0x1A00 | msg[i]; // Green
        }
    } else {
        // Failed - didn't get same page back
        const char* msg = "STACK FAIL";
        for (int i = 0; msg[i] != '\0'; i++) {
            test_line[i] = 0x1C00 | msg[i]; // Red
        }
    }
    
    // Clean up
    memory_manager.free_physical_page(page1);
    memory_manager.free_physical_page(page3);
    
    // Kernel main loop - just halt and wait
    while (true) {
        asm volatile("hlt");
    }
}

} // namespace kira::kernel 