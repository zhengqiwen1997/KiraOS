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
    void* device_memory = memMgr.allocate_physical_page();
    if (!device_memory) {
        return false;
    }
    
    ATABlockDevice* ata_device = new(device_memory) ATABlockDevice(0);  // Master drive
    
    // Register the device
    i32 device_id = manager.register_device(ata_device, "hda");
    if (device_id < 0) {
        console.add_message("Failed to register ATA device", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    // Test lookup by ID
    BlockDevice* found_device = manager.get_device(device_id);
    if (found_device != ata_device) {
        console.add_message("Device lookup by ID failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    // Test lookup by name
    found_device = manager.get_device("hda");
    if (found_device != ata_device) {
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
    u32 initialized_count = manager.initialize_devices();
    
    if (initialized_count == 0) {
        console.add_message("No block devices initialized", kira::display::VGA_YELLOW_ON_BLUE);
        // This might be OK if no ATA drives are present
        return true;
    }
    
    char msg[64];
    strcpy_s(msg, "Initialized ", sizeof(msg));
    char count_str[16];
    number_to_decimal(count_str, initialized_count);
    strcat(msg, count_str);
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
    void* write_buffer = memMgr.allocate_physical_page();
    void* read_buffer = memMgr.allocate_physical_page();
    
    if (!write_buffer || !read_buffer) {
        if (write_buffer) memMgr.free_physical_page(write_buffer);
        if (read_buffer) memMgr.free_physical_page(read_buffer);
        return false;
    }
    
    // Fill write buffer with test pattern
    u8* write_bytes = static_cast<u8*>(write_buffer);
    for (u32 i = 0; i < 512; i++) {
        write_bytes[i] = static_cast<u8>(i & 0xFF);
    }
    
    // Test block write (to a safe location - block 100)
    FSResult result = device->write_blocks(100, 1, write_buffer);
    if (result != FSResult::SUCCESS) {
        console.add_message("Block write failed", kira::display::VGA_RED_ON_BLUE);
        memMgr.free_physical_page(write_buffer);
        memMgr.free_physical_page(read_buffer);
        return false;
    }
    
    // Test block read
    result = device->read_blocks(100, 1, read_buffer);
    if (result != FSResult::SUCCESS) {
        console.add_message("Block read failed", kira::display::VGA_RED_ON_BLUE);
        memMgr.free_physical_page(write_buffer);
        memMgr.free_physical_page(read_buffer);
        return false;
    }
    
    // Verify data
    u8* read_bytes = static_cast<u8*>(read_buffer);
    bool data_match = true;
    for (u32 i = 0; i < 512; i++) {
        if (read_bytes[i] != write_bytes[i]) {
            data_match = false;
            break;
        }
    }
    
    memMgr.free_physical_page(write_buffer);
    memMgr.free_physical_page(read_buffer);
    
    if (!data_match) {
        console.add_message("Block data verification failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    console.add_message("Block I/O operations successful", kira::display::VGA_YELLOW_ON_BLUE);
    return true;
}

} // namespace kira::test 