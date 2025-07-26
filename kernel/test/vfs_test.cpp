#include "test/vfs_test.hpp"
#include "memory/memory_manager.hpp"

namespace kira::test {

using namespace kira::system;
using namespace kira::fs;

bool VFSTest::run_tests() {
    print_section_header("VFS Test Suite");
    
    u32 passedTests = 0;
    u32 totalTests = 5;
    
    if (test_vfs_initialization()) passedTests++;
    if (test_filesystem_mounting()) passedTests++;
    if (test_file_operations()) passedTests++;
    if (test_directory_operations()) passedTests++;
    if (test_path_resolution()) passedTests++;
    
    print_section_footer("VFS Test Suite", passedTests, totalTests);
    return (passedTests == totalTests);
}

bool VFSTest::test_vfs_initialization() {
    VFS& vfs = VFS::get_instance();
    return vfs.initialize() == FSResult::SUCCESS;
}

bool VFSTest::test_filesystem_mounting() {
    auto& console = kira::kernel::console;
    VFS& vfs = VFS::get_instance();
    
    // Create RamFS instance
    auto& memMgr = MemoryManager::get_instance();
    void* memory = memMgr.allocate_physical_page();
    if (!memory) {
        return false;
    }
    
    RamFS* ramfs = new(memory) RamFS();
    
    // Register the file system
    if (vfs.register_filesystem(ramfs) != FSResult::SUCCESS) {
        print_error("Failed to register RamFS");
        return false;
    }
    
    // Mount the file system
    if (vfs.mount("ram0", "/", "ramfs") != FSResult::SUCCESS) {
        print_error("Failed to mount RamFS");
        return false;
    }
    
    print_success("RamFS mounted successfully");
    return true;
}

bool VFSTest::test_file_operations() {
    VFS& vfs = VFS::get_instance();
    
    // Test file creation and writing
    i32 fd;
    FSResult result = vfs.open("/test.txt", static_cast<OpenFlags>(static_cast<u32>(OpenFlags::CREATE) | static_cast<u32>(OpenFlags::READ_WRITE)), fd);
    if (result != FSResult::SUCCESS) {
        print_error("Failed to create test file");
        return false;
    }
    
    // Write test data
    const char* test_data = "Hello, VFS World!";
    u32 test_len = strlen(test_data);
    
    result = vfs.write(fd, test_len, test_data);
    if (result != FSResult::SUCCESS) {
        print_error("Failed to write to test file");
        vfs.close(fd);
        return false;
    }
    
    // Close and reopen for reading
    vfs.close(fd);
    
    result = vfs.open("/test.txt", OpenFlags::READ_ONLY, fd);
    if (result != FSResult::SUCCESS) {
        print_error("Failed to reopen test file");
        return false;
    }
    
    // Read back the data
    char read_buffer[64];
    result = vfs.read(fd, test_len, read_buffer);
    if (result != FSResult::SUCCESS) {
        print_error("Failed to read from test file");
        vfs.close(fd);
        return false;
    }
    
    read_buffer[test_len] = '\0';
    
    // Verify data
    if (strcmp(read_buffer, test_data) != 0) {
        print_error("File data mismatch");
        vfs.close(fd);
        return false;
    }
    
    vfs.close(fd);
    print_success("File I/O operations successful");
    return true;
}

bool VFSTest::test_directory_operations() {
    VFS& vfs = VFS::get_instance();
    
    // Test directory listing of root
    DirectoryEntry entry;
    FSResult result = vfs.readdir("/", 0, entry);
    if (result != FSResult::SUCCESS) {
        // Empty directory is ok for now
        if (result != FSResult::NOT_FOUND) {
            print_error("Failed to read root directory");
            return false;
        }
    } else {
        // If we found an entry, display it
        char msg[128];
        strcpy_s(msg, "Found file: ", sizeof(msg));
        strcat(msg, entry.name);
        print_debug(msg);
    }
    
    print_success("Directory operations functional");
    return true;
}

bool VFSTest::test_path_resolution() {
    VFS& vfs = VFS::get_instance();
    
    // Test resolving root path
    VNode* root_vnode = nullptr;
    FSResult result = vfs.resolve_path("/", root_vnode);
    if (result != FSResult::SUCCESS || !root_vnode) {
        print_error("Failed to resolve root path");
        return false;
    }
    
    if (root_vnode->get_type() != FileType::DIRECTORY) {
        print_error("Root is not a directory");
        return false;
    }
    
    print_success("Path resolution working");
    return true;
}

} // namespace kira::test 