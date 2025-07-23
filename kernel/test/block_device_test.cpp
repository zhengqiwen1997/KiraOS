#include "test/block_device_test.hpp"
#include "display/console.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::test {

using namespace kira::system;
using namespace kira::fs;
using namespace kira::utils;

bool BlockDeviceTest::run_tests() {
    auto& console = kira::kernel::console;
    
    console.add_message("\n=== Block Device Tests ===\n", kira::display::VGA_CYAN_ON_BLUE);
    
    // Test 1: Device manager
    if (!test_device_manager()) {
        console.add_message("FAIL: Device manager test failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: Device manager", kira::display::VGA_GREEN_ON_BLUE);
    
    // Test 2: Device registration
    if (!test_device_registration()) {
        console.add_message("FAIL: Device registration failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: Device registration", kira::display::VGA_GREEN_ON_BLUE);
    
    // Test 3: ATA device initialization
    if (!test_ata_device_init()) {
        console.add_message("FAIL: ATA device initialization failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: ATA device initialization", kira::display::VGA_GREEN_ON_BLUE);
    
    // Test 4: Block operations
    if (!test_block_operations()) {
        console.add_message("FAIL: Block operations failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: Block operations", kira::display::VGA_GREEN_ON_BLUE);
    
    console.add_message("\n=== All Block Device Tests Passed ===\n", kira::display::VGA_GREEN_ON_BLUE);
    return true;
}

bool BlockDeviceTest::test_device_manager() {
    BlockDeviceManager& manager = BlockDeviceManager::get_instance();
    return true;  // If we got here, singleton creation worked
}

bool BlockDeviceTest::test_device_registration() {
    auto& console = kira::kernel::console;
    auto& memMgr = MemoryManager::get_instance();
    BlockDeviceManager& manager = BlockDeviceManager::get_instance();
    
    // Create a test ATA device
    void* deviceMemory = memMgr.allocate_physical_page();
    if (!deviceMemory) {
        return false;
    }
    
    ATABlockDevice* ataDevice = new(deviceMemory) ATABlockDevice(0);  // Master drive
    
    // Register the device
    i32 deviceId = manager.register_device(ataDevice, "hda");
    if (deviceId < 0) {
        console.add_message("Failed to register ATA device", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    // Test lookup by ID
    BlockDevice* foundDevice = manager.get_device(deviceId);
    if (foundDevice != ataDevice) {
        console.add_message("Device lookup by ID failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    // Test lookup by name
    foundDevice = manager.get_device("hda");
    if (foundDevice != ataDevice) {
        console.add_message("Device lookup by name failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    console.add_message("ATA device registered as hda", kira::display::VGA_YELLOW_ON_BLUE);
    return true;
}

bool BlockDeviceTest::test_ata_device_init() {
    auto& console = kira::kernel::console;
    BlockDeviceManager& manager = BlockDeviceManager::get_instance();
    
    // Initialize all registered devices
    u32 initializedCount = manager.initialize_devices();
    
    if (initializedCount == 0) {
        console.add_message("No block devices initialized", kira::display::VGA_YELLOW_ON_BLUE);
        // This might be OK if no ATA drives are present
        return true;
    }
    
    char msg[64];
    strcpy_s(msg, "Initialized ", sizeof(msg));
    char countStr[16];
    number_to_decimal(countStr, initializedCount);
    strcat(msg, countStr);
    strcat(msg, " block device(s)");
    console.add_message(msg, kira::display::VGA_YELLOW_ON_BLUE);
    
    return true;
}

bool BlockDeviceTest::test_block_operations() {
    auto& console = kira::kernel::console;
    auto& memMgr = MemoryManager::get_instance();
    BlockDeviceManager& manager = BlockDeviceManager::get_instance();
    
    // Get the first registered device
    BlockDevice* device = manager.get_device("hda");
    if (!device) {
        console.add_message("No hda device found for testing", kira::display::VGA_YELLOW_ON_BLUE);
        return true;  // Not a failure if no device
    }
    
    // Allocate test buffers
    void* writeBuffer = memMgr.allocate_physical_page();
    void* readBuffer = memMgr.allocate_physical_page();
    
    if (!writeBuffer || !readBuffer) {
        if (writeBuffer) memMgr.free_physical_page(writeBuffer);
        if (readBuffer) memMgr.free_physical_page(readBuffer);
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
        console.add_message("Block write failed", kira::display::VGA_RED_ON_BLUE);
        memMgr.free_physical_page(writeBuffer);
        memMgr.free_physical_page(readBuffer);
        return false;
    }
    
    // Test block read
    result = device->read_blocks(100, 1, readBuffer);
    if (result != FSResult::SUCCESS) {
        console.add_message("Block read failed", kira::display::VGA_RED_ON_BLUE);
        memMgr.free_physical_page(writeBuffer);
        memMgr.free_physical_page(readBuffer);
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
    
    memMgr.free_physical_page(writeBuffer);
    memMgr.free_physical_page(readBuffer);
    
    if (!dataMatch) {
        console.add_message("Block data verification failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    console.add_message("Block I/O operations successful", kira::display::VGA_YELLOW_ON_BLUE);
    return true;
}

} // namespace kira::test 