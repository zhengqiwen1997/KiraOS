#include "test/fat32_test.hpp"
#include "display/console.hpp"
#include "core/utils.hpp"
#include "memory/memory_manager.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::test {

using namespace kira::display;
using namespace kira::fs;
using namespace kira::utils;
using kira::kernel::console;

bool FAT32Test::run_tests() {
    console.add_message("\n=== FAT32 File System Tests ===\n", VGA_CYAN_ON_BLUE);
    
    bool allPassed = true;
    allPassed &= test_fat32_mount();
    allPassed &= test_fat32_integration_with_vfs();
    allPassed &= test_fat32_directory_operations();
    
    if (allPassed) {
        console.add_message("\n=== All FAT32 Tests Passed ===\n", VGA_GREEN_ON_BLUE);
    }
    return allPassed;
}

bool FAT32Test::test_fat32_mount() {
    bool testPassed = true;
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0);
    
    if (!device) {
        console.add_message("No block device available for FAT32 testing", VGA_YELLOW_ON_BLUE);
        print_test_result("FAT32 Mount (skipped - no device)", true);
        return true;
    }
    
    // Create FAT32 instance
    auto& memMgr = MemoryManager::get_instance();
    void* fat32Memory = memMgr.allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Test mount without mock data (should fail gracefully)
        FSResult mountResult = fat32->mount("test");
        if (mountResult != FSResult::SUCCESS) {
            console.add_message("FAT32 mount failed as expected (no filesystem)", VGA_GREEN_ON_BLUE);
        } else {
            console.add_message("FAT32 mount unexpectedly succeeded", VGA_YELLOW_ON_BLUE);
        }
        
        // Test with mock data
        if (create_mock_fat32_data(device)) {
            FSResult mockMountResult = fat32->mount("test");
            if (mockMountResult == FSResult::SUCCESS) {
                console.add_message("FAT32 mount with mock data successful", VGA_GREEN_ON_BLUE);
            } else {
                console.add_message("FAT32 mount with mock data failed", VGA_RED_ON_BLUE);
                testPassed = false;
            }
        } else {
            console.add_message("Failed to create mock FAT32 data", VGA_RED_ON_BLUE);
            testPassed = false;
        }
        
        fat32->~FAT32();
        memMgr.free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Mount", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_integration_with_vfs() {
    bool testPassed = true;
    
    console.add_message("Testing FAT32 integration with VFS...", VGA_CYAN_ON_BLUE);
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0);
    
    if (!device) {
        console.add_message("No block device for VFS integration test", VGA_YELLOW_ON_BLUE);
        print_test_result("FAT32 VFS Integration (skipped)", true);
        return true;
    }
    
    // Create some mock FAT32 data on the device
    if (!create_mock_fat32_data(device)) {
        console.add_message("Failed to create mock FAT32 data for VFS test", VGA_RED_ON_BLUE);
        print_test_result("FAT32 VFS Integration (setup failed)", false);
        return false;
    }
    
    // Create FAT32 instance
    auto& memMgr = MemoryManager::get_instance();
    void* fat32Memory = memMgr.allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Mount the FAT32 filesystem
        FSResult mountResult = fat32->mount("test");
        if (mountResult != FSResult::SUCCESS) {
            console.add_message("FAT32 mount failed for VFS integration test", VGA_RED_ON_BLUE);
            testPassed = false;
        } else {
            // Test getting root directory through VFS
            VNode* rootNode = nullptr;
            FSResult rootResult = fat32->get_root(rootNode);
            if (rootResult == FSResult::SUCCESS && rootNode) {
                console.add_message("VFS root directory access successful", VGA_GREEN_ON_BLUE);
                
                // Test directory listing
                DirectoryEntry dirEntry;
                FSResult listResult = rootNode->read_dir(0, dirEntry);
                if (listResult == FSResult::NOT_FOUND) {
                    console.add_message("VFS directory listing works (empty directory)", VGA_GREEN_ON_BLUE);
                } else {
                    console.add_message("VFS directory listing failed", VGA_RED_ON_BLUE);
                    testPassed = false;
                }
                
                console.add_message("About to clean up root node...", VGA_CYAN_ON_BLUE);
                
                // Clean up root node safely using cleanup method
                if (rootNode) {
                    console.add_message("Cleaning up root node resources...", VGA_CYAN_ON_BLUE);
                    rootNode->~VNode();
                    console.add_message("Freeing root node memory...", VGA_CYAN_ON_BLUE);
                    memMgr.free_physical_page(rootNode);
                    rootNode = nullptr;
                    console.add_message("Root node cleanup complete", VGA_CYAN_ON_BLUE);
                }
            } else {
                console.add_message("VFS root directory access failed", VGA_RED_ON_BLUE);
                testPassed = false;
            }
        }
        
        console.add_message("About to clean up FAT32...", VGA_CYAN_ON_BLUE);
        
        // Clean up FAT32 resources safely using cleanup method
        console.add_message("Cleaning up FAT32 resources...", VGA_CYAN_ON_BLUE);
        // No explicit cleanup needed - destructor will handle it
        
        console.add_message("About to free FAT32 memory...", VGA_CYAN_ON_BLUE);
        console.add_message("FAT32 memory pointer check...", VGA_CYAN_ON_BLUE);
        if (fat32Memory) {
            console.add_message("FAT32 memory pointer is valid, freeing...", VGA_CYAN_ON_BLUE);
            memMgr.free_physical_page(fat32Memory);
            console.add_message("FAT32 memory freed successfully", VGA_CYAN_ON_BLUE);
        } else {
            console.add_message("FAT32 memory pointer is NULL!", VGA_RED_ON_BLUE);
        }
        console.add_message("FAT32 cleanup complete", VGA_CYAN_ON_BLUE);
    }
    
    print_test_result("FAT32 VFS Integration", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_directory_operations() {
    bool testPassed = true;
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0);
    
    if (!device) {
        console.add_message("No block device for directory operations test", VGA_YELLOW_ON_BLUE);
        print_test_result("FAT32 Directory Operations (skipped)", true);
        return true;
    }
    
    // Create some mock FAT32 data on the device
    if (!create_mock_fat32_data(device)) {
        console.add_message("Failed to create mock FAT32 data for directory test", VGA_RED_ON_BLUE);
        print_test_result("FAT32 Directory Operations (setup failed)", false);
        return false;
    }
    
    // Create FAT32 instance
    auto& memMgr = MemoryManager::get_instance();
    void* fat32Memory = memMgr.allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Mount the FAT32 filesystem
        FSResult mountResult = fat32->mount("test");
        if (mountResult != FSResult::SUCCESS) {
            console.add_message("FAT32 mount failed for directory operations test", VGA_YELLOW_ON_BLUE);
            fat32->~FAT32();
            memMgr.free_physical_page(fat32Memory);
            print_test_result("FAT32 Directory Operations (mount failed)", false);
            return false;
        }
        
        // Get root directory
        VNode* rootNode = nullptr;
        FSResult rootResult = fat32->get_root(rootNode);
        if (rootResult != FSResult::SUCCESS || !rootNode) {
            console.add_message("Failed to get root directory", VGA_RED_ON_BLUE);
            testPassed = false;
        } else {
            // Test file operations
            console.add_message("Testing file create/lookup/delete operations...", VGA_CYAN_ON_BLUE);
            
            // Test 1: Create and lookup first file
            FSResult createResult1 = rootNode->create_file("test.txt", FileType::REGULAR);
            if (createResult1 == FSResult::SUCCESS) {
                VNode* foundNode = nullptr;
                FSResult lookupResult = rootNode->lookup("test.txt", foundNode);
                if (lookupResult == FSResult::SUCCESS && foundNode) {
                    console.add_message("File create/lookup: SUCCESS", VGA_GREEN_ON_BLUE);
                    memMgr.free_physical_page(foundNode);
                    
                    // Test 2: Delete first file
                    FSResult deleteResult = rootNode->delete_file("test.txt");
                    if (deleteResult == FSResult::SUCCESS) {
                        console.add_message("File deletion: SUCCESS", VGA_GREEN_ON_BLUE);
                        
                        // Test 3: Create and delete second file
                        FSResult createResult2 = rootNode->create_file("test2.txt", FileType::REGULAR);
                        if (createResult2 == FSResult::SUCCESS) {
                            FSResult deleteResult2 = rootNode->delete_file("test2.txt");
                            if (deleteResult2 == FSResult::SUCCESS) {
                                console.add_message("Second file create/delete: SUCCESS", VGA_GREEN_ON_BLUE);
                            } else {
                                console.add_message("Second file deletion failed", VGA_RED_ON_BLUE);
                                testPassed = false;
                            }
                        } else {
                            console.add_message("Second file creation failed", VGA_RED_ON_BLUE);
                            testPassed = false;
                        }
                    } else {
                        console.add_message("First file deletion failed", VGA_RED_ON_BLUE);
                        testPassed = false;
                    }
                } else {
                    console.add_message("File lookup failed", VGA_RED_ON_BLUE);
                    testPassed = false;
                }
            } else {
                console.add_message("First file creation failed", VGA_RED_ON_BLUE);
                testPassed = false;
            }
        }

        fat32->~FAT32();
        memMgr.free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Directory Operations", testPassed);
    return testPassed;
}

