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
#include "core/syscalls.hpp"
#include "core/usermode.hpp"
#include "user_programs.hpp"
#include "display/console.hpp"
#include "test/exception_tester.hpp"

namespace kira::kernel {

using namespace kira::display;
using namespace kira::system;

#define ENABLE_EXCEPTION_TESTING
//#define ENABLE_SINGLE_EXCEPTION_TEST

// Global console instance
kira::display::ScrollableConsole console;
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
    console.add_message("GDT initialized", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message("TSS initialized", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message("IDT initialized", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message("Exception handlers initialized", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message("IRQ system initialized", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message("System calls initialized", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message("Console initialized", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message("System ready!", kira::display::VGA_YELLOW_ON_BLUE);
    console.add_message("Press F1 to enter scroll mode", kira::display::VGA_GREEN_ON_BLUE);
    console.add_message("Arrow Keys = scroll, F1 = exit scroll mode", kira::display::VGA_CYAN_ON_BLUE);
    
    // Force display refresh
    console.refresh_display();
    
#ifdef ENABLE_SINGLE_EXCEPTION_TEST
    console.add_message("Starting single exception test...", kira::display::VGA_CYAN_ON_BLUE);
    kira::test::ExceptionTester::run_single_test();
#endif

#ifdef ENABLE_EXCEPTION_TESTING
    console.add_message("Starting all exception tests...", kira::display::VGA_GREEN_ON_BLUE);
    kira::test::ExceptionTester::run_all_tests();
#endif
    
    // Main kernel loop
    console.add_message("Entering main loop...", kira::display::VGA_YELLOW_ON_BLUE);
    
    // auto& process_manager = ProcessManager::get_instance();
    
    // u32 pid1 = process_manager.create_user_process(kira::usermode::user_test_syscall, "TestSysCall", 5);
    // if (pid1) {
    //     console.add_message("User mode process: SUCCESS", kira::display::VGA_GREEN_ON_BLUE);
    // } else {
    //     console.add_message("User mode process: FAILED", kira::display::VGA_RED_ON_BLUE);
    // }
    
    // Test memory manager quickly (silent)
    auto& memoryManager = MemoryManager::get_instance();
    void* page1 = memoryManager.allocate_physical_page();
    void* page2 = memoryManager.allocate_physical_page();
    if (page1) memoryManager.free_physical_page(page1);
    if (page2) memoryManager.free_physical_page(page2);
    
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