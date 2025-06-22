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
#include "core/process.hpp"
#include "core/test_processes.hpp"

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



// Kernel main function
void main(volatile unsigned short* vga_buffer) noexcept {
    // Initialize VGA display system
    VGADisplay vga;
    
    // === SYSTEM INITIALIZATION (Lines 1-6) ===
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
    
    // Initialize process management
    vga.print_string(5, 0, "Process Manager: Init...", VGA_YELLOW_ON_BLUE);
    ProcessManager::initialize();
    vga.print_string(5, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Enable timer and keyboard interrupts
    irq::enable_irq(PIC::IRQ_TIMER);
    irq::enable_irq(PIC::IRQ_KEYBOARD);
    vga.print_string(6, 0, "IRQ: Timer & Keyboard enabled", VGA_GREEN_ON_BLUE);
    
    // Show interrupt flag status
    u32 eflags;
    asm volatile("pushf; pop %0" : "=r"(eflags));
    bool interrupts_enabled = (eflags & 0x200) != 0;
    vga.print_string(7, 0, "Interrupts: ", VGA_WHITE_ON_BLUE);
    vga.print_string(7, 12, interrupts_enabled ? "ENABLED" : "DISABLED", 
                    interrupts_enabled ? VGA_GREEN_ON_BLUE : VGA_RED_ON_BLUE);
    
    vga.print_string(8, 0, "Entering scheduler loop...", VGA_MAGENTA_ON_BLUE);
    
    // === PROCESS CREATION (Lines 9-12) ===
    vga.print_string(9, 0, "=== PROCESS MANAGEMENT TEST ===", VGA_YELLOW_ON_BLUE);
    
    // Create test processes
    auto& pm = ProcessManager::get_instance();
    
    u32 pid1 = pm.create_process(process_counter, "Counter", 5);
    u32 pid2 = pm.create_process(process_spinner, "Spinner", 5);
    u32 pid3 = pm.create_process(process_dots, "Dots", 5);
    
    vga.print_string(10, 0, "Created processes: ", VGA_CYAN_ON_BLUE);
    vga.print_decimal(10, 19, pid1, VGA_WHITE_ON_BLUE);
    vga.print_string(10, 21, ", ", VGA_CYAN_ON_BLUE);
    vga.print_decimal(10, 23, pid2, VGA_WHITE_ON_BLUE);
    vga.print_string(10, 25, ", ", VGA_CYAN_ON_BLUE);
    vga.print_decimal(10, 27, pid3, VGA_WHITE_ON_BLUE);
    
    if (pid1 && pid2 && pid3) {
        vga.print_string(11, 0, "Process creation: SUCCESS", VGA_GREEN_ON_BLUE);
    } else {
        vga.print_string(11, 0, "Process creation: FAILED", VGA_RED_ON_BLUE);
    }
    
    // Test memory manager quickly (silent)
    auto& memory_manager = MemoryManager::get_instance();
    void* page1 = memory_manager.allocate_physical_page();
    void* page2 = memory_manager.allocate_physical_page();
    if (page1) memory_manager.free_physical_page(page1);
    if (page2) memory_manager.free_physical_page(page2);
    
    // === DYNAMIC STATUS (Lines 13-24) ===
    // Line 13: IRQ Activity counters
    // Line 14: Real-time interrupt markers
    // Line 15: Process 1 output (Counter)
    // Line 16: Process 2 output (Spinner)  
    // Line 17: Process 3 output (Dots)
    // Line 18: Keyboard scan codes (hex)
    // Line 19: Keyboard characters (ASCII)
    // Line 20: Keyboard state (key names)
    // Line 21: Current process info
    // Line 22: Process manager statistics
    // Lines 23-24: Available for expansion
    
    // Kernel main loop with process scheduling
    u32 loop_counter = 0;
    while (true) {
        // Update line 22 (scheduler stats) less frequently since line 21 is updated immediately
        if ((loop_counter % 100) == 0) {
            // Only update line 22 (scheduler statistics) - line 21 is updated immediately by scheduler
            auto& pm = ProcessManager::get_instance();
            pm.display_stats();
        }
        
        // Update IRQ counts less frequently 
        if ((loop_counter % 500) == 0) {
            // Show interrupt counts (Line 13)
            u32 timer_count = irq::get_irq_count(0);
            u32 keyboard_count = irq::get_irq_count(1);
            vga.print_string(13, 0, "IRQ Counts - Timer:", VGA_CYAN_ON_BLUE);
            vga.print_decimal(13, 20, timer_count, VGA_WHITE_ON_BLUE);
            vga.print_string(13, 30, "Keyboard:", VGA_CYAN_ON_BLUE);
            vga.print_decimal(13, 40, keyboard_count, VGA_WHITE_ON_BLUE);
        }
        
        // Show heartbeat every 1000 iterations (Line 12)
        if ((loop_counter % 1000) == 0) {
            static u32 heartbeat_pos = 0;
            vga.print_char(12, (heartbeat_pos % 70), '*', VGA_GREEN_ON_BLUE);
            heartbeat_pos++;
        }
        
        loop_counter++;
        
        // Small delay to prevent overwhelming the system
        for (int i = 0; i < 100; i++) {
            asm volatile("nop");
        }
        
        // Halt to allow interrupts (and process scheduling)
        asm volatile("hlt");
    }
}

} // namespace kira::kernel 