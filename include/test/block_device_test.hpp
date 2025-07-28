#pragma once

#include "fs/block_device.hpp"
#include "test/test_base.hpp"

namespace kira::test {

/**
 * @brief Block Device Test Suite
 * 
 * Tests the block device abstraction layer and ATA integration
 */
class BlockDeviceTest : public TestBase {
public:
    /**
     * @brief Run comprehensive block device tests
     * @return true if all tests pass, false otherwise
     */
    static bool run_tests();
    
private:
    /**
     * @brief Test ATA block device initialization
     * @return true if successful
     */
    static bool test_ata_device_init();
    
    /**
     * @brief Test block read/write operations
     * @return true if successful
     */
    static bool test_block_operations();
    
    /**
     * @brief Test device registration and lookup
     * @return true if successful
     */
    static bool test_device_registration();
};

} // namespace kira::test 