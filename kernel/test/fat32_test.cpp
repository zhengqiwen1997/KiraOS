#include "test/fat32_test.hpp"
#include "memory/memory_manager.hpp"

namespace kira::test {

using namespace kira::fs;

static MemoryManager* memMgrPtr = nullptr;
static BlockDeviceManager* bdmPtr = nullptr;

bool FAT32Test::run_tests() {
    print_section_header("FAT32 File System Tests");
    
    memMgrPtr = &MemoryManager::get_instance();
    bdmPtr = &BlockDeviceManager::get_instance();
    
    u32 passedTests = 0;
    u32 totalTests = 3;
    
    if (test_fat32_mount()) passedTests++;
    if (test_fat32_integration_with_vfs()) passedTests++;
    if (test_fat32_directory_operations()) passedTests++;
    
    memMgrPtr = nullptr;
    bdmPtr = nullptr;
    
    print_section_footer("FAT32 File System Tests", passedTests, totalTests);
    return (passedTests == totalTests);
}

bool FAT32Test::test_fat32_mount() {
    bool testPassed = true;
    
    // Get a block device for testing
    // BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdmPtr->get_device(0);
    
    if (!device) {
        print_warning("No block device available for FAT32 testing");
        print_test_result("FAT32 Mount (skipped - no device)", true);
        return true;
    }
    
    // Create FAT32 instance
    void* fat32Memory = memMgrPtr->allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Clear the disk first to ensure clean state for the first mount test
        u8 emptySector[512];
        memset(emptySector, 0, 512);
        if (device->write_blocks(0, 1, emptySector) != FSResult::SUCCESS) {
            print_error("Failed to clear boot sector for first mount test");
            testPassed = false;
        } else {
            // Test mount without mock data (should fail gracefully)
            FSResult mountResult = fat32->mount("test");
            if (mountResult != FSResult::SUCCESS) {
                print_success("FAT32 mount failed as expected (no filesystem)");
            } else {
                print_warning("FAT32 mount unexpectedly succeeded");
            }
        }
        
        // Test with mock data
        if (create_mock_fat32_data(device)) {
            FSResult mockMountResult = fat32->mount("test");
            if (mockMountResult == FSResult::SUCCESS) {
                print_success("FAT32 mount with mock data successful");
            } else {
                print_error("FAT32 mount with mock data failed");
                testPassed = false;
            }
        } else {
            print_error("Failed to create mock FAT32 data");
            testPassed = false;
        }
        
        fat32->~FAT32();
        memMgrPtr->free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Mount", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_integration_with_vfs() {
    bool testPassed = true;
    
    print_info("Testing FAT32 integration with VFS...");
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0);
    
    if (!device) {
        print_warning("No block device for VFS integration test");
        print_test_result("FAT32 VFS Integration (skipped)", true);
        return true;
    }
    
    // Create some mock FAT32 data on the device
    if (!create_mock_fat32_data(device)) {
        print_error("Failed to create mock FAT32 data for VFS test");
        print_test_result("FAT32 VFS Integration (setup failed)", false);
        return false;
    }
    
    // Create FAT32 instance
    void* fat32Memory = memMgrPtr->allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Mount the FAT32 filesystem
        FSResult mountResult = fat32->mount("test");
        if (mountResult != FSResult::SUCCESS) {
            print_error("FAT32 mount failed for VFS integration test");
            testPassed = false;
        } else {
            // Test getting root directory through VFS
            VNode* rootNode = nullptr;
            FSResult rootResult = fat32->get_root(rootNode);
            if (rootResult == FSResult::SUCCESS && rootNode) {
                print_success("VFS root directory access successful");
                
                // Test directory listing
                DirectoryEntry dirEntry;
                FSResult listResult = rootNode->read_dir(0, dirEntry);
                if (listResult == FSResult::NOT_FOUND) {
                    print_success("VFS directory listing works (empty directory)");
                } else {
                    print_error("VFS directory listing failed");
                    testPassed = false;
                }
                
                print_debug("About to clean up root node...");
                
                // Clean up root node safely using cleanup method
                if (rootNode) {
                    print_debug("Cleaning up root node resources...");
                    rootNode->~VNode();
                    print_debug("Freeing root node memory...");
                    memMgrPtr->free_physical_page(rootNode);
                    rootNode = nullptr;
                    print_debug("Root node cleanup complete");
                }
            } else {
                print_error("VFS root directory access failed");
                testPassed = false;
            }
        }
        
        print_debug("About to clean up FAT32...");
        
        // Clean up FAT32 resources safely using cleanup method
        print_debug("Cleaning up FAT32 resources...");
        // No explicit cleanup needed - destructor will handle it
        
