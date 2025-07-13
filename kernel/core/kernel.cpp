#include "core/types.hpp"
#include "memory/memory_manager.hpp"
#include "memory/virtual_memory.hpp"
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
#include "core/syscalls.hpp"
#include "core/usermode.hpp"
#include "user_programs.hpp"
#include "display/console.hpp"
#include "test/exception_tester.hpp"

namespace kira::kernel {

using namespace kira::display;
using namespace kira::system;
using namespace kira::utils;  // Add utils namespace for String

#define ENABLE_EXCEPTION_TESTING
//#define ENABLE_SINGLE_EXCEPTION_TEST

// Global console instance
ScrollableConsole console;
bool consoleInitialized = false;

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
    initialize_irq();
    
    // Initialize system calls
    initialize_syscalls();
    
    // Initialize console after system is ready
    if (!consoleInitialized) {
        console.initialize();
        consoleInitialized = true;
    }
    
    // Add system initialization messages to console
    console.add_message("KiraOS Kernel Started", kira::display::VGA_GREEN_ON_BLUE);
    console.add_message("Press F1 to enter scroll mode", kira::display::VGA_GREEN_ON_BLUE);
    console.add_message("Arrow Keys = scroll, F1 = exit scroll mode", kira::display::VGA_CYAN_ON_BLUE);
    
    // Debug memory map state
    console.add_message(kira::utils::String("\nMemory Map Address: ") + kira::utils::to_hex_string(gMemoryMapAddr), kira::display::VGA_YELLOW_ON_BLUE);
    console.add_message(kira::utils::String("Memory Map Count: ") + kira::utils::to_hex_string(gMemoryMapCount), kira::display::VGA_YELLOW_ON_BLUE);
    
//     // Calculate total usable RAM
    u32 totalRam = MemoryManager::calculate_total_usable_ram();
    console.add_message(kira::utils::String("Total Usable RAM: ") + kira::utils::to_hex_string(totalRam), kira::display::VGA_YELLOW_ON_BLUE);
    
    auto& virtualMemoryManager = VirtualMemoryManager::get_instance();
    virtualMemoryManager.initialize();
    console.add_message("VirtualMemoryManager initialized successfully", kira::display::VGA_GREEN_ON_BLUE);
    console.add_message("Kernel continuing after paging enabled...", kira::display::VGA_CYAN_ON_BLUE);
    
// #ifdef ENABLE_SINGLE_EXCEPTION_TEST
//     console.add_message("Starting single exception test...\n", kira::display::VGA_CYAN_ON_BLUE);
//     kira::test::ExceptionTester::run_single_test();
// #endif

// #ifdef ENABLE_EXCEPTION_TESTING
//     console.add_message("Starting all exception tests...\n", kira::display::VGA_GREEN_ON_BLUE);
//     kira::test::ExceptionTester::run_all_tests();
// #endif
    
//     // Main kernel loop
//     console.add_message("Entering main loop...\n", kira::display::VGA_YELLOW_ON_BLUE);
    
    // auto& process_manager = ProcessManager::get_instance();
    
    // u32 pid1 = process_manager.create_user_process(kira::usermode::user_test_simple, "TestSysCall", 5);
    // if (pid1) {
    //     console.add_message("User mode process: SUCCESS", kira::display::VGA_GREEN_ON_BLUE);
    // } else {
    //     console.add_message("User mode process: FAILED", kira::display::VGA_RED_ON_BLUE);
    // }
    
    // Test memory manager more thoroughly
    console.add_message("Testing memory management...", kira::display::VGA_YELLOW_ON_BLUE);
    auto& memoryManager = MemoryManager::get_instance();
    // Try to allocate a single page first
    console.add_message("Attempting to allocate a single page...", kira::display::VGA_YELLOW_ON_BLUE);
    void* page1 = memoryManager.allocate_physical_page();
    void* page2 = memoryManager.allocate_physical_page();
    
    if (page1 && page2) {
        console.add_message(kira::utils::String("Page allocated at: ") + kira::utils::to_hex_string((u32)page1), kira::display::VGA_GREEN_ON_BLUE);
        console.add_message(kira::utils::String("And: ") + kira::utils::to_hex_string((u32)page2), kira::display::VGA_GREEN_ON_BLUE);
        
        // Free the page
        console.add_message("Attempting to free the page...", kira::display::VGA_YELLOW_ON_BLUE);
        memoryManager.free_physical_page(page1);
        memoryManager.free_physical_page(page2);
        console.add_message("Memory deallocation: SUCCESS", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        console.add_message("Memory allocation FAILED - checking memory manager state...", kira::display::VGA_RED_ON_BLUE);
        console.add_message(kira::utils::String("Free page count: ") + kira::utils::to_hex_string(memoryManager.get_free_page_count()), kira::display::VGA_YELLOW_ON_BLUE);
        console.add_message(kira::utils::String("Max free pages: ") + kira::utils::to_hex_string(memoryManager.get_max_free_pages()), kira::display::VGA_YELLOW_ON_BLUE);
    }
    
    // Kernel main loop
    u32 counter = 0;
    while (true) {
        counter++;
        
        // Periodic console update (less frequent)
        if ((counter % 1000000) == 0) {
            // Add a heartbeat message occasionally
            console.add_message("System running...", kira::display::VGA_WHITE_ON_BLUE);
        }
        
        // Delay to prevent overwhelming the system
        for (int i = 0; i < 1000; i++) {
            asm volatile("nop");
        }
    }
}

} // namespace kira::kernel