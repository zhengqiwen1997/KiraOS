#include "test/vfs_test.hpp"
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

bool VFSTest::run_tests() {
    auto& console = kira::kernel::console;
    
    console.add_message("\n=== VFS Test Suite ===\n", kira::display::VGA_CYAN_ON_BLUE);
    
    // Test 1: VFS initialization
    if (!test_vfs_initialization()) {
        console.add_message("FAIL: VFS initialization failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: VFS initialization", kira::display::VGA_GREEN_ON_BLUE);
    
    // Test 2: File system mounting
    if (!test_filesystem_mounting()) {
        console.add_message("FAIL: File system mounting failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: File system mounting", kira::display::VGA_GREEN_ON_BLUE);
    
    // Test 3: File operations
    if (!test_file_operations()) {
        console.add_message("FAIL: File operations failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: File operations", kira::display::VGA_GREEN_ON_BLUE);
    
    // Test 4: Directory operations
    if (!test_directory_operations()) {
        console.add_message("FAIL: Directory operations failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: Directory operations", kira::display::VGA_GREEN_ON_BLUE);
    
    // Test 5: Path resolution
    if (!test_path_resolution()) {
        console.add_message("FAIL: Path resolution failed", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    console.add_message("PASS: Path resolution", kira::display::VGA_GREEN_ON_BLUE);
    
    console.add_message("\n=== All VFS Tests Passed ===\n", kira::display::VGA_GREEN_ON_BLUE);
    return true;
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
        console.add_message("Failed to register RamFS", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    // Mount the file system
    if (vfs.mount("ram0", "/", "ramfs") != FSResult::SUCCESS) {
        console.add_message("Failed to mount RamFS", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    console.add_message("RamFS mounted successfully", kira::display::VGA_YELLOW_ON_BLUE);
    return true;
}

bool VFSTest::test_file_operations() {
    auto& console = kira::kernel::console;
    VFS& vfs = VFS::get_instance();
    
    // Test file creation and writing
    i32 fd;
    FSResult result = vfs.open("/test.txt", static_cast<OpenFlags>(static_cast<u32>(OpenFlags::CREATE) | static_cast<u32>(OpenFlags::READ_WRITE)), fd);
    if (result != FSResult::SUCCESS) {
        console.add_message("Failed to create test file", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    // Write test data
    const char* test_data = "Hello, VFS World!";
    u32 test_len = strlen(test_data);
    
    result = vfs.write(fd, test_len, test_data);
    if (result != FSResult::SUCCESS) {
        console.add_message("Failed to write to test file", kira::display::VGA_RED_ON_BLUE);
        vfs.close(fd);
        return false;
    }
    
    // Close and reopen for reading
    vfs.close(fd);
    
    result = vfs.open("/test.txt", OpenFlags::READ_ONLY, fd);
    if (result != FSResult::SUCCESS) {
        console.add_message("Failed to reopen test file", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    // Read back the data
    char read_buffer[64];
    result = vfs.read(fd, test_len, read_buffer);
    if (result != FSResult::SUCCESS) {
        console.add_message("Failed to read from test file", kira::display::VGA_RED_ON_BLUE);
        vfs.close(fd);
        return false;
    }
    
    read_buffer[test_len] = '\0';
    
    // Verify data
    if (strcmp(read_buffer, test_data) != 0) {
        console.add_message("File data mismatch", kira::display::VGA_RED_ON_BLUE);
        vfs.close(fd);
        return false;
    }
    
    vfs.close(fd);
    console.add_message("File I/O operations successful", kira::display::VGA_YELLOW_ON_BLUE);
    return true;
}

bool VFSTest::test_directory_operations() {
    auto& console = kira::kernel::console;
    VFS& vfs = VFS::get_instance();
    
    // Test directory listing of root
    DirectoryEntry entry;
    FSResult result = vfs.readdir("/", 0, entry);
    if (result != FSResult::SUCCESS) {
        // Empty directory is ok for now
        if (result != FSResult::NOT_FOUND) {
            console.add_message("Failed to read root directory", kira::display::VGA_RED_ON_BLUE);
            return false;
        }
    } else {
        // If we found an entry, display it
        char msg[128];
        strcpy_s(msg, "Found file: ", sizeof(msg));
        strcat(msg, entry.name);
        console.add_message(msg, kira::display::VGA_YELLOW_ON_BLUE);
    }
    
    console.add_message("Directory operations functional", kira::display::VGA_YELLOW_ON_BLUE);
    return true;
}

bool VFSTest::test_path_resolution() {
    auto& console = kira::kernel::console;
    VFS& vfs = VFS::get_instance();
    
    // Test resolving root path
    VNode* root_vnode = nullptr;
    FSResult result = vfs.resolve_path("/", root_vnode);
    if (result != FSResult::SUCCESS || !root_vnode) {
        console.add_message("Failed to resolve root path", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    if (root_vnode->get_type() != FileType::DIRECTORY) {
        console.add_message("Root is not a directory", kira::display::VGA_RED_ON_BLUE);
        return false;
    }
    
    console.add_message("Path resolution working", kira::display::VGA_YELLOW_ON_BLUE);
    return true;
}

} // namespace kira::test 