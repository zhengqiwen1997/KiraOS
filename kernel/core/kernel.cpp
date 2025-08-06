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
#include "test/process_test.hpp"
#include "test/exception_tester.hpp"
#include "drivers/ata.hpp"
#include "test/ata_driver_test.hpp"
#include "test/vfs_test.hpp"
#include "test/block_device_test.hpp"
#include "test/fat32_test.hpp"
#include "test/sync_test.hpp"
#include "test/k_printf_test.hpp"
#include "fs/vfs.hpp"
#include "fs/fat32.hpp"
#include "fs/block_device.hpp"
#include "drivers/ata.hpp"

namespace kira::kernel {

using namespace kira::display;
using namespace kira::system;
using namespace kira::fs;
using namespace kira::utils;
using namespace kira::drivers;

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

    // DEBUG: Add message after VirtualMemoryManager
    console.add_message("VirtualMemoryManager completed, getting MemoryManager...", kira::display::VGA_CYAN_ON_BLUE);

    // Initialize memory manager
    auto& memoryManager = MemoryManager::get_instance();
    
    // DEBUG: Add message after MemoryManager
    console.add_message("MemoryManager singleton obtained successfully", kira::display::VGA_GREEN_ON_BLUE);

    // Initialize process management
    console.add_message("\nInitializing process management...", kira::display::VGA_YELLOW_ON_BLUE);
    auto& process_manager = ProcessManager::get_instance();
    