        print_debug("About to free FAT32 memory...");
        print_debug("FAT32 memory pointer check...");
        if (fat32Memory) {
            print_debug("FAT32 memory pointer is valid, freeing...");
            memMgrPtr->free_physical_page(fat32Memory);
            print_debug("FAT32 memory freed successfully");
        } else {
            print_error("FAT32 memory pointer is NULL!");
        }
        print_debug("FAT32 cleanup complete");
    }
    
    print_test_result("FAT32 VFS Integration", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_directory_operations() {
    bool testPassed = true;
    
    // Get a block device for testing
    BlockDevice* device = bdmPtr->get_device(0);
    
    if (!device) {
        print_warning("No block device for directory operations test");
        print_test_result("FAT32 Directory Operations (skipped)", true);
        return true;
    }
    
    // Create some mock FAT32 data on the device
    if (!create_mock_fat32_data(device)) {
        print_error("Failed to create mock FAT32 data for directory test");
        print_test_result("FAT32 Directory Operations (setup failed)", false);
        return false;
    }
    
    // Create FAT32 instance
    void* fat32Memory = memMgrPtr->allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Mount the FAT32 filesystem
        FSResult mountResult = fat32->mount("test");
        if (mountResult != FSResult::SUCCESS) {
            print_warning("FAT32 mount failed for directory operations test");
            fat32->~FAT32();
            memMgrPtr->free_physical_page(fat32Memory);
            print_test_result("FAT32 Directory Operations (mount failed)", false);
            return false;
        }
        
        // Get root directory
        VNode* rootNode = nullptr;
        FSResult rootResult = fat32->get_root(rootNode);
        if (rootResult != FSResult::SUCCESS || !rootNode) {
            print_error("Failed to get root directory");
            testPassed = false;
        } else {
            // Test file operations
            print_info("Testing file create/lookup/delete operations...");
            
            // Test 1: Create and lookup first file
            FSResult createResult1 = rootNode->create_file("test.txt", FileType::REGULAR);
            if (createResult1 == FSResult::SUCCESS) {
                VNode* foundNode = nullptr;
                FSResult lookupResult = rootNode->lookup("test.txt", foundNode);
                if (lookupResult == FSResult::SUCCESS && foundNode) {
                    print_success("File create/lookup: SUCCESS");
                    memMgrPtr->free_physical_page(foundNode);
                    
                    // Test 2: Delete first file
                    FSResult deleteResult = rootNode->delete_file("test.txt");
                    if (deleteResult == FSResult::SUCCESS) {
                        print_success("File deletion: SUCCESS");
                        
                        // Test 3: Create and delete second file
                        FSResult createResult2 = rootNode->create_file("test2.txt", FileType::REGULAR);
                        if (createResult2 == FSResult::SUCCESS) {
                            FSResult deleteResult2 = rootNode->delete_file("test2.txt");
                            if (deleteResult2 == FSResult::SUCCESS) {
                                print_success("Second file create/delete: SUCCESS");
                            } else {
                                print_error("Second file deletion failed");
                                testPassed = false;
                            }
                        } else {
                            print_error("Second file creation failed");
                            testPassed = false;
                        }
                    } else {
                        print_error("First file deletion failed");
                        testPassed = false;
                    }
                } else {
                    print_error("File lookup failed");
                    testPassed = false;
                }
            } else {
                print_error("First file creation failed");
                testPassed = false;
            }
        }

        fat32->~FAT32();
        memMgrPtr->free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Directory Operations", testPassed);
    return testPassed;
}

bool FAT32Test::create_mock_fat32_data(BlockDevice* device) {
    if (!device) return false;
        
    // FAT32 layout constants - using safe region to avoid bootloader/kernel
    static constexpr u32 BOOT_SECTOR = 1000;  // Safe region, far from bootloader
    static constexpr u32 RESERVED_SECTORS = 32;
    static constexpr u32 FAT_SIZE_SECTORS = 1024;
    static constexpr u32 NUM_FATS = 2;
    static constexpr u32 ROOT_CLUSTER = 2;
    static constexpr u32 SECTORS_PER_CLUSTER = 8;
    static constexpr u32 BYTES_PER_SECTOR = 512;
    
    // Calculate FAT32 layout
    u32 fatStartSector = RESERVED_SECTORS;
    u32 fatEndSector = fatStartSector + (NUM_FATS * FAT_SIZE_SECTORS);
    u32 dataStartSector = fatEndSector;
    u32 rootClusterSector = dataStartSector + ((ROOT_CLUSTER - 2) * SECTORS_PER_CLUSTER);
    u32 rootClusterEndSector = rootClusterSector + SECTORS_PER_CLUSTER;
    
    // Clear only the specific sectors that FAT32 will use
    u8 emptySector[BYTES_PER_SECTOR];
    memset(emptySector, 0, BYTES_PER_SECTOR);
    
    // Clear boot sector
    if (device->write_blocks(BOOT_SECTOR, 1, emptySector) != FSResult::SUCCESS) {
        return false;
    }
    
    // Clear FAT tables
    for (u32 sector = fatStartSector; sector < fatEndSector; sector++) {
        if (device->write_blocks(sector, 1, emptySector) != FSResult::SUCCESS) {
            return false;
        }
    }
    
    // Clear root directory cluster
    for (u32 sector = rootClusterSector; sector < rootClusterEndSector; sector++) {
        if (device->write_blocks(sector, 1, emptySector) != FSResult::SUCCESS) {
            return false;
        }
    }
    
    // Create a simple FAT32 BPB (BIOS Parameter Block)
    Fat32Bpb bpb;
    memset(&bpb, 0, sizeof(bpb));
    
    // Jump instruction (standard boot jump)
    bpb.jump_boot[0] = 0xEB;
    bpb.jump_boot[1] = 0x58;
    bpb.jump_boot[2] = 0x90;
    
    // OEM name
    memcpy(bpb.oem_name, "KIRAOS  ", 8);
    
    // Basic FAT32 parameters using constants
    bpb.bytes_per_sector = BYTES_PER_SECTOR;
    bpb.sectors_per_cluster = SECTORS_PER_CLUSTER;
    bpb.reserved_sector_count = RESERVED_SECTORS;
    bpb.num_fats = NUM_FATS;
    bpb.root_entry_count = 0;  // FAT32 uses cluster chain for root
    bpb.total_sectors_16 = 0;  // Use 32-bit field
    bpb.media = 0xF8;  // Hard disk
    bpb.fat_size_16 = 0;  // Use 32-bit field
    bpb.sectors_per_track = 63;
    bpb.num_heads = 255;
    bpb.hidden_sectors = BOOT_SECTOR;  // Offset from start of disk
    bpb.total_sectors_32 = 65536;  // 32MB filesystem
    bpb.fat_size_32 = FAT_SIZE_SECTORS;
    bpb.ext_flags = 0;
    bpb.fs_version = 0;
    bpb.root_cluster = ROOT_CLUSTER;
    bpb.fs_info = 1;
    bpb.backup_boot_sector = 6;
    bpb.drive_number = 0x80;
    bpb.boot_signature = 0x29;
    bpb.volume_id = 0x12345678;
    memcpy(bpb.volume_label, "KIRAOS     ", 11);
    memcpy(bpb.fs_type, "FAT32   ", 8);
    
    // Write BPB to sector 0
    u8 sector0[512];
    memset(sector0, 0, 512);
    memcpy(sector0, &bpb, sizeof(bpb));
    sector0[510] = 0x55;  // Boot signature
    sector0[511] = 0xAA;
    
    FSResult writeResult = device->write_blocks(0, 1, sector0);
    if (writeResult != FSResult::SUCCESS) {
        return false;
    }
    
    // Initialize FAT tables
    u8 fatSector[BYTES_PER_SECTOR];
    memset(fatSector, 0, BYTES_PER_SECTOR);
    
    // First FAT sector has special entries
    u32* fatEntries = reinterpret_cast<u32*>(fatSector);
    fatEntries[0] = 0x0FFFFFF8;  // Media descriptor + end-of-chain
    fatEntries[1] = 0x0FFFFFFF;  // End-of-chain
    fatEntries[2] = 0x0FFFFFFF;  // Root directory (single cluster)
    
    // Write first FAT sector to both FAT copies
    u32 firstFatSector = fatStartSector;
    u32 secondFatSector = fatStartSector + FAT_SIZE_SECTORS;
    if (device->write_blocks(firstFatSector, 1, fatSector) != FSResult::SUCCESS) return false;
    if (device->write_blocks(secondFatSector, 1, fatSector) != FSResult::SUCCESS) return false;
    
    // Initialize remaining FAT sectors (all zeros = free clusters)
    memset(fatSector, 0, BYTES_PER_SECTOR);
    for (u32 sector = firstFatSector + 1; sector < firstFatSector + FAT_SIZE_SECTORS; sector++) {
        if (device->write_blocks(sector, 1, fatSector) != FSResult::SUCCESS) return false;
    }
    for (u32 sector = secondFatSector + 1; sector < secondFatSector + FAT_SIZE_SECTORS; sector++) {
        if (device->write_blocks(sector, 1, fatSector) != FSResult::SUCCESS) return false;
    }
    
    // Initialize root directory cluster
    u32 rootClusterSize = SECTORS_PER_CLUSTER * BYTES_PER_SECTOR;
    u8 rootCluster[rootClusterSize];
    memset(rootCluster, 0, rootClusterSize);  // Empty directory
    
    FSResult rootWriteResult = device->write_blocks(rootClusterSector, SECTORS_PER_CLUSTER, rootCluster);
    if (rootWriteResult != FSResult::SUCCESS) {
        return false;
    }
    
    return true;
}

} // namespace kira::test 