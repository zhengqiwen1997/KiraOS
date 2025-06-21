#include "core/types.hpp"
#include "memory/memory_manager.hpp"
#include "display/vga.hpp"
#include "arch/x86/idt.hpp"
#include "interrupts/exceptions.hpp"
#include "core/utils.hpp"
#include "interrupts/pic.hpp"
#include "interrupts/irq.hpp"
#include "core/io.hpp"
#include "drivers/keyboard.hpp"

namespace kira::kernel {

using namespace kira::system;
using namespace kira::display;
using namespace kira::system::utils;

// Test configuration - uncomment to enable interrupt testing
// #define ENABLE_INTERRUPT_TESTING

#ifdef ENABLE_INTERRUPT_TESTING
// Current test configuration (change these to test different interrupts)
#define TEST_INTERRUPT_NUM 14
#define TEST_INTERRUPT_NAME "Page Fault"

// Available interrupts to test:
// 0="Division Error", 1="Debug", 2="NMI", 3="Breakpoint", 4="Overflow"
// 5="Bound Range", 6="Invalid Opcode", 7="Device Not Available", 8="Double Fault"
// 10="Invalid TSS", 11="Segment Not Present", 12="Stack Fault", 13="General Protection Fault", 14="Page Fault"
#endif

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
    
    // === SYSTEM INITIALIZATION (Lines 1-5) ===
    vga.print_string(1, 0, "KiraOS C++ Kernel v1.0", VGA_WHITE_ON_BLUE);
    
    // Initialize IDT (Interrupt Descriptor Table)
    vga.print_string(2, 0, "IDT: Init...", VGA_YELLOW_ON_BLUE);
    IDT::initialize();
    
    // Set up exception handlers for all common CPU exceptions
    IDT::set_interrupt_gate(0, (void*)exception_stub_0);   // Division Error
    IDT::set_interrupt_gate(1, (void*)exception_stub_1);   // Debug
    IDT::set_interrupt_gate(2, (void*)exception_stub_2);   // NMI
    IDT::set_interrupt_gate(3, (void*)exception_stub_3);   // Breakpoint
    IDT::set_interrupt_gate(4, (void*)exception_stub_4);   // Overflow
    IDT::set_interrupt_gate(5, (void*)exception_stub_5);   // Bound Range
    IDT::set_interrupt_gate(6, (void*)exception_stub_6);   // Invalid Opcode
    IDT::set_interrupt_gate(7, (void*)exception_stub_7);   // Device Not Available
    IDT::set_interrupt_gate(8, (void*)exception_stub_8);   // Double Fault
    IDT::set_interrupt_gate(10, (void*)exception_stub_10); // Invalid TSS
    IDT::set_interrupt_gate(11, (void*)exception_stub_11); // Segment Not Present
    IDT::set_interrupt_gate(12, (void*)exception_stub_12); // Stack Fault
    IDT::set_interrupt_gate(13, (void*)exception_stub_13); // General Protection Fault
    IDT::set_interrupt_gate(14, (void*)exception_stub_14); // Page Fault
    
