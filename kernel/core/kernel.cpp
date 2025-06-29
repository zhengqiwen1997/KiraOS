#include "core/types.hpp"
#include "memory/memory_manager.hpp"
#include "display/vga.hpp"
#include "arch/x86/gdt.hpp"
#include "arch/x86/idt.hpp"
#include "interrupts/pic.hpp"
#include "interrupts/exceptions.hpp"
#include "core/utils.hpp"
#include "interrupts/irq.hpp"
#include "core/io.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/timer.hpp"
#include "core/process.hpp"
#include "core/test_processes.hpp"
#include "core/syscalls.hpp"
#include "core/usermode.hpp"
#include "user_programs.hpp"
#include "arch/x86/tss.hpp"

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
    
    // Initialize GDT with user mode support
    vga.print_string(3, 0, "GDT: Init user mode...", VGA_YELLOW_ON_BLUE);
    TSSManager::initialize();
    GDTManager::initialize();
    vga.print_string(3, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Initialize hardware interrupts
    vga.print_string(4, 0, "IRQ: Init...", VGA_YELLOW_ON_BLUE);
    irq::initialize();
    vga.print_string(4, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Initialize keyboard system
    vga.print_string(5, 0, "Keyboard: Init...", VGA_YELLOW_ON_BLUE);
    Keyboard::initialize();
    vga.print_string(5, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Initialize process management
    vga.print_string(6, 0, "Process Manager: Init...", VGA_YELLOW_ON_BLUE);
    ProcessManager::initialize();
    vga.print_string(6, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Initialize system calls
    vga.print_string(7, 0, "System Calls: Init...", VGA_YELLOW_ON_BLUE);
    initialize_syscalls();
    vga.print_string(7, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Initialize timer (PIT) to generate timer interrupts
    vga.print_string(8, 0, "Timer: Init (100 Hz)...", VGA_YELLOW_ON_BLUE);
    kira::drivers::Timer::initialize(100); // 100 Hz = 10ms intervals
    vga.print_string(8, 40, "OK", VGA_GREEN_ON_BLUE);
    
    // Enable timer and keyboard interrupts
    irq::enable_irq(PIC::IRQ_TIMER);
    irq::enable_irq(PIC::IRQ_KEYBOARD);
    vga.print_string(9, 0, "IRQ: Timer & Keyboard enabled", VGA_GREEN_ON_BLUE);
    
    // Show interrupt flag status
    u32 eflags;
    asm volatile("pushf; pop %0" : "=r"(eflags));
    bool interrupts_enabled = (eflags & 0x200) != 0;
    vga.print_string(10, 0, "Interrupts: ", VGA_WHITE_ON_BLUE);
    vga.print_string(10, 12, interrupts_enabled ? "ENABLED" : "DISABLED", 
                    interrupts_enabled ? VGA_GREEN_ON_BLUE : VGA_RED_ON_BLUE);
    
    vga.print_string(11, 0, "Entering scheduler loop...", VGA_MAGENTA_ON_BLUE);
    
    // === PROCESS CREATION (Lines 12-15) ===
    vga.print_string(12, 0, "=== USER MODE PROCESS TEST ===", VGA_YELLOW_ON_BLUE);
    
    // For now, create regular user mode processes instead of ELF loading
    // ELF loading will be added after we get the basic system working
    auto& process_manager = ProcessManager::get_instance();
    
    u32 pid1 = process_manager.create_user_process(kira::usermode::user_test_syscall, "TestSysCall", 5);
    
    vga.print_string(13, 0, "Created test user process: ", VGA_CYAN_ON_BLUE);
   vga.print_decimal(13, 27, pid1, VGA_WHITE_ON_BLUE);
    
    if (pid1) {
        vga.print_string(14, 0, "User mode process: SUCCESS", VGA_GREEN_ON_BLUE);
    } else {
        vga.print_string(14, 0, "User mode process: FAILED", VGA_RED_ON_BLUE);
    }
    
    // Test memory manager quickly (silent)
    auto& memory_manager = MemoryManager::get_instance();
    void* page1 = memory_manager.allocate_physical_page();
    void* page2 = memory_manager.allocate_physical_page();
    if (page1) memory_manager.free_physical_page(page1);
    if (page2) memory_manager.free_physical_page(page2);
    
    // === DYNAMIC STATUS (Lines 15-25) ===
    // Line 15: IRQ Activity counters
    // Line 16: Real-time interrupt markers
    // Line 17: User program 1 output (Hello World)
    // Line 18: User program 2 output (Counter)  
    // Line 19: User program 3 output (Animation)
    // Line 20: Keyboard scan codes (hex)
    // Line 21: Keyboard characters (ASCII)
    // Line 22: Keyboard state (key names)
    // Line 23: Current process info
    // Line 24: Process manager statistics
    // Line 25: Available for expansion
    
    // Kernel main loop with process scheduling
    u32 loop_counter = 0;
    while (true) {
        // Update line 24 (scheduler stats) less frequently since line 23 is updated immediately
        if ((loop_counter % 100) == 0) {
            // Only update line 24 (scheduler statistics) - line 23 is updated immediately by scheduler
            auto& pm = ProcessManager::get_instance();
            pm.display_stats();
        }
        
        // Update IRQ counts less frequently 
        if ((loop_counter % 500) == 0) {
            // Show interrupt counts (Line 15)
            u32 timer_count = irq::get_irq_count(0);
            u32 keyboard_count = irq::get_irq_count(1);
            vga.print_string(15, 0, "IRQ Counts - Timer:", VGA_CYAN_ON_BLUE);
            vga.print_decimal(15, 20, timer_count, VGA_WHITE_ON_BLUE);
            vga.print_string(15, 30, "Keyboard:", VGA_CYAN_ON_BLUE);
            vga.print_decimal(15, 40, keyboard_count, VGA_WHITE_ON_BLUE);
        }
        
        // Show heartbeat every 1000 iterations (Line 14)
        if ((loop_counter % 1000) == 0) {
            static u32 heartbeat_pos = 0;
            vga.print_char(14, (heartbeat_pos % 70), '*', VGA_GREEN_ON_BLUE);
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