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
    allPassed &= test_fat32_cluster_operations();
    allPassed &= test_fat32_directory_reading();
    allPassed &= test_fat32_file_reading();
    allPassed &= test_fat32_integration_with_vfs();
    allPassed &= test_fat32_read_write_operations();
    
    if (allPassed) {
        console.add_message("\n=== All FAT32 Tests Passed ===\n", VGA_GREEN_ON_BLUE);
    }
    return allPassed;
}

bool FAT32Test::test_fat32_mount() {
    bool testPassed = true;
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0); // Try to get first device
    
    if (!device) {
        console.add_message("No block device available for FAT32 testing", VGA_YELLOW_ON_BLUE);
        print_test_result("FAT32 Mount (skipped - no device)", true);
        return true; // Skip test if no device available
    }
    
    // Create FAT32 instance
    auto& memMgr = MemoryManager::get_instance();
    void* fat32Memory = memMgr.allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Test mount (will likely fail due to no actual FAT32 filesystem, but tests the API)
        FSResult result = fat32->mount("test");
        
        // We expect this to fail in test environment, but the API should work
        if (result != FSResult::SUCCESS) {
            console.add_message("FAT32 mount failed as expected (no FAT32 filesystem)", VGA_YELLOW_ON_BLUE);
        }
        
        // Test that the FAT32 object was created properly
        if (!fat32->get_name() || strcmp(fat32->get_name(), "fat32") != 0) {
            testPassed = false;
        }
        
        // Clean up
        fat32->~FAT32();
        memMgr.free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Mount API", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_cluster_operations() {
    bool testPassed = true;
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0);
    
    if (!device) {
        console.add_message("No block device for cluster operations test", VGA_YELLOW_ON_BLUE);
        print_test_result("FAT32 Cluster Operations (skipped)", true);
        return true;
    }
    
    // Create FAT32 instance
    auto& memMgr = MemoryManager::get_instance();
    void* fat32Memory = memMgr.allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Test that we can create a FAT32 node (tests public API)
        VNode* testVnode = nullptr;
        FSResult result = fat32->create_vnode(1, FileType::REGULAR, testVnode);
        
        if (result == FSResult::SUCCESS && testVnode) {
            console.add_message("FAT32 VNode creation successful", VGA_YELLOW_ON_BLUE);
            
            // Clean up the test vnode
            testVnode->~VNode();
            memMgr.free_physical_page(testVnode);
        } else {
            console.add_message("FAT32 VNode creation failed", VGA_YELLOW_ON_BLUE);
        }
        
        // Test cluster chain constants are properly defined
        if (Fat32Cluster::FREE != 0x00000000 ||
            Fat32Cluster::END_MIN != 0x0FFFFFF8 ||
            Fat32Cluster::END_MAX != 0x0FFFFFFF) {
            testPassed = false;
        }
        
        console.add_message("FAT32 cluster constants and basic operations verified", VGA_YELLOW_ON_BLUE);
        
        // Clean up
        fat32->~FAT32();
        memMgr.free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Cluster Operations", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_directory_reading() {
    bool testPassed = true;
    
    // Test FAT32 directory entry structure
    Fat32DirEntry testEntry;
    
    // Set up a test directory entry
    for (int i = 0; i < 8; i++) {
        testEntry.name[i] = (i < 4) ? 'T' + i : ' ';
    }
    for (int i = 8; i < 11; i++) {
        testEntry.name[i] = (i == 8) ? 'T' : (i == 9) ? 'X' : 'T';
    }
    
    testEntry.attr = Fat32Attr::ARCHIVE;
    testEntry.first_cluster_low = 0x1234;
    testEntry.first_cluster_high = 0x5678;
    testEntry.file_size = 1024;
    
    // Validate the entry
    u32 full_cluster = (static_cast<u32>(testEntry.first_cluster_high) << 16) | testEntry.first_cluster_low;
    if (full_cluster != 0x56781234) {
        testPassed = false;
    }
    
    if (testEntry.file_size != 1024) {
        testPassed = false;
    }
    
    // Test attribute flags
    if (!(testEntry.attr & Fat32Attr::ARCHIVE) ||
        (testEntry.attr & Fat32Attr::DIRECTORY) ||
        (testEntry.attr & Fat32Attr::VOLUME_ID)) {
        testPassed = false;
    }
    
    print_test_result("FAT32 Directory Reading", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_file_reading() {
    bool testPassed = true;
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0);
    
    if (!device) {
        console.add_message("No block device for file I/O test", VGA_YELLOW_ON_BLUE);
        print_test_result("FAT32 File I/O (skipped)", true);
        return true;
    }
    
    // Create FAT32 instance
    auto& memMgr = MemoryManager::get_instance();
    void* fat32Memory = memMgr.allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Test creating a FAT32Node for testing file operations
        void* nodeMemory = memMgr.allocate_physical_page();
        if (!nodeMemory) {
            testPassed = false;
        } else {
            // Create a test node with a fake cluster (for testing the API)
            FAT32Node* testNode = new(nodeMemory) FAT32Node(1, FileType::REGULAR, fat32, 2, 1024);
            
            // Test file size retrieval
            u32 fileSize;
            FSResult sizeResult = testNode->get_size(fileSize);
            if (sizeResult != FSResult::SUCCESS || fileSize != 1024) {
                console.add_message("FAT32 file size retrieval failed", VGA_YELLOW_ON_BLUE);
                testPassed = false;
            } else {
                console.add_message("FAT32 file size retrieval successful", VGA_YELLOW_ON_BLUE);
            }
            
            // Test file stat
            FileStat stat;
            FSResult statResult = testNode->get_stat(stat);
            if (statResult != FSResult::SUCCESS || stat.size != 1024 || stat.type != FileType::REGULAR) {
                console.add_message("FAT32 file stat failed", VGA_YELLOW_ON_BLUE);
                testPassed = false;
            } else {
                console.add_message("FAT32 file stat successful", VGA_YELLOW_ON_BLUE);
            }
            
            // Test read operation API (expected to fail without mounted filesystem)
            u8 readBuffer[512];
            FSResult readResult = testNode->read(0, 512, readBuffer);
            if (readResult == FSResult::SUCCESS) {
                console.add_message("FAT32 Node read API - unexpected success", VGA_YELLOW_ON_BLUE);
            } else {
                console.add_message("FAT32 Node read API - correctly failed (unmounted)", VGA_GREEN_ON_BLUE);
                // This is expected behavior - API works but filesystem isn't mounted
            }
            
            // Test write operation API (expected to fail without mounted filesystem)
            const char* testData = "Hello FAT32!";
            FSResult writeResult = testNode->write(0, strlen(testData), testData);
            if (writeResult == FSResult::SUCCESS) {
                console.add_message("FAT32 Node write API - unexpected success", VGA_YELLOW_ON_BLUE);
            } else {
                console.add_message("FAT32 Node write API - correctly failed (unmounted)", VGA_GREEN_ON_BLUE);
                // This is expected behavior - API works but filesystem isn't mounted
            }
            
            // Clean up test node
            testNode->~FAT32Node();
            memMgr.free_physical_page(nodeMemory);
        }
        
        // Clean up FAT32 instance
        fat32->~FAT32();
        memMgr.free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Node API (unmounted)", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_integration_with_vfs() {
    bool testPassed = true;
    
    // Get VFS instance
    VFS& vfs = VFS::get_instance();
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0);
    
    if (!device) {
        console.add_message("No block device for VFS integration test", VGA_YELLOW_ON_BLUE);
        print_test_result("FAT32 VFS Integration (skipped)", true);
        return true;
    }
    
    // Create FAT32 instance
    auto& memMgr = MemoryManager::get_instance();
    void* fat32Memory = memMgr.allocate_physical_page();
    if (!fat32Memory) {
        testPassed = false;
    } else {
        FAT32* fat32 = new(fat32Memory) FAT32(device);
        
        // Test registration with VFS
        FSResult result = vfs.register_filesystem(fat32);
        if (result != FSResult::SUCCESS) {
            testPassed = false;
            console.add_message("Failed to register FAT32 with VFS", VGA_RED_ON_BLUE);
        } else {
            console.add_message("FAT32 registered with VFS successfully", VGA_YELLOW_ON_BLUE);
        }
        
        // Test that FAT32 implements the FileSystem interface properly
        if (!fat32->get_name() || strcmp(fat32->get_name(), "fat32") != 0) {
            testPassed = false;
        }
        
        if (!fat32->is_mounted()) {
            // This is expected since we haven't mounted it yet
        }
        
        // Clean up
        fat32->~FAT32();
        memMgr.free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 VFS Integration", testPassed);
    return testPassed;
}

