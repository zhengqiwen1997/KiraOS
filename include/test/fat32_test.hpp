#pragma once

#include "fs/fat32.hpp"
#include "fs/vfs.hpp"
#include "fs/block_device.hpp"

namespace kira::test {

class FAT32Test {
public:
    static bool run_tests();

private:
    static bool test_fat32_mount();
    static bool test_fat32_bpb_parsing();
    static bool test_fat32_cluster_operations();
    static bool test_fat32_directory_reading();
    static bool test_fat32_file_reading();
    static bool test_fat32_integration_with_vfs();
    static bool test_fat32_read_write_operations();
    static bool test_fat32_directory_operations();
    
    static void print_test_result(const char* test_name, bool passed);
    static bool create_mock_fat32_data(kira::fs::BlockDevice* device);
};

} // namespace kira::test 