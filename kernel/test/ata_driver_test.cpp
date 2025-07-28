#include "test/ata_driver_test.hpp"
#include "memory/memory_manager.hpp"

namespace kira::test {

using namespace kira::system;
using namespace kira::drivers;

static MemoryManager* memMgrPtr = nullptr;

bool ATADriverTest::run_tests() {
    print_section_header("ATA Driver Tests");
    
    memMgrPtr = &MemoryManager::get_instance();
    u32 passedTests = 0;
    u32 totalTests = 3;
    
    // Test 1: Driver initialization and drive detection
    if (test_drive_detection()) passedTests++;
    else print_error("No ATA drives detected");
    
    // Determine which drive to test
    ATADriver::initialize(); // Re-initialize to check drives
    ATADriver::DriveType testDrive = ATADriver::DriveType::MASTER; // Default to master
    
    // Test 2: Drive information retrieval
    if (test_drive_info(testDrive)) passedTests++;
    else print_error("Could not get drive information");
    
    // Test 3: Sector reading
    if (test_sector_read(testDrive)) passedTests++;
    else print_error("Sector read failed");
    
    memMgrPtr = nullptr;
    print_section_footer("ATA Driver Tests", passedTests, totalTests);
    return (passedTests == totalTests);
}

bool ATADriverTest::test_drive_detection() {
    if (ATADriver::initialize()) {
        print_success("ATA driver initialized");
        return true;
    }
    return false;
}

bool ATADriverTest::test_drive_info(ATADriver::DriveType drive) {
    void* infoBuffer = memMgrPtr->allocate_physical_page();
    if (!infoBuffer) {
        print_error("Failed to allocate buffer for drive info");
        return false;
    }
    bool result = ATADriver::get_drive_info(drive, infoBuffer);
    memMgrPtr->free_physical_page(infoBuffer);
    if (result) print_success("Drive information retrieved");
    return result;
}

bool ATADriverTest::test_sector_read(ATADriver::DriveType drive) {
    void* testBuffer = memMgrPtr->allocate_physical_page();
    if (!testBuffer) {
        print_error("Failed to allocate buffer for sector read");
        return false;
    }
    // Read sector 1 from the available drive
    if (ATADriver::read_sectors(drive, 1, 1, testBuffer)) {
        print_success("Sector read successful");
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
        print_info(msg);
        memMgrPtr->free_physical_page(testBuffer);
        return true;
    } else {
        print_error("Sector read failed");
        memMgrPtr->free_physical_page(testBuffer);
        return false;
    }
}

} // namespace kira::test 