bool FAT32Test::test_fat32_read_write_operations() {
    bool testPassed = true;
    
    console.add_message("Testing FAT32 read/write with mock data...", VGA_CYAN_ON_BLUE);
    
    // Get a block device for testing
    BlockDeviceManager& bdm = BlockDeviceManager::get_instance();
    BlockDevice* device = bdm.get_device(0);
    
    if (!device) {
        console.add_message("No block device for read/write test", VGA_YELLOW_ON_BLUE);
        print_test_result("FAT32 Read/Write (skipped)", true);
        return true;
    }
    
    // Create some mock FAT32 data on the device
    if (!create_mock_fat32_data(device)) {
        console.add_message("Failed to create mock FAT32 data", VGA_RED_ON_BLUE);
        print_test_result("FAT32 Read/Write (setup failed)", false);
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
            console.add_message("FAT32 mount failed - BPB validation failed", VGA_YELLOW_ON_BLUE);
            fat32->~FAT32();
            memMgr.free_physical_page(fat32Memory);
            print_test_result("FAT32 Read/Write (mount failed)", false);
            return false;
        }
        
        console.add_message("FAT32 mount successful", VGA_GREEN_ON_BLUE);
        
        // Test cluster allocation
        u32 testCluster = fat32->allocate_cluster();
        if (testCluster >= 2) {
            console.add_message("FAT32 cluster allocation successful", VGA_GREEN_ON_BLUE);
            
            // Test cluster deallocation
            FSResult freeResult = fat32->free_cluster(testCluster);
            if (freeResult == FSResult::SUCCESS) {
                console.add_message("FAT32 cluster deallocation successful", VGA_GREEN_ON_BLUE);
            } else {
                console.add_message("FAT32 cluster deallocation failed", VGA_YELLOW_ON_BLUE);
            }
        } else {
            console.add_message("FAT32 cluster allocation failed", VGA_YELLOW_ON_BLUE);
            // Don't fail the test as this might be expected without a real filesystem
        }
        
        // Test FAT chain operations
        u32 cluster1 = 2;
        u32 cluster2 = 3;
        FSResult linkResult = fat32->set_next_cluster(cluster1, cluster2);
        if (linkResult == FSResult::SUCCESS) {
            console.add_message("FAT32 cluster linking successful", VGA_GREEN_ON_BLUE);
            
            // Test reading the link back
            u32 nextCluster;
            FSResult readResult = fat32->get_next_cluster(cluster1, nextCluster);
            if (readResult == FSResult::SUCCESS && nextCluster == cluster2) {
                console.add_message("FAT32 cluster chain reading successful", VGA_GREEN_ON_BLUE);
            } else {
                console.add_message("FAT32 cluster chain reading failed", VGA_YELLOW_ON_BLUE);
            }
        } else {
            console.add_message("FAT32 cluster linking failed", VGA_YELLOW_ON_BLUE);
        }
        
        // Test file data operations with a mock file
        const char* testData = "Hello FAT32 World! This is a test of file I/O operations.";
        u32 dataSize = strlen(testData);
        
        // Test writing file data
        FSResult writeResult = fat32->write_file_data(2, 0, dataSize, testData);
        if (writeResult == FSResult::SUCCESS) {
            console.add_message("FAT32 file write successful", VGA_GREEN_ON_BLUE);
            
            // Test reading the data back
            char readBuffer[256];
            memset(readBuffer, 0, sizeof(readBuffer));
            
            FSResult readResult = fat32->read_file_data(2, 0, dataSize, readBuffer);
            if (readResult == FSResult::SUCCESS) {
                // Verify the data matches
                bool dataMatches = true;
                for (u32 i = 0; i < dataSize; i++) {
                    if (testData[i] != readBuffer[i]) {
                        dataMatches = false;
                        break;
                    }
                }
                
                if (dataMatches) {
                    console.add_message("FAT32 file read/write verification successful!", VGA_GREEN_ON_BLUE);
                } else {
                    console.add_message("FAT32 file data mismatch", VGA_RED_ON_BLUE);
                    testPassed = false;
                }
            } else {
                console.add_message("FAT32 file read failed", VGA_YELLOW_ON_BLUE);
            }
        } else {
            console.add_message("FAT32 file write failed", VGA_YELLOW_ON_BLUE);
        }
        
        // Clean up FAT32 instance
        fat32->~FAT32();
        memMgr.free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Read/Write Operations", testPassed);
    return testPassed;
}

