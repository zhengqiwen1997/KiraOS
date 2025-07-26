#pragma once

#include "drivers/ata.hpp"
#include "test/test_base.hpp"

namespace kira::test {

/**
 * @brief ATA Driver Test Suite
 * 
 * Contains comprehensive tests for the ATA/IDE driver functionality.
 * Tests drive detection, sector reading, and integration with memory manager.
 */
class ATADriverTest : public TestBase {
public:
    /**
     * @brief Run comprehensive ATA driver tests
     * @return true if all tests pass, false otherwise
     */
    static bool run_tests();
    
private:
    /**
     * @brief Test basic drive detection
     * @return true if at least one drive detected
     */
    static bool test_drive_detection();
    
    /**
     * @brief Test drive information retrieval
     * @param drive Drive to test
     * @return true if drive info retrieved successfully
     */
    static bool test_drive_info(kira::drivers::ATADriver::DriveType drive);
    
    /**
     * @brief Test sector reading functionality
     * @param drive Drive to test
     * @return true if sector read successful
     */
    static bool test_sector_read(kira::drivers::ATADriver::DriveType drive);
};

} // namespace kira::test 