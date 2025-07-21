#pragma once

#include "fs/vfs.hpp"
#include "fs/ramfs.hpp"

namespace kira::test {

/**
 * @brief VFS Test Suite
 * 
 * Tests the Virtual File System and RamFS implementation
 */
class VFSTest {
public:
    /**
     * @brief Run comprehensive VFS tests
     * @return true if all tests pass, false otherwise
     */
    static bool run_tests();
    
private:
    /**
     * @brief Test VFS initialization
     * @return true if successful
     */
    static bool test_vfs_initialization();
    
    /**
     * @brief Test file system mounting
     * @return true if successful
     */
    static bool test_filesystem_mounting();
    
    /**
     * @brief Test basic file operations
     * @return true if successful
     */
    static bool test_file_operations();
    
    /**
     * @brief Test directory operations
     * @return true if successful
     */
    static bool test_directory_operations();
    
    /**
     * @brief Test path resolution
     * @return true if successful
     */
    static bool test_path_resolution();
};

} // namespace kira::test 