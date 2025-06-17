#include "system/types.hpp"
#include "system/memory_manager.hpp"
#include "display/vga_display.hpp"

namespace kira::kernel {

using namespace kira::system;
using namespace kira::display;

// Helper function to display test results with proper formatting
void display_test_result(VGADisplay& vga, u32 line, const char* test_name, bool passed, const char* details = nullptr) {
    // Display test name
    vga.print_string(line, 0, test_name);
    
    // Display result with appropriate color
    if (passed) {
        vga.print_string(line, 25, "[PASS]", VGA_GREEN_ON_BLUE);
    } else {
        vga.print_string(line, 25, "[FAIL]", VGA_RED_ON_BLUE);
    }
    
    // Display additional details if provided
    if (details) {
        vga.print_string(line, 32, details, VGA_CYAN_ON_BLUE);
    }
}

// Helper function to display memory addresses for debugging
void display_memory_info(VGADisplay& vga, u32 line, const char* label, void* addr) {
    vga.print_string(line, 0, label, VGA_YELLOW_ON_BLUE);
    if (addr) {
        vga.print_hex(line, 20, (u32)addr, VGA_WHITE_ON_BLUE);
    } else {
        vga.print_string(line, 20, "NULL", VGA_RED_ON_BLUE);
    }
}

// Kernel main function
void main(volatile unsigned short* vga_buffer) noexcept {
    // Initialize VGA display system
    VGADisplay vga;
    
    // Display kernel header starting from line 14 (after memory map analysis)
    vga.print_string(14, 0, "KiraOS C++ Kernel v1.0", VGA_WHITE_ON_BLUE);
    vga.print_string(15, 0, "======================", VGA_WHITE_ON_BLUE);
    vga.print_string(16, 0, "Memory Manager Tests:", VGA_YELLOW_ON_BLUE);
    
    // Test 1: Memory Manager Initialization
    vga.print_string(17, 0, "Test 1: Getting MemoryManager instance...", VGA_CYAN_ON_BLUE);
    auto& memory_manager = MemoryManager::get_instance();
    display_test_result(vga, 18, "MemoryManager Init", true, "Instance obtained");
    
    // Test 2: Basic Allocation
    vga.print_string(19, 0, "Test 2: Basic page allocation...", VGA_CYAN_ON_BLUE);
    void* page1 = memory_manager.allocate_physical_page();
    void* page2 = memory_manager.allocate_physical_page();
    bool basic_alloc_ok = (page1 != nullptr && page2 != nullptr && page1 != page2);
    display_test_result(vga, 20, "Basic Allocation", basic_alloc_ok);
    
    if (basic_alloc_ok) {
        display_memory_info(vga, 21, "  Page1:", page1);
        display_memory_info(vga, 22, "  Page2:", page2);
    }
    
    // Test 3: Stack Behavior (LIFO)
    vga.print_string(23, 0, "Test 3: Stack behavior (LIFO)...", VGA_CYAN_ON_BLUE);
    memory_manager.free_physical_page(page2);  // Free page2
    void* page3 = memory_manager.allocate_physical_page();  // Should get page2 back
    bool stack_ok = (page3 == page2);
    display_test_result(vga, 24, "Stack Behavior", stack_ok, stack_ok ? "LIFO works" : "LIFO failed");
    
    // Clean up allocated pages
    if (page1) memory_manager.free_physical_page(page1);
    if (page3) memory_manager.free_physical_page(page3);
    
    // Display completion message (stay within 25 lines)
    vga.print_string(23, 40, "Tests completed.", VGA_CYAN_ON_BLUE);
    vga.print_string(24, 0, "Kernel entering idle state...", VGA_LIGHT_GRAY_ON_BLUE);
    
    // Kernel main loop - halt and wait for interrupts
    while (true) {
        asm volatile("hlt");
    }
}

} // namespace kira::kernel 