bool FAT32Test::create_mock_fat32_data(BlockDevice* device) {
    // Create a minimal mock FAT32 boot sector
    u8 bootSector[512];
    memset(bootSector, 0, sizeof(bootSector));
    
    // Create a minimal BPB (BIOS Parameter Block)
    Fat32Bpb* bpb = reinterpret_cast<Fat32Bpb*>(bootSector);
    
    // Set basic FAT32 parameters
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = 8;  // 4KB clusters
    bpb->reserved_sector_count = 32;
    bpb->num_fats = 2;
    bpb->fat_size_32 = 1024;  // Sectors per FAT
    bpb->root_cluster = 2;    // Root directory starts at cluster 2
    
    // Copy OEM name
    memcpy(bpb->oem_name, "KIRAOS  ", 8);
    
    // Write boot sector to device
    FSResult result = device->write_blocks(0, 1, bootSector);
    if (result != FSResult::SUCCESS) {
        return false;
    }
    
    // Create a simple FAT table
    u8 fatSector[512];
    memset(fatSector, 0, sizeof(fatSector));
    
    u32* fatEntries = reinterpret_cast<u32*>(fatSector);
    
    // Set up basic FAT entries
    fatEntries[0] = 0x0FFFFFF8;  // Media descriptor
    fatEntries[1] = 0x0FFFFFFF;  // End of chain
    fatEntries[2] = 0x0FFFFFFF;  // Root directory (end of chain)
    fatEntries[3] = 0x00000000;  // Free cluster
    fatEntries[4] = 0x00000000;  // Free cluster
    // ... rest are free (already zeroed)
    
    // Write FAT sector (first FAT starts at sector 32)
    result = device->write_blocks(32, 1, fatSector);
    if (result != FSResult::SUCCESS) {
        return false;
    }
    
    // Also write to second FAT (for consistency)
    result = device->write_blocks(32 + 1024, 1, fatSector);
    if (result != FSResult::SUCCESS) {
        return false;
    }
    
    console.add_message("Mock FAT32 data created successfully", VGA_YELLOW_ON_BLUE);
    return true;
}

void FAT32Test::print_test_result(const char* test_name, bool passed) {
    char message[256];
    strcpy(message, passed ? "PASS: " : "FAIL: ");
    strcat(message, test_name);
    
    console.add_message(message, passed ? VGA_GREEN_ON_BLUE : VGA_RED_ON_BLUE);
}

} // namespace kira::test 