    // Load the IDT
    IDT::load();
    vga.print_string(2, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Initialize hardware interrupts
    vga.print_string(3, 0, "IRQ: Init...", VGA_YELLOW_ON_BLUE);
    irq::initialize();
    vga.print_string(3, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Initialize keyboard system
    vga.print_string(4, 0, "Keyboard: Init...", VGA_YELLOW_ON_BLUE);
    Keyboard::initialize();
    vga.print_string(4, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Debug: Show what addresses are actually installed in IDT
    vga.print_string(6, 0, "Debug: IRQ stub addresses", VGA_CYAN_ON_BLUE);
    vga.print_string(6, 30, "IRQ0:", VGA_CYAN_ON_BLUE);
    vga.print_hex(6, 35, (u32)irq_stub_0, VGA_WHITE_ON_BLUE);
    vga.print_string(6, 50, "IRQ1:", VGA_CYAN_ON_BLUE);
    vga.print_hex(6, 55, (u32)irq_stub_1, VGA_WHITE_ON_BLUE);
    
    // Enable timer and keyboard interrupts
    irq::enable_irq(PIC::IRQ_TIMER);
    irq::enable_irq(PIC::IRQ_KEYBOARD);
    vga.print_string(5, 0, "IRQ: Timer & Keyboard enabled", VGA_GREEN_ON_BLUE);
    
    // Debug: Show PIC mask
    u16 pic_mask = PIC::get_irq_mask();
    vga.print_string(7, 0, "PIC Mask: 0x", VGA_CYAN_ON_BLUE);
    vga.print_hex(7, 13, pic_mask, VGA_WHITE_ON_BLUE);
    
    // === INTERRUPT TESTING (Lines 8-12) ===
    vga.print_string(8, 0, "=== INTERRUPT TESTING ===", VGA_YELLOW_ON_BLUE);
    
    // Test memory manager quickly (silent)
    auto& memory_manager = MemoryManager::get_instance();
    void* page1 = memory_manager.allocate_physical_page();
    void* page2 = memory_manager.allocate_physical_page();
    if (page1) memory_manager.free_physical_page(page1);
    if (page2) memory_manager.free_physical_page(page2);
    
    // First test: CPU exception (should work if IDT is working)
    vga.print_string(9, 0, "Test 1: Exception (INT3)...", VGA_CYAN_ON_BLUE);
    
    // Use inline assembly with explicit NOP to ensure proper continuation
    asm volatile(
        "int $1\n\t"
        "nop\n\t"        // Ensure there's a safe instruction after the interrupt
        "nop"            // Additional safety
    );
    
    vga.print_string(9, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Second test: Hardware interrupt via software
    vga.print_string(10, 0, "Test 2: Software IRQ 0...", VGA_CYAN_ON_BLUE);
    asm volatile("int $32");  // Manually trigger timer interrupt (IRQ 0)
    vga.print_string(10, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Show interrupt flag status
    u32 eflags;
    asm volatile("pushf; pop %0" : "=r"(eflags));
    bool interrupts_enabled = (eflags & 0x200) != 0;
    vga.print_string(11, 0, "Interrupts: ", VGA_WHITE_ON_BLUE);
    vga.print_string(11, 12, interrupts_enabled ? "ENABLED" : "DISABLED", 
                    interrupts_enabled ? VGA_GREEN_ON_BLUE : VGA_RED_ON_BLUE);
    
    vga.print_string(12, 0, "Entering main loop...", VGA_MAGENTA_ON_BLUE);
    
    // === DYNAMIC STATUS (Lines 13-24) ===
    // Line 13: IRQ Activity counters
    // Line 14: Real-time interrupt markers
    // Line 15: Timer handler activity
    // Line 16: Keyboard handler activity  
    // Line 17: Timer dots display
    // Line 18: Keyboard scan codes (hex)
    // Line 19: Keyboard characters (ASCII)
    // Line 20: Keyboard state (Shift, Caps Lock)
    // Lines 21-24: Available for expansion
    
    // Kernel main loop - active loop with periodic halts
    u32 loop_counter = 0;
    while (true) {
        // Update dynamic status every 1000 iterations
        if ((loop_counter % 1000) == 0) {
            // Show interrupt counts (Line 13)
            u32 timer_count = irq::get_irq_count(0);
            u32 keyboard_count = irq::get_irq_count(1);
            vga.print_string(13, 0, "IRQ Counts - Timer:", VGA_CYAN_ON_BLUE);
            vga.print_decimal(13, 20, timer_count, VGA_WHITE_ON_BLUE);
            vga.print_string(13, 30, "Keyboard:", VGA_CYAN_ON_BLUE);
            vga.print_decimal(13, 40, keyboard_count, VGA_WHITE_ON_BLUE);
        }
        
        // Show heartbeat every 5000 iterations (Line 24)
        if ((loop_counter % 5000) == 0) {
            static u32 heartbeat_pos = 0;
            vga.print_char(24, (heartbeat_pos % 70), '*', VGA_GREEN_ON_BLUE);
            heartbeat_pos++;
        }
        
        loop_counter++;
        
        // Small delay to prevent overwhelming the system
        for (int i = 0; i < 1000; i++) {
            asm volatile("nop");
        }
        
        // Halt to allow interrupts
        asm volatile("hlt");
    }
}

} // namespace kira::kernel 