#include "test/ata_driver_test.hpp"
#include "display/console.hpp"
#include "memory/memory_manager.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::test {

using namespace kira::system;
using namespace kira::drivers;

bool ATADriverTest::run_tests() {
    auto& console = kira::kernel::console;
    
    console.add_message("\n=== ATA Driver Tests ===\n", kira::display::VGA_CYAN_ON_BLUE);
    
    // Test 1: Driver initialization and drive detection
    if (!test_drive_detection()) {
        console.add_message("FAIL: No ATA drives detected", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    console.add_message("PASS: ATA driver initialized", kira::display::VGA_GREEN_ON_BLUE);
    
    // Determine which drive to test
    bool masterPresent = ATADriver::initialize(); // Re-initialize to check drives
    ATADriver::DriveType testDrive = ATADriver::DriveType::MASTER; // Default to master
    
    // Test 2: Drive information retrieval
    if (!test_drive_info(testDrive)) {
        console.add_message("FAIL: Could not get drive information", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    console.add_message("PASS: Drive information retrieved", kira::display::VGA_GREEN_ON_BLUE);
    
    // Test 3: Sector reading
    if (!test_sector_read(testDrive)) {
        console.add_message("FAIL: Sector read failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    console.add_message("\n=== All ATA Tests Passed ===\n", kira::display::VGA_GREEN_ON_BLUE);
    return true;
}

bool ATADriverTest::test_drive_detection() {
    return ATADriver::initialize();
}

bool ATADriverTest::test_drive_info(ATADriver::DriveType drive) {
    auto& memMgr = MemoryManager::get_instance();
    void* infoBuffer = memMgr.allocate_physical_page();
    if (!infoBuffer) {
        return false;
    }
    
    bool result = ATADriver::get_drive_info(drive, infoBuffer);
    memMgr.free_physical_page(infoBuffer);
    
    return result;
}

bool ATADriverTest::test_sector_read(ATADriver::DriveType drive) {
    auto& console = kira::kernel::console;
    auto& memMgr = MemoryManager::get_instance();
    
    void* testBuffer = memMgr.allocate_physical_page();
    if (!testBuffer) {
        return false;
    }
    
    // Read sector 1 from the available drive
    if (ATADriver::read_sectors(drive, 1, 1, testBuffer)) {
        console.add_message("PASS: Sector read successful", kira::display::VGA_GREEN_ON_BLUE);
        
        // Display first few bytes (whatever they are)
        u8* byteBuffer = static_cast<u8*>(testBuffer);
        char msg[64];
        kira::utils::strcpy_s(msg, "First bytes: ", sizeof(msg));
        for (int i = 0; i < 4; i++) {
            char hex[16];
            kira::utils::number_to_hex(hex, byteBuffer[i]);
            kira::utils::strcat(msg, hex);
            kira::utils::strcat(msg, " ");
        }
        console.add_message(msg, kira::display::VGA_YELLOW_ON_BLUE);
        
        memMgr.free_physical_page(testBuffer);
        return true;
    } else {
        memMgr.free_physical_page(testBuffer);
        return false;
    }
}

} // namespace kira::test 