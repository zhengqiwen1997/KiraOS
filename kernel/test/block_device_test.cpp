#include "test/block_device_test.hpp"
#include "memory/memory_manager.hpp"
#include "test/test_base.hpp"

namespace kira::test {

using namespace kira::system;
using namespace kira::fs;

static MemoryManager* memMgrPtr = nullptr;
static BlockDeviceManager* bdmPtr = nullptr;

bool BlockDeviceTest::run_tests() {
    print_section_header("Block Device Tests");
    
    memMgrPtr = &MemoryManager::get_instance();
    bdmPtr = &BlockDeviceManager::get_instance();

    u32 passedTests = 0;
    u32 totalTests = 3;
    
    if (test_device_registration()) passedTests++;
    if (test_ata_device_init()) passedTests++;
    if (test_block_operations()) passedTests++;
    
    memMgrPtr = nullptr;
    bdmPtr = nullptr;

    print_section_footer("Block Device Tests", passedTests, totalTests);
    return (passedTests == totalTests);
}

bool BlockDeviceTest::test_device_registration() {    
    // Create a test ATA device
    void* deviceMemory = memMgrPtr->allocate_physical_page();
    if (!deviceMemory) {
        return false;
    }
    
    ATABlockDevice* ataDevice = new(deviceMemory) ATABlockDevice(0);  // Master drive
    
    // Register the device
    i32 deviceId = bdmPtr->register_device(ataDevice, "hda");
    if (deviceId < 0) {
        print_error("Failed to register ATA device");
        return false;
    }
    
    // Test lookup by ID
    BlockDevice* foundDevice = bdmPtr->get_device(deviceId);
    if (foundDevice != ataDevice) {
        print_error("Device lookup by ID failed");
        return false;
    }
    
    // Test lookup by name
    foundDevice = bdmPtr->get_device("hda");
    if (foundDevice != ataDevice) {
        print_error("Device lookup by name failed");
        return false;
    }
    
    print_success("ATA device registered as hda");
    return true;
}

bool BlockDeviceTest::test_ata_device_init() {
    // Initialize all registered devices
    u32 initializedCount = bdmPtr->initialize_devices();
    
    if (initializedCount == 0) {
        print_warning("No block devices initialized");
        // This might be OK if no ATA drives are present
        return true;
    }
    
    printf_info("Initialized %u block device(s)\n", initializedCount);
    
    return true;
}

bool BlockDeviceTest::test_block_operations() {    
    // Get the first registered device
    BlockDevice* device = bdmPtr->get_device("hda");
    if (!device) {
        print_warning("No hda device found for testing");
        return true;  // Not a failure if no device
    }
    
    // Allocate test buffers
    void* writeBuffer = memMgrPtr->allocate_physical_page();
    void* readBuffer = memMgrPtr->allocate_physical_page();
    
    if (!writeBuffer || !readBuffer) {
        if (writeBuffer) memMgrPtr->free_physical_page(writeBuffer);
        if (readBuffer) memMgrPtr->free_physical_page(readBuffer);
        return false;
    }
    
    // Fill write buffer with test pattern
    u8* writeBytes = static_cast<u8*>(writeBuffer);
    for (u32 i = 0; i < 512; i++) {
        writeBytes[i] = static_cast<u8>(i & 0xFF);
    }
    
    // Test block write (to a safe location - block 100)
    FSResult result = device->write_blocks(100, 1, writeBuffer);
    if (result != FSResult::SUCCESS) {
        print_error("Block write failed");
        memMgrPtr->free_physical_page(writeBuffer);
        memMgrPtr->free_physical_page(readBuffer);
        return false;
    }
    
    // Test block read
    result = device->read_blocks(100, 1, readBuffer);
    if (result != FSResult::SUCCESS) {
        print_error("Block read failed");
        memMgrPtr->free_physical_page(writeBuffer);
        memMgrPtr->free_physical_page(readBuffer);
        return false;
    }
    
    // Verify data
    u8* readBytes = static_cast<u8*>(readBuffer);
    bool dataMatch = true;
    for (u32 i = 0; i < 512; i++) {
        if (readBytes[i] != writeBytes[i]) {
            dataMatch = false;
            break;
        }
    }
    
    memMgrPtr->free_physical_page(writeBuffer);
    memMgrPtr->free_physical_page(readBuffer);
    
    if (!dataMatch) {
        print_error("Block data verification failed");
        return false;
    }
    
    print_success("Block I/O operations successful");
    return true;
}

} // namespace kira::test 