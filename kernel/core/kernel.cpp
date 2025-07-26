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
#include "drivers/ata.hpp"
#include "test/ata_driver_test.hpp"
#include "test/vfs_test.hpp"
#include "test/block_device_test.hpp"
#include "test/fat32_test.hpp"
#include "test/sync_test.hpp"

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
    
    // Calculate total usable RAM
    u32 totalRam = MemoryManager::calculate_total_usable_ram();
    console.add_message("Total Usable RAM calculated", kira::display::VGA_YELLOW_ON_BLUE);
    
    auto& virtualMemoryManager = VirtualMemoryManager::get_instance();
    virtualMemoryManager.initialize();

    // Test memory manager before running user processes
    console.add_message("Testing memory management...", kira::display::VGA_YELLOW_ON_BLUE);
    
    auto& memoryManager = MemoryManager::get_instance();
    
    // Test basic memory allocation
    void* testPage = memoryManager.allocate_physical_page();
    if (testPage) {
        console.add_message("Memory manager allocation: SUCCESS", kira::display::VGA_GREEN_ON_BLUE);
        memoryManager.free_physical_page(testPage);
    } else {
        console.add_message("Memory manager allocation: FAILED", kira::display::VGA_RED_ON_BLUE);
    }
    
    // Initialize and test ATA driver
    console.add_message("Initializing ATA/IDE driver...", kira::display::VGA_YELLOW_ON_BLUE);
    if (kira::test::ATADriverTest::run_tests()) {
        console.add_message("ATA driver ready for file system", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        console.add_message("ATA driver tests failed", kira::display::VGA_RED_ON_BLUE);
    }
    
    // Initialize and test VFS
    console.add_message("Initializing Virtual File System...", kira::display::VGA_YELLOW_ON_BLUE);
    if (kira::test::VFSTest::run_tests()) {
        console.add_message("VFS ready for applications", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        console.add_message("VFS tests failed", kira::display::VGA_RED_ON_BLUE);
    }
    
    // Initialize and test Block Devices
    console.add_message("Initializing Block Device Layer...", kira::display::VGA_YELLOW_ON_BLUE);
    if (kira::test::BlockDeviceTest::run_tests()) {
        console.add_message("Block devices ready for file systems", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        console.add_message("Block device tests failed", kira::display::VGA_RED_ON_BLUE);
    }
    
    // Initialize and test Synchronization Primitives
    console.add_message("Testing Synchronization Primitives...", kira::display::VGA_YELLOW_ON_BLUE);
    if (kira::test::SyncTest::run_tests()) {
        console.add_message("Synchronization primitives ready", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        console.add_message("Synchronization tests failed", kira::display::VGA_RED_ON_BLUE);
    }
    
    // Initialize and test FAT32 File System
    console.add_message("Testing FAT32 File System...", kira::display::VGA_YELLOW_ON_BLUE);
    if (kira::test::FAT32Test::run_tests()) {
        console.add_message("FAT32 file system ready", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        console.add_message("FAT32 tests failed", kira::display::VGA_RED_ON_BLUE);
    }
    
    // Initialize process management
    console.add_message("Initializing process management...", kira::display::VGA_YELLOW_ON_BLUE);
    auto& process_manager = ProcessManager::get_instance();
    
    console.add_message("About to create user process...", kira::display::VGA_YELLOW_ON_BLUE);
    u32 pid1 = process_manager.create_user_process(kira::usermode::user_test_syscall, "TestSysCall", 5);
    if (pid1) {
        console.add_message("User mode process: SUCCESS", kira::display::VGA_GREEN_ON_BLUE);
        console.add_message("Starting process scheduler...", kira::display::VGA_YELLOW_ON_BLUE);
        
        // Debug: Check if ready queue still has processes before scheduling
        console.add_message("DEBUG: About to call schedule() - checking ready queue...", kira::display::VGA_MAGENTA_ON_BLUE);
        
        // Enable timer-driven scheduling before starting the scheduler
        ProcessManager::enable_timer_scheduling();
        
        // Start the process scheduler - this will run the user process and enter idle loop
        process_manager.schedule();
        
        // The schedule() call above should not return in normal operation
        // If we get here, something went wrong
        console.add_message("ERROR: Scheduler returned unexpectedly", kira::display::VGA_RED_ON_BLUE);
    } else {
        console.add_message("User mode process: FAILED", kira::display::VGA_RED_ON_BLUE);
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