bool FAT32Test::create_mock_fat32_data(BlockDevice* device) {
    if (!device) return false;
    
    auto& memMgr = MemoryManager::get_instance();
    
    // Create a simple FAT32 BPB (BIOS Parameter Block)
    Fat32Bpb bpb;
    memset(&bpb, 0, sizeof(bpb));
    
    // Jump instruction
    bpb.jump_boot[0] = 0xEB;
    bpb.jump_boot[1] = 0x58;
    bpb.jump_boot[2] = 0x90;
    
    // OEM name
    memcpy(bpb.oem_name, "KIRAOS  ", 8);
    
    // Basic FAT32 parameters
    bpb.bytes_per_sector = 512;
    bpb.sectors_per_cluster = 8;  // 4KB clusters
    bpb.reserved_sector_count = 32;
    bpb.num_fats = 2;
    bpb.root_entry_count = 0;  // FAT32 uses cluster chain for root
    bpb.total_sectors_16 = 0;  // Use 32-bit field
    bpb.media = 0xF8;  // Hard disk
    bpb.fat_size_16 = 0;  // Use 32-bit field
    bpb.sectors_per_track = 63;
    bpb.num_heads = 255;
    bpb.hidden_sectors = 0;
    bpb.total_sectors_32 = 65536;  // 32MB
    bpb.fat_size_32 = 1024;  // FAT size in sectors
    bpb.ext_flags = 0;
    bpb.fs_version = 0;
    bpb.root_cluster = 2;  // Root directory starts at cluster 2
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
    
    // Initialize FAT tables (sectors 32-1055 and 1056-2079)
    u8 fatSector[512];
    memset(fatSector, 0, 512);
    
    // First FAT sector has special entries
    u32* fatEntries = reinterpret_cast<u32*>(fatSector);
    fatEntries[0] = 0x0FFFFFF8;  // Media descriptor + end-of-chain
    fatEntries[1] = 0x0FFFFFFF;  // End-of-chain
    fatEntries[2] = 0x0FFFFFFF;  // Root directory (single cluster)
    
    // Write first FAT sector to both FAT copies
    if (device->write_blocks(32, 1, fatSector) != FSResult::SUCCESS) return false;
    if (device->write_blocks(1056, 1, fatSector) != FSResult::SUCCESS) return false;
    
    // Initialize remaining FAT sectors (all zeros = free clusters)
    memset(fatSector, 0, 512);
    for (u32 sector = 33; sector < 1056; sector++) {
        if (device->write_blocks(sector, 1, fatSector) != FSResult::SUCCESS) return false;
    }
    for (u32 sector = 1057; sector < 2080; sector++) {
        if (device->write_blocks(sector, 1, fatSector) != FSResult::SUCCESS) return false;
    }
    
    // Initialize root directory cluster (cluster 2 = sectors 2080-2087)
    u8 rootCluster[4096];  // 8 sectors * 512 bytes
    memset(rootCluster, 0, 4096);  // Empty directory
    
    FSResult rootWriteResult = device->write_blocks(2080, 8, rootCluster);
    if (rootWriteResult != FSResult::SUCCESS) {
        return false;
    }
    
    return true;
}

void FAT32Test::print_test_result(const char* testName, bool passed) {
    char msg[256];
    strcpy_s(msg, testName, sizeof(msg));
    if (passed) {
        strcat(msg, ": PASSED");
        console.add_message(msg, VGA_GREEN_ON_BLUE);
    } else {
        strcat(msg, ": FAILED");
        console.add_message(msg, VGA_RED_ON_BLUE);
    }
}

} // namespace kira::test 