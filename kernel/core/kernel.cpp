#include "core/types.hpp"
#include "memory/memory_manager.hpp"
#include "arch/x86/gdt.hpp"
#include "arch/x86/idt.hpp"
#include "arch/x86/tss.hpp"
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
#include "display/console.hpp"

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

// Global console instance
ScrollableConsole console;
bool console_initialized = false;

// Kernel main function
void main(volatile unsigned short* vga_buffer) noexcept {
    // Initialize GDT first
    GDTManager::initialize();
    
    // Initialize TSS
    TSSManager::initialize();
    
    // Initialize IDT (required for interrupts)
    IDT::initialize();
    
    // Initialize exception handlers
    Exceptions::initialize();
    
    // Initialize IRQ system
    irq::initialize();
    
    // Initialize system calls
    initialize_syscalls();
    
    // Initialize console after system is ready
    if (!console_initialized) {
        console.initialize();
        console_initialized = true;
    }
    
    // Add system initialization messages to console
    console.add_message("KiraOS Kernel Started", VGA_GREEN_ON_BLUE);
    console.add_message("GDT initialized", VGA_CYAN_ON_BLUE);
    console.add_message("TSS initialized", VGA_CYAN_ON_BLUE);
    console.add_message("IDT initialized", VGA_CYAN_ON_BLUE);
    console.add_message("Exception handlers initialized", VGA_CYAN_ON_BLUE);
    console.add_message("IRQ system initialized", VGA_CYAN_ON_BLUE);
    console.add_message("System calls initialized", VGA_CYAN_ON_BLUE);
    console.add_message("Console initialized", VGA_CYAN_ON_BLUE);
    console.add_message("System ready!", VGA_YELLOW_ON_BLUE);
    console.add_message("Press F1 to enter scroll mode", VGA_GREEN_ON_BLUE);
    console.add_message("Arrow Keys = scroll, F1 = exit scroll mode", VGA_CYAN_ON_BLUE);
    
    // Force display refresh
    console.refresh_display();
    
    // Main kernel loop
    console.add_message("Entering main loop...", VGA_YELLOW_ON_BLUE);
    
    u32 counter = 0;
    while (true) {
        counter++;
        
        // Periodic console update (less frequent)
        if ((counter % 1000000) == 0) {
            // Add a heartbeat message occasionally
            console.add_message("System running...", VGA_WHITE_ON_BLUE);
        }
        
        // Delay to prevent overwhelming the system
        for (int i = 0; i < 1000; i++) {
            asm volatile("nop");
        }
    }
}

} // namespace kira::kernel 