    // Initialize the ATA driver, VFS and Block Devices
    console.add_message("Initializing ATA driver...", kira::display::VGA_YELLOW_ON_BLUE);
    bool ataInitialized = ATADriver::initialize();
    if (ataInitialized) {
        console.add_message("ATA driver initialized successfully", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        console.add_message("ATA driver initialization failed", kira::display::VGA_RED_ON_BLUE);
    }
    
    VFS::get_instance().initialize();
    
    // Create and register ATA block devices for detected drives
    console.add_message("Registering ATA block devices...", kira::display::VGA_YELLOW_ON_BLUE);
    auto& blockMgr = BlockDeviceManager::get_instance();
    u32 registeredDevices = 0;
    
    // Check if master drive is present and register it
    if (ATADriver::is_master_present()) {
        console.add_message("Creating ATA master device...", kira::display::VGA_CYAN_ON_BLUE);
        void* deviceMemory = memoryManager.allocate_physical_page();
        if (deviceMemory) {
            ATABlockDevice* masterDevice = new(deviceMemory) ATABlockDevice(0); // Master = 0
            i32 deviceId = blockMgr.register_device(masterDevice, "hda");
            if (deviceId >= 0) {
                console.add_message("Registered ATA master as hda", kira::display::VGA_GREEN_ON_BLUE);
                registeredDevices++;
            } else {
                console.add_message("Failed to register ATA master", kira::display::VGA_RED_ON_BLUE);
            }
        }
    }
    
    // Check if slave drive is present and register it
    if (ATADriver::is_slave_present()) {
        console.add_message("Creating ATA slave device...", kira::display::VGA_CYAN_ON_BLUE);
        void* deviceMemory2 = memoryManager.allocate_physical_page();
        if (deviceMemory2) {
            ATABlockDevice* slaveDevice = new(deviceMemory2) ATABlockDevice(1); // Slave = 1
            i32 deviceId = blockMgr.register_device(slaveDevice, "hdb");
            if (deviceId >= 0) {
                console.add_message("Registered ATA slave as hdb", kira::display::VGA_GREEN_ON_BLUE);
                registeredDevices++;
            } else {
                console.add_message("Failed to register ATA slave", kira::display::VGA_RED_ON_BLUE);
            }
        }
    }
    
    char deviceCountStr[32];
    kira::utils::number_to_decimal(deviceCountStr, registeredDevices);
    console.add_message("Total ATA devices registered:", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message(deviceCountStr, kira::display::VGA_YELLOW_ON_BLUE);
    
    // Now initialize the registered devices
    u32 initializedCount = blockMgr.initialize_devices();
    char initCountStr[32];
    kira::utils::number_to_decimal(initCountStr, initializedCount);
    console.add_message("ATA devices initialized:", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message(initCountStr, kira::display::VGA_YELLOW_ON_BLUE);


    // We put many tests here because we don't want to run them on disk boot
    #ifndef DISK_BOOT_ONLY
        // Test k_printf functionality
        // console.add_message("\nTesting k_printf functionality...", kira::display::VGA_YELLOW_ON_BLUE);
        // if (kira::test::KPrintfTest::run_tests()) {
        //     console.add_message("k_printf tests passed", kira::display::VGA_GREEN_ON_BLUE);
        // } else {
        //     console.add_message("k_printf tests failed", kira::display::VGA_RED_ON_BLUE);
        // }

        // Test memory manager before running user processes
        console.add_message("Testing memory management...", kira::display::VGA_YELLOW_ON_BLUE);
    
        // void* testPage = memoryManager.allocate_physical_page();
        // if (testPage) {
        //     console.add_message("Memory manager allocation: SUCCESS", kira::display::VGA_GREEN_ON_BLUE);
        //     memoryManager.free_physical_page(testPage);
        // } else {
        //     console.add_message("Memory manager allocation: FAILED", kira::display::VGA_RED_ON_BLUE);
        // }

        // Initialize and test Synchronization Primitives
        // console.add_message("\nTesting Synchronization Primitives...", kira::display::VGA_YELLOW_ON_BLUE);
        // if (kira::test::SyncTest::run_tests()) {
        //     console.add_message("Synchronization primitives ready", kira::display::VGA_GREEN_ON_BLUE);
        // } else {
        //     console.add_message("Synchronization tests failed", kira::display::VGA_RED_ON_BLUE);
        // }

        // Test ATA driver
        // console.add_message("\nInitializing ATA/IDE driver...", kira::display::VGA_YELLOW_ON_BLUE);
        // if (kira::test::ATADriverTest::run_tests()) {
        //     console.add_message("ATA driver ready for file system", kira::display::VGA_GREEN_ON_BLUE);
        // } else {
        //     console.add_message("ATA driver tests failed", kira::display::VGA_RED_ON_BLUE);
        // }

        // Test VFS - DISABLED: conflicts with shell RamFS mounting
        console.add_message("\nVFS Test skipped (conflicts with shell filesystem)", kira::display::VGA_YELLOW_ON_BLUE);
        // if (kira::test::VFSTest::run_tests()) {
        //     console.add_message("VFS ready for applications", kira::display::VGA_GREEN_ON_BLUE);
        // } else {
        //     console.add_message("VFS tests failed", kira::display::VGA_RED_ON_BLUE);
        // }
            
        // // Test Block Devices
        // console.add_message("\nInitializing Block Device Layer...", kira::display::VGA_YELLOW_ON_BLUE);
        // if (kira::test::BlockDeviceTest::run_tests()) {
        //     console.add_message("Block devices ready for file systems", kira::display::VGA_GREEN_ON_BLUE);
        // } else {
        //     console.add_message("Block device tests failed", kira::display::VGA_RED_ON_BLUE);
        // }
        
        // // Test FAT32 File System
        // console.add_message("\nTesting FAT32 File System...", kira::display::VGA_YELLOW_ON_BLUE);
        // // kira::test::FAT32Test fat32Test;
        // if (kira::test::FAT32Test::run_tests()) {
        //     console.add_message("FAT32 file system ready", kira::display::VGA_GREEN_ON_BLUE);
        // } else {
        //     console.add_message("FAT32 tests failed", kira::display::VGA_RED_ON_BLUE);
        // }

        // Initialize and test Process Management and Scheduler
        // console.add_message("\nTesting Process Management and Scheduler...", kira::display::VGA_YELLOW_ON_BLUE);
        // if (kira::test::ProcessTest::run_tests()) {
        //     console.add_message("Process management and scheduler ready", kira::display::VGA_GREEN_ON_BLUE);
        // } else {
        //     console.add_message("Process management tests failed", kira::display::VGA_RED_ON_BLUE);
        // }
    #else
        console.add_message("Many tests disabled for disk boot", kira::display::VGA_CYAN_ON_BLUE);
    #endif
    
        
    // Mount FAT32 as root filesystem for shell functionality
    console.add_message("Mounting FAT32 as root filesystem...", kira::display::VGA_YELLOW_ON_BLUE);
    auto& vfs = VFS::get_instance();
    auto& memMgr = MemoryManager::get_instance();
    
    // Get first available block device for FAT32
    BlockDevice* blockDevice = BlockDeviceManager::get_instance().get_device(0); // Primary master ATA device
    
    if (!blockDevice) {
        console.add_message("ERROR: No block device available for FAT32", kira::display::VGA_RED_ON_BLUE);
        console.add_message("FAT32 requires ATA/IDE drive to be detected", kira::display::VGA_YELLOW_ON_BLUE);
        return;
    }
    
    // Create and register FAT32 using SAFE static allocation (minimal pool size)
    console.add_message("Creating FAT32 filesystem...", kira::display::VGA_YELLOW_ON_BLUE);
    FAT32* fat32 = FAT32::create_static_instance(blockDevice);
    if (!fat32) {
        console.add_message("ERROR: Failed to create FAT32 filesystem", kira::display::VGA_RED_ON_BLUE);
        return;
    }

    u32 mfatCacheSector = fat32->get_m_fatCacheSector();
    k_printf("[FAT32] get_m_fatCacheSector: %d\n", mfatCacheSector);
    
    console.add_message("DEBUG: FAT32 instance created with static allocation", kira::display::VGA_CYAN_ON_BLUE);
    vfs.register_filesystem(fat32);
    console.add_message("DEBUG: FAT32 registered with VFS", kira::display::VGA_CYAN_ON_BLUE);
    
    // Mount FAT32 at root
    FSResult mountResult = vfs.mount("", "/", "fat32");
    char mountResultStr[32];
    kira::utils::number_to_decimal(mountResultStr, static_cast<u32>(mountResult));
    console.add_message("FAT32 mount result:", kira::display::VGA_CYAN_ON_BLUE);
    console.add_message(mountResultStr, kira::display::VGA_CYAN_ON_BLUE);
    
    if (mountResult == FSResult::SUCCESS) {
        console.add_message("FAT32 mounted successfully at /", kira::display::VGA_GREEN_ON_BLUE);
        
        // Verify we can access the root directory
        VNode* rootVNode = nullptr;
        FSResult rootResult = vfs.resolve_path("/", rootVNode);
        if (rootResult == FSResult::SUCCESS && rootVNode) {
            console.add_message("Going to Create boot directory", kira::display::VGA_GREEN_ON_BLUE);

            FSResult fileResult = rootVNode->create_file("LOG", FileType::REGULAR);
            if (fileResult == FSResult::SUCCESS) {
                console.add_message("Created LOG file", kira::display::VGA_GREEN_ON_BLUE);
            } else {
                console.add_message("[FATAL] Failed to create LOG file", kira::display::VGA_RED_ON_BLUE);
            }
            console.add_message("FAT32 root directory accessible", kira::display::VGA_CYAN_ON_BLUE);
        } else {
            console.add_message("FAT32 root directory not accessible", kira::display::VGA_RED_ON_BLUE);
        }
        
        console.add_message("FAT32 filesystem ready for shell", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        console.add_message("ERROR: FAT32 mount failed", kira::display::VGA_RED_ON_BLUE);
        return;
    }
    console.add_message("About to create user process...", kira::display::VGA_YELLOW_ON_BLUE);
    k_printf("[FAT32] get_m_fatCacheSector 2: %d\n", mfatCacheSector);

    u32 pid1 = process_manager.create_user_process(kira::usermode::user_shell, "KiraShell", 5);
    if (pid1) {
        char pidMsg[64];
        strcpy_s(pidMsg, "Interactive shell started with PID: ", sizeof(pidMsg));
        char pidStr[16];
        number_to_decimal(pidStr, pid1);
        strcat(pidMsg, pidStr);
        console.add_message(pidMsg, kira::display::VGA_GREEN_ON_BLUE);
        console.add_message("Starting process scheduler...", kira::display::VGA_YELLOW_ON_BLUE);
        
        // Debug: Check if ready queue still has processes before scheduling
        console.add_message("DEBUG: About to call schedule() - checking ready queue...", kira::display::VGA_MAGENTA_ON_BLUE);
        
        // Enable timer-driven scheduling before starting the scheduler
        ProcessManager::enable_timer_scheduling();
        k_printf("[FAT32] get_m_fatCacheSector 3: %d\n", mfatCacheSector);

        // Start the process scheduler - this will run the user process and enter idle loop
        process_manager.schedule();
        k_printf("[FAT32] get_m_fatCacheSector 4: %d\n", mfatCacheSector);

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