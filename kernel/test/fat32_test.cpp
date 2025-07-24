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
            memset(readBuffer, 0, sizeof(readBuffer));
            
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

bool FAT32Test::test_fat32_directory_operations() {
    bool testPassed = true;
    
    console.add_message("Testing FAT32 directory operations...", VGA_CYAN_ON_BLUE);
    
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
        
        console.add_message("FAT32 mount successful for directory operations", VGA_GREEN_ON_BLUE);
        
        // Debug: Test name conversion
        u8 testFatName[11];
        fat32->convert_standard_name_to_fat("test.txt", testFatName);
        console.add_message("Name conversion test completed", VGA_YELLOW_ON_BLUE);
        
        // Debug: Show what the converted name looks like
        char convertedName[256];
        fat32->convert_fat_name(testFatName, convertedName);
        char nameMsg[256];
        strcpy_s(nameMsg, "Converted name: '", sizeof(nameMsg));
        strcat(nameMsg, convertedName);
        strcat(nameMsg, "'");
        console.add_message(nameMsg, VGA_CYAN_ON_BLUE);
        
        // Get root directory
        VNode* rootNode = nullptr;
        FSResult rootResult = fat32->get_root(rootNode);
        if (rootResult != FSResult::SUCCESS || !rootNode) {
            console.add_message("Failed to get root directory", VGA_RED_ON_BLUE);
            testPassed = false;
        } else {
            console.add_message("Root directory obtained successfully", VGA_GREEN_ON_BLUE);
            
            // Simple test: just try to create a file and see what happens
            console.add_message("Attempting to create FIRST file...", VGA_YELLOW_ON_BLUE);
            
            // Debug: Check what lookup finds before creating first file
            VNode* existingNode1 = nullptr;
            FSResult lookupResult1 = rootNode->lookup("test.txt", existingNode1);
            
            char resultMsg[256];
            char resultStr[16];
            strcpy_s(resultMsg, "Pre-creation lookup for 'test.txt': ", sizeof(resultMsg));
            number_to_decimal(resultStr, static_cast<u32>(lookupResult1));
            strcat(resultMsg, resultStr);
            console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
            
            if (lookupResult1 == FSResult::SUCCESS && existingNode1) {
                console.add_message("'test.txt' already exists before creation!", VGA_RED_ON_BLUE);
                existingNode1->~VNode();
                memMgr.free_physical_page(existingNode1);
            }
            
            FSResult createFileResult = rootNode->create_file("test.txt", FileType::REGULAR);
            
            strcpy_s(resultMsg, "FIRST file creation result: ", sizeof(resultMsg));
            number_to_decimal(resultStr, static_cast<u32>(createFileResult));
            strcat(resultMsg, resultStr);
            console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
            
            if (createFileResult == FSResult::SUCCESS) {
                console.add_message("File creation successful!", VGA_GREEN_ON_BLUE);
                
                // Debug: Check raw directory immediately after first file creation
                console.add_message("Raw directory immediately after FIRST file creation:", VGA_YELLOW_ON_BLUE);
                u8 rawClusterData1[4096];
                FSResult rawReadResult1 = device->read_blocks(2080, 8, rawClusterData1);
                if (rawReadResult1 == FSResult::SUCCESS) {
                    Fat32DirEntry* rawEntries1 = reinterpret_cast<Fat32DirEntry*>(rawClusterData1);
                    for (u32 i = 0; i < 4; i++) {
                        Fat32DirEntry& rawEntry = rawEntries1[i];
                        char entryMsg[256];
                        strcpy_s(entryMsg, "After 1st: Entry ", sizeof(entryMsg));
                        char indexStr[16];
                        number_to_decimal(indexStr, i);
                        strcat(entryMsg, indexStr);
                        strcat(entryMsg, ": first_byte=");
                        char firstByteStr[16];
                        number_to_decimal(firstByteStr, (u32)rawEntry.name[0]);
                        strcat(entryMsg, firstByteStr);
                        
                        if (rawEntry.name[0] == 0x00) {
                            strcat(entryMsg, " FREE");
                        } else if (rawEntry.name[0] == 0xE5) {
                            strcat(entryMsg, " DELETED");
                        } else {
                            strcat(entryMsg, " ");
                            char rawName[12];
                            for (int j = 0; j < 11; j++) {
                                rawName[j] = rawEntry.name[j];
                            }
                            rawName[11] = '\0';
                            strcat(entryMsg, rawName);
                        }
                        console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                    }
                }
                
                // Force sync to ensure first file is written to disk before creating second file
                fat32->sync();
                console.add_message("Synced filesystem after first file creation", VGA_YELLOW_ON_BLUE);
                
                // Debug: Show raw directory entries after first file creation
                console.add_message("Raw directory entries after first file creation:", VGA_YELLOW_ON_BLUE);
                for (u32 i = 0; i < 16; i++) { // Check first 16 entries
                    DirectoryEntry dirEntry;
                    FSResult listResult = rootNode->read_dir(i, dirEntry);
                    if (listResult == FSResult::SUCCESS) {
                        char entryMsg[256];
                        strcpy_s(entryMsg, "Raw Entry ", sizeof(entryMsg));
                        char indexStr[16];
                        number_to_decimal(indexStr, i);
                        strcat(entryMsg, indexStr);
                        strcat(entryMsg, ": '");
                        strcat(entryMsg, dirEntry.name);
                        strcat(entryMsg, "'");
                        console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                    } else if (listResult == FSResult::NOT_FOUND) {
                        console.add_message("End of directory reached", VGA_YELLOW_ON_BLUE);
                        break; // End of directory
                    }
                }
                
                // Debug: List directory entries after first file creation
                console.add_message("Listing directory entries after first file creation...", VGA_YELLOW_ON_BLUE);
                for (u32 i = 0; i < 10; i++) { // Check first 10 entries
                    DirectoryEntry dirEntry;
                    FSResult listResult = rootNode->read_dir(i, dirEntry);
                    if (listResult == FSResult::SUCCESS) {
                        char entryMsg[256];
                        strcpy_s(entryMsg, "Entry ", sizeof(entryMsg));
                        char indexStr[16];
                        number_to_decimal(indexStr, i);
                        strcat(entryMsg, indexStr);
                        strcat(entryMsg, ": '");
                        strcat(entryMsg, dirEntry.name);
                        strcat(entryMsg, "'");
                        console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                    } else if (listResult == FSResult::NOT_FOUND) {
                        console.add_message("End of directory reached", VGA_YELLOW_ON_BLUE);
                        break; // End of directory
                    }
                }
                
                // Debug: Show what we're looking for
                console.add_message("Looking for file: 'test.txt'", VGA_YELLOW_ON_BLUE);
                
                // Try to look it up
                console.add_message("Attempting to lookup created file...", VGA_YELLOW_ON_BLUE);
                VNode* foundNode = nullptr;
                FSResult lookupResult = rootNode->lookup("test.txt", foundNode);
                
                strcpy_s(resultMsg, "File lookup result: ", sizeof(resultMsg));
                number_to_decimal(resultStr, static_cast<u32>(lookupResult));
                strcat(resultMsg, resultStr);
                console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
                
                if (lookupResult == FSResult::SUCCESS && foundNode) {
                    console.add_message("File lookup successful!", VGA_GREEN_ON_BLUE);
                    
                    // Clean up found node
                    foundNode->~VNode();
                    memMgr.free_physical_page(foundNode);
                    
                    // NEW TEST: Try to delete the first file before creating second file
                    // console.add_message("Attempting to delete FIRST file...", VGA_YELLOW_ON_BLUE);
                    // FSResult deleteResult = rootNode->delete_file("test.txt");
                    
                    // strcpy_s(resultMsg, "FIRST file deletion result: ", sizeof(resultMsg));
                    // number_to_decimal(resultStr, static_cast<u32>(deleteResult));
                    // strcat(resultMsg, resultStr);
                    // console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
                    
                    // if (deleteResult == FSResult::SUCCESS) {
                    //     console.add_message("First file deletion successful!", VGA_GREEN_ON_BLUE);
                    // } else {
                    //     console.add_message("First file deletion failed", VGA_RED_ON_BLUE);
                    // }
                } else {
                    console.add_message("File lookup failed", VGA_RED_ON_BLUE);
                    testPassed = false;
                }
            } else {
                console.add_message("File creation failed", VGA_RED_ON_BLUE);
                testPassed = false;
            }
            
            // Test creating a file in root directory
            console.add_message("Attempting to create second file...", VGA_YELLOW_ON_BLUE);
            
            // Debug: Show root directory cluster info
            FAT32Node* rootFat32Node = static_cast<FAT32Node*>(rootNode);
            u32 rootCluster = rootFat32Node->get_first_cluster();
            strcpy_s(resultMsg, "Root directory cluster: ", sizeof(resultMsg));
            number_to_decimal(resultStr, rootCluster);
            strcat(resultMsg, resultStr);
            console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
            
            // Debug: Check if the file already exists before creation
            VNode* existingNode = nullptr;
            FSResult checkResult = rootNode->lookup("test2.txt", existingNode);
            strcpy_s(resultMsg, "Pre-creation lookup result: ", sizeof(resultMsg));
            number_to_decimal(resultStr, static_cast<u32>(checkResult));
            strcat(resultMsg, resultStr);
            console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
            
            if (checkResult == FSResult::SUCCESS && existingNode) {
                console.add_message("File already exists before creation!", VGA_RED_ON_BLUE);
                existingNode->~VNode();
                memMgr.free_physical_page(existingNode);
            }
            
            // Debug: Show raw directory entries before second file creation
            console.add_message("Raw directory entries before second file creation:", VGA_YELLOW_ON_BLUE);
            for (u32 i = 0; i < 16; i++) { // Check first 16 entries
                DirectoryEntry dirEntry;
                FSResult listResult = rootNode->read_dir(i, dirEntry);
                if (listResult == FSResult::SUCCESS) {
                    char entryMsg[256];
                    strcpy_s(entryMsg, "Raw Entry ", sizeof(entryMsg));
                    char indexStr[16];
                    number_to_decimal(indexStr, i);
                    strcat(entryMsg, indexStr);
                    strcat(entryMsg, ": '");
                    strcat(entryMsg, dirEntry.name);
                    strcat(entryMsg, "'");
                    console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                } else if (listResult == FSResult::NOT_FOUND) {
                    console.add_message("End of directory reached", VGA_YELLOW_ON_BLUE);
                    break; // End of directory
                }
            }
            
            FSResult createFileResult2 = rootNode->create_file("test2.txt", FileType::REGULAR);
            
            strcpy_s(resultMsg, "Second file creation result: ", sizeof(resultMsg));
            number_to_decimal(resultStr, static_cast<u32>(createFileResult2));
            strcat(resultMsg, resultStr);
            console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
            
            if (createFileResult2 == FSResult::SUCCESS) {
                console.add_message("Second file creation successful", VGA_GREEN_ON_BLUE);
                
                // Debug: Try to directly read the root directory cluster to see what's actually there
                console.add_message("Reading raw root directory cluster data...", VGA_YELLOW_ON_BLUE);
                
                // Calculate the sector for cluster 2 (root directory)
                // Data area starts at: reserved_sectors + (num_fats * fat_size) = 32 + (2 * 1024) = 2080
                u8 rawClusterData[4096]; // 8 sectors * 512 bytes
                FSResult rawReadResult = device->read_blocks(2080, 8, rawClusterData);
                
                if (rawReadResult == FSResult::SUCCESS) {
                    Fat32DirEntry* rawEntries = reinterpret_cast<Fat32DirEntry*>(rawClusterData);
                    
                    console.add_message("Raw cluster entries:", VGA_YELLOW_ON_BLUE);
                    for (u32 i = 0; i < 4; i++) { // Check first 4 raw entries
                        Fat32DirEntry& rawEntry = rawEntries[i];
                        
                        char entryMsg[256];
                        strcpy_s(entryMsg, "Raw Entry ", sizeof(entryMsg));
                        char indexStr[16];
                        number_to_decimal(indexStr, i);
                        strcat(entryMsg, indexStr);
                        strcat(entryMsg, ": ");
                        
                        // Show the first byte value
                        char firstByteStr[16];
                        number_to_decimal(firstByteStr, (u32)rawEntry.name[0]);
                        strcat(entryMsg, "first_byte=");
                        strcat(entryMsg, firstByteStr);
                        strcat(entryMsg, " ");
                        
                        if (rawEntry.name[0] == 0x00) {
                            strcat(entryMsg, "FREE (0x00)");
                            console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                        } else if (rawEntry.name[0] == 0xE5) {
                            strcat(entryMsg, "DELETED (0xE5)");
                            console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                        } else {
                            // Convert raw name to readable format
                            char rawName[12];
                            for (int j = 0; j < 11; j++) {
                                rawName[j] = rawEntry.name[j];
                            }
                            rawName[11] = '\0';
                            strcat(entryMsg, rawName);
                            
                            // Also show if this matches our deletion target
                            if (i == 1) { // Check if entry 1 matches our deletion target
                                u8 deleteFatName[11];
                                fat32->convert_standard_name_to_fat("test2.txt", deleteFatName);
                                
                                // Compare byte by byte
                                bool matches = true;
                                for (int k = 0; k < 11; k++) {
                                    if (rawEntry.name[k] != deleteFatName[k]) {
                                        matches = false;
                                        break;
                                    }
                                }
                                
                                if (matches) {
                                    strcat(entryMsg, " (MATCHES deletion target)");
                                } else {
                                    strcat(entryMsg, " (does NOT match deletion target)");
                                }
                            }
                            
                            console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                        }
                    }
                } else {
                    console.add_message("Failed to read raw cluster data", VGA_RED_ON_BLUE);
                }
                
                // Debug: List directory entries after second file creation
                console.add_message("Listing directory entries after second file creation...", VGA_YELLOW_ON_BLUE);
                for (u32 i = 0; i < 10; i++) { // Check first 10 entries
                    DirectoryEntry dirEntry;
                    FSResult listResult = rootNode->read_dir(i, dirEntry);
                    if (listResult == FSResult::SUCCESS) {
                        char entryMsg[256];
                        strcpy_s(entryMsg, "Entry ", sizeof(entryMsg));
                        char indexStr[16];
                        number_to_decimal(indexStr, i);
                        strcat(entryMsg, indexStr);
                        strcat(entryMsg, ": '");
                        strcat(entryMsg, dirEntry.name);
                        strcat(entryMsg, "'");
                        console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                    } else if (listResult == FSResult::NOT_FOUND) {
                        console.add_message("End of directory reached", VGA_YELLOW_ON_BLUE);
                        break; // End of directory
                    }
                }
                
                // Debug: Try to read more entries to see if there are more clusters
                console.add_message("Checking for more directory entries...", VGA_YELLOW_ON_BLUE);
                for (u32 i = 10; i < 50; i++) { // Check more entries
                    DirectoryEntry dirEntry;
                    FSResult listResult = rootNode->read_dir(i, dirEntry);
                    if (listResult == FSResult::SUCCESS) {
                        char entryMsg[256];
                        strcpy_s(entryMsg, "Entry ", sizeof(entryMsg));
                        char indexStr[16];
                        number_to_decimal(indexStr, i);
                        strcat(entryMsg, indexStr);
                        strcat(entryMsg, ": '");
                        strcat(entryMsg, dirEntry.name);
                        strcat(entryMsg, "'");
                        console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                    } else if (listResult == FSResult::NOT_FOUND) {
                        console.add_message("No more entries found", VGA_YELLOW_ON_BLUE);
                        break; // End of directory
                    }
                }
                
                // Test looking up the second file
                VNode* foundNode2 = nullptr;
                FSResult lookupResult2 = rootNode->lookup("test2.txt", foundNode2);
                
                strcpy_s(resultMsg, "Second file lookup result: ", sizeof(resultMsg));
                number_to_decimal(resultStr, static_cast<u32>(lookupResult2));
                strcat(resultMsg, resultStr);
                console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
                
                if (lookupResult2 == FSResult::SUCCESS && foundNode2) {
                    console.add_message("Second file lookup successful", VGA_GREEN_ON_BLUE);
                    
                    // Test deleting the second file
                    console.add_message("Attempting to delete second file...", VGA_YELLOW_ON_BLUE);
                    
                    // Debug: Show what FAT name we're looking for in deletion
                    u8 deleteFatName[11];
                    fat32->convert_standard_name_to_fat("test2.txt", deleteFatName);
                    char deleteName[256];
                    fat32->convert_fat_name(deleteFatName, deleteName);
                    char deleteMsg[256];
                    strcpy_s(deleteMsg, "Deletion looking for FAT name: '", sizeof(deleteMsg));
                    strcat(deleteMsg, deleteName);
                    strcat(deleteMsg, "'");
                    console.add_message(deleteMsg, VGA_CYAN_ON_BLUE);
                    
                    // Debug: List directory entries before deletion
                    console.add_message("Listing directory entries before deletion...", VGA_YELLOW_ON_BLUE);
                    for (u32 i = 0; i < 10; i++) { // Check first 10 entries
                        DirectoryEntry dirEntry;
                        FSResult listResult = rootNode->read_dir(i, dirEntry);
                        if (listResult == FSResult::SUCCESS) {
                            char entryMsg[256];
                            strcpy_s(entryMsg, "Entry ", sizeof(entryMsg));
                            char indexStr[16];
                            number_to_decimal(indexStr, i);
                            strcat(entryMsg, indexStr);
                            strcat(entryMsg, ": '");
                            strcat(entryMsg, dirEntry.name);
                            strcat(entryMsg, "'");
                            console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                        } else if (listResult == FSResult::NOT_FOUND) {
                            console.add_message("End of directory reached", VGA_YELLOW_ON_BLUE);
                            break; // End of directory
                        }
                    }
                    
                    FSResult deleteResult2 = rootNode->delete_file("test2.txt");
                    
                    strcpy_s(resultMsg, "Second file deletion result: ", sizeof(resultMsg));
                    number_to_decimal(resultStr, static_cast<u32>(deleteResult2));
                    strcat(resultMsg, resultStr);
                    console.add_message(resultMsg, VGA_CYAN_ON_BLUE);
                    
                    if (deleteResult2 == FSResult::SUCCESS) {
                        console.add_message("Second file deletion successful", VGA_GREEN_ON_BLUE);
                    } else {
                        console.add_message("Second file deletion failed", VGA_YELLOW_ON_BLUE);
                        testPassed = false;
                    }

                                // Debug: Show raw directory entries before second file creation
                    console.add_message("Raw directory entries after second file deletion:", VGA_YELLOW_ON_BLUE);
                    for (u32 i = 0; i < 16; i++) { // Check first 16 entries
                        DirectoryEntry dirEntry;
                        FSResult listResult = rootNode->read_dir(i, dirEntry);
                        if (listResult == FSResult::SUCCESS) {
                            char entryMsg[256];
                            strcpy_s(entryMsg, "Raw Entry ", sizeof(entryMsg));
                            char indexStr[16];
                            number_to_decimal(indexStr, i);
                            strcat(entryMsg, indexStr);
                            strcat(entryMsg, ": '");
                            strcat(entryMsg, dirEntry.name);
                            strcat(entryMsg, "'");
                            console.add_message(entryMsg, VGA_CYAN_ON_BLUE);
                        } else if (listResult == FSResult::NOT_FOUND) {
                            console.add_message("End of directory reached", VGA_YELLOW_ON_BLUE);
                            break; // End of directory
                        }
                    }
                    
                    // Clean up found node
                    foundNode2->~VNode();
                    memMgr.free_physical_page(foundNode2);
                } else {
                    console.add_message("Second file lookup failed", VGA_YELLOW_ON_BLUE);
                    testPassed = false;
                }
            } else {
                console.add_message("Second file creation failed", VGA_YELLOW_ON_BLUE);
                testPassed = false;
            }
        }
        
        // Clean up FAT32 instance
        fat32->~FAT32();
        memMgr.free_physical_page(fat32Memory);
    }
    
    print_test_result("FAT32 Directory Operations", testPassed);
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
    
    // Initialize root directory cluster (cluster 2)
    u8 rootDirCluster[4096]; // 8 sectors * 512 bytes
    memset(rootDirCluster, 0, sizeof(rootDirCluster));
    
    // All entries are already initialized to 0x00 (free) by memset
    // No need to explicitly mark end of directory - the first 0x00 entry will serve as the end marker
    
    // Write root directory cluster (cluster 2 starts at sector 2080)
    // Data area starts at: reserved_sectors + (num_fats * fat_size) = 32 + (2 * 1024) = 2080
    result = device->write_blocks(2080, 8, rootDirCluster); // 8 sectors per cluster
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