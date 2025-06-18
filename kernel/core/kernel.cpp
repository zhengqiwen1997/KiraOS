#include "system/types.hpp"
#include "system/memory_manager.hpp"
#include "display/vga_display.hpp"
#include "system/idt.hpp"
#include "system/exceptions.hpp"

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
    
    // Initialize IDT (Interrupt Descriptor Table)
    vga.print_string(15, 1, "IDT: Init...", VGA_YELLOW_ON_BLUE);
    IDT::initialize();
    
    // Set up exception handlers
    IDT::set_interrupt_gate(0, (void*)exception_stub_0);   // Division Error
    IDT::set_interrupt_gate(6, (void*)exception_stub_6);   // Invalid Opcode
    IDT::set_interrupt_gate(13, (void*)exception_stub_13); // General Protection Fault
    IDT::set_interrupt_gate(14, (void*)exception_stub_14); // Page Fault
    
    // Load the IDT
    IDT::load();
    vga.print_string(15, 52, "OK", VGA_GREEN_ON_BLUE);
    
    vga.print_string(16, 0, "======================", VGA_WHITE_ON_BLUE);
    vga.print_string(17, 0, "Memory Manager Tests:", VGA_YELLOW_ON_BLUE);
    
    // Test 1: Memory Manager Initialization
    vga.print_string(18, 0, "Test 1: Getting MemoryManager instance...", VGA_CYAN_ON_BLUE);
    auto& memory_manager = MemoryManager::get_instance();
    display_test_result(vga, 19, "MemoryManager Init", true, "Instance obtained");
    
    // Test 2: Basic Allocation
    vga.print_string(20, 0, "Test 2: Basic page allocation...", VGA_CYAN_ON_BLUE);
    void* page1 = memory_manager.allocate_physical_page();
    void* page2 = memory_manager.allocate_physical_page();
    bool basic_alloc_ok = (page1 != nullptr && page2 != nullptr && page1 != page2);
    display_test_result(vga, 21, "Basic Allocation", basic_alloc_ok);
    
    if (basic_alloc_ok) {
        display_memory_info(vga, 22, "  Page1:", page1);
        display_memory_info(vga, 23, "  Page2:", page2);
    }
    
    // Clean up allocated pages and display completion
    if (page1) memory_manager.free_physical_page(page1);
    if (page2) memory_manager.free_physical_page(page2);
    
    // Test 3: Exception Handling - Division by Zero Test
    vga.print_string(23, 0, "Test 3: Testing exception handler...", VGA_CYAN_ON_BLUE);
    vga.print_string(24, 0, "Triggering division by zero...", VGA_YELLOW_ON_BLUE);
    
    // This will trigger our division error exception handler
    volatile int dividend = 42;
    volatile int divisor = 0;
    volatile int result = dividend / divisor;  // Division by zero!
    
    // This line should never be reached due to the exception above
    vga.print_string(24, 40, "This shouldn't appear!", VGA_RED_ON_BLUE);
    
    // Kernel main loop - halt and wait for interrupts
    while (true) {
        asm volatile("hlt");
    }
}

} // namespace kira::kernel 