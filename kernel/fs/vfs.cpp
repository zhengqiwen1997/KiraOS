#include "fs/vfs.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"
#include "display/console.hpp"

// External console reference from kernel namespace (like other working files)
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::fs {

using namespace kira::system;
using namespace kira::utils;
using namespace kira::display;

// Safe string comparison without relying on strcmp
static bool safe_string_equals(const char* str1, const char* str2) {
    if (!str1 || !str2) {
        return false;
    }
    
    u32 i = 0;
    while (i < 256) { // Reasonable safety limit
        if (str1[i] != str2[i]) {
            return false;
        }
        if (str1[i] == '\0') {
            return true; // Both strings ended at the same position
        }
        i++;
    }
    return false; // Hit length limit, strings too long
}

// Static member definitions
VFS* VFS::s_instance = nullptr;

// Static VFS instance - use static allocation to avoid memory mapping issues
static VFS s_static_vfs_instance;

//=============================================================================
// VNode Implementation
//=============================================================================

VNode::VNode(u32 inode, FileType type, FileSystem* fs)
    : m_inode(inode), m_type(type), m_filesystem(fs) {
}

void VNode::release() {
    if (m_refCount > 0) {
        m_refCount--;
    }
    // Actual deletion is controlled by filesystem-specific caches.
}

//=============================================================================
// FileDescriptor Implementation
//=============================================================================

FileDescriptor::FileDescriptor(VNode* vnode, OpenFlags flags)
    : m_vnode(vnode), m_flags(flags), m_position(0) {
    if (m_vnode) {
        m_vnode->retain();
    }
}

FileDescriptor::~FileDescriptor() {
    if (m_vnode) {
        m_vnode->release();
    }
}

FSResult FileDescriptor::read(u32 size, void* buffer) {
    if (!buffer || size == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Check if file is open for reading
    if (static_cast<u32>(m_flags) & static_cast<u32>(OpenFlags::WRITE_ONLY)) {
        return FSResult::PERMISSION_DENIED;
    }
    
    FSResult result = m_vnode->read(m_position, size, buffer);
    if (result == FSResult::SUCCESS) {
        m_position += size;
    }
    
    return result;
}

FSResult FileDescriptor::write(u32 size, const void* buffer) {
    if (!buffer || size == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Check if file is open for writing
    u32 flags = static_cast<u32>(m_flags);
    if (!(flags & static_cast<u32>(OpenFlags::WRITE_ONLY)) && 
        !(flags & static_cast<u32>(OpenFlags::READ_WRITE))) {
        return FSResult::PERMISSION_DENIED;
    }
    
    // Handle append mode
    if (flags & static_cast<u32>(OpenFlags::APPEND)) {
        u32 file_size;
        if (m_vnode->get_size(file_size) == FSResult::SUCCESS) {
            m_position = file_size;
        }
    }
    
    FSResult result = m_vnode->write(m_position, size, buffer);
    if (result == FSResult::SUCCESS) {
        m_position += size;
    }
    
    return result;
}

FSResult FileDescriptor::seek(u32 position) {
    m_position = position;
    return FSResult::SUCCESS;
}

FSResult FileDescriptor::get_position(u32& position) const {
    position = m_position;
    return FSResult::SUCCESS;
}

FSResult FileDescriptor::get_size(u32& size) const {
    return m_vnode->get_size(size);
}

//=============================================================================
// VFS Implementation
//=============================================================================

VFS::VFS() : m_filesystemCount(0), m_rootVnode(nullptr) {
    kira::kernel::console.add_message("[VFS] Constructor called", VGA_CYAN_ON_BLUE);
    
    // Initialize file descriptor table
    for (u32 i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        m_fileDescriptors[i] = nullptr;
    }
    
    // Initialize filesystem table
    for (u32 i = 0; i < MAX_FILESYSTEMS; i++) {
        m_filesystems[i] = nullptr;
    }
    
    kira::kernel::console.add_message("[VFS] Constructor completed - m_rootVnode set to nullptr", VGA_GREEN_ON_BLUE);
}

VFS& VFS::get_instance() {
    // kira::kernel::console.add_message("[VFS] get_instance called", VGA_MAGENTA_ON_BLUE);
    
    if (!s_instance) {
        // kira::kernel::console.add_message("[VFS] Using static VFS instance", VGA_GREEN_ON_BLUE);
        
        // Use static allocation instead of dynamic - this avoids memory mapping issues
        s_instance = &s_static_vfs_instance;
        
        // Print the static instance address
        char ptrBuffer[32];
        kira::utils::number_to_hex(ptrBuffer, reinterpret_cast<u32>(s_instance));
        kira::kernel::console.add_message("[VFS] Static VFS address:", VGA_GREEN_ON_BLUE);
        kira::kernel::console.add_message(ptrBuffer, VGA_YELLOW_ON_BLUE);
        
        // kira::kernel::console.add_message("[VFS] Static VFS instance ready", VGA_GREEN_ON_BLUE);
    } else {
        // kira::kernel::console.add_message("[VFS] Using existing singleton", VGA_CYAN_ON_BLUE);
    }
    
    // Print the s_instance pointer address for comparison
    // char currentBuffer[32];
    // kira::utils::number_to_hex(currentBuffer, reinterpret_cast<u32>(s_instance));
    // kira::kernel::console.add_message("[VFS] Current s_instance:", VGA_GREEN_ON_BLUE);
    // kira::kernel::console.add_message(currentBuffer, VGA_YELLOW_ON_BLUE);
    
    return *s_instance;
}

FSResult VFS::initialize() {
    // Initialize file descriptor table
    for (u32 i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        m_fileDescriptors[i] = nullptr;
    }
    
    // Initialize file system registry
    for (u32 i = 0; i < MAX_FILESYSTEMS; i++) {
        m_filesystems[i] = nullptr;
    }
    m_filesystemCount = 0;
    
    m_rootVnode = nullptr;
    
    return FSResult::SUCCESS;
}

FSResult VFS::register_filesystem(FileSystem* fs) {
    if (!fs) {
        return FSResult::INVALID_PARAMETER;
    }
    
    if (m_filesystemCount >= MAX_FILESYSTEMS) {
        return FSResult::NO_SPACE;
    }
    
    m_filesystems[m_filesystemCount] = fs;
    m_filesystemCount++;
    
    return FSResult::SUCCESS;
}

FSResult VFS::mount(const char* device, const char* mountpoint, const char* fstype) {
    if (!device || !mountpoint || !fstype) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Find the requested file system type
    FileSystem* fs = nullptr;
    for (u32 i = 0; i < m_filesystemCount; i++) {
        if (m_filesystems[i] && safe_string_equals(m_filesystems[i]->get_name(), fstype)) {
            fs = m_filesystems[i];
            break;
        }
    }
    
    if (!fs) {
        return FSResult::NOT_FOUND;
    }
    
    // Mount the file system
    FSResult result = fs->mount(device);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    // If this is the root mount, set it as root
    if (mountpoint[0] == '/' && mountpoint[1] == '\0') {
        // kira::kernel::console.add_message("[VFS] Setting up root mount", VGA_CYAN_ON_BLUE);
        result = fs->get_root(m_rootVnode);
        if (result != FSResult::SUCCESS) {
            kira::kernel::console.add_message("[VFS] ERROR: get_root failed", VGA_RED_ON_BLUE);
            fs->unmount();
            return result;
        }
        // kira::kernel::console.add_message("[VFS] Root vnode set successfully", VGA_GREEN_ON_BLUE);
        
        // Debug: Verify m_rootVnode is not null
        if (m_rootVnode) {
            kira::kernel::console.add_message("[VFS] m_rootVnode is valid after setting", VGA_GREEN_ON_BLUE);
        } else {
            kira::kernel::console.add_message("[VFS] ERROR: m_rootVnode is NULL after setting!", VGA_RED_ON_BLUE);
        }
    }
    
    return FSResult::SUCCESS;
}

FSResult VFS::unmount(const char* mountpoint) {
    if (!mountpoint) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // TODO: Implement unmounting for specific mount points
    // For now, just handle root unmount
    if (mountpoint[0] == '/' && mountpoint[1] == '\0' && m_rootVnode) {
        FileSystem* fs = m_rootVnode->get_filesystem();
        if (fs) {
            FSResult result = fs->unmount();
            if (result == FSResult::SUCCESS) {
                m_rootVnode = nullptr;
            }
            return result;
        }
    }
    
    return FSResult::NOT_FOUND;
}

FSResult VFS::open(const char* path, OpenFlags flags, i32& fd) {
    if (!path) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Resolve the path to a VNode
    VNode* vnode = nullptr;
    FSResult result = resolve_path(path, vnode);
    
    // Handle file creation if requested
    if (result == FSResult::NOT_FOUND && 
        (static_cast<u32>(flags) & static_cast<u32>(OpenFlags::CREATE))) {
        
        // Extract parent directory and filename
        const char* last_slash = nullptr;
        for (const char* p = path; *p; p++) {
            if (*p == '/') {
                last_slash = p;
            }
        }
        
        VNode* parent_vnode = nullptr;
        if (!last_slash || last_slash == path) {
            // File is in root directory
            parent_vnode = m_rootVnode;
        } else {
            // Get parent directory
            u32 parent_len = last_slash - path;
            char parent_path[256];
            for (u32 i = 0; i < parent_len && i < 255; i++) {
                parent_path[i] = path[i];
            }
            parent_path[parent_len < 256 ? parent_len : 255] = '\0';
            
            result = resolve_path(parent_path, parent_vnode);
            if (result != FSResult::SUCCESS) {
                return result;
            }
        }
        
        if (!parent_vnode || parent_vnode->get_type() != FileType::DIRECTORY) {
            return FSResult::NOT_DIRECTORY;
        }
        
        // Get filename
        const char* filename = last_slash ? last_slash + 1 : path + 1; // Skip leading slash
        
        // Create the file in the parent directory
        result = parent_vnode->create_file(filename, FileType::REGULAR);
        if (result != FSResult::SUCCESS) {
            if (parent_vnode && parent_vnode != m_rootVnode) {
                parent_vnode->release();
            }
            return result;
        }
        
        // Now resolve the newly created file
        result = resolve_path(path, vnode);
        if (result != FSResult::SUCCESS) {
            if (parent_vnode && parent_vnode != m_rootVnode) {
                parent_vnode->release();
            }
            return result;
        }
        if (parent_vnode && parent_vnode != m_rootVnode) {
            parent_vnode->release();
        }
    }
    
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    // Allocate file descriptor
    i32 new_fd = allocate_fd();
    if (new_fd < 0) {
        return FSResult::TOO_MANY_FILES;
    }
    
    // Create file descriptor
    auto& memMgr = MemoryManager::get_instance();
    void* memory = memMgr.allocate_physical_page();
    if (!memory) {
        free_fd(new_fd);
        if (vnode) vnode->release();
        return FSResult::NO_SPACE;
    }
    
    m_fileDescriptors[new_fd] = new(memory) FileDescriptor(vnode, flags);
    // FileDescriptor retains its own reference; drop our resolve_path reference
    if (vnode) vnode->release();
    fd = new_fd;
    
    return FSResult::SUCCESS;
}

FSResult VFS::close(i32 fd) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || !m_fileDescriptors[fd]) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Clean up file descriptor
    FileDescriptor* file_desc = m_fileDescriptors[fd];
    auto& memMgr = MemoryManager::get_instance();
    
    file_desc->~FileDescriptor();
    memMgr.free_physical_page(file_desc);
    
    free_fd(fd);
    
    return FSResult::SUCCESS;
}

FSResult VFS::read(i32 fd, u32 size, void* buffer) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || !m_fileDescriptors[fd]) {
        return FSResult::INVALID_PARAMETER;
    }
    
    return m_fileDescriptors[fd]->read(size, buffer);
}

FSResult VFS::write(i32 fd, u32 size, const void* buffer) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || !m_fileDescriptors[fd]) {
        return FSResult::INVALID_PARAMETER;
    }
    
    return m_fileDescriptors[fd]->write(size, buffer);
}

FSResult VFS::seek(i32 fd, u32 position) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || !m_fileDescriptors[fd]) {
        return FSResult::INVALID_PARAMETER;
    }
    
    return m_fileDescriptors[fd]->seek(position);
}

FSResult VFS::stat(const char* path, FileStat& stat) {
    if (!path) {
        return FSResult::INVALID_PARAMETER;
    }
    
    VNode* vnode = nullptr;
    FSResult result = resolve_path(path, vnode);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    FSResult r = vnode->get_stat(stat);
    vnode->release();
    return r;
}

FSResult VFS::mkdir(const char* path) {
    if (!path) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // TODO: Implement directory creation
    return FSResult::NOT_FOUND;
}

FSResult VFS::rmdir(const char* path) {
    if (!path) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // TODO: Implement directory removal
    return FSResult::NOT_FOUND;
}

FSResult VFS::readdir(const char* path, u32 index, DirectoryEntry& entry) {

    // Debug: Check m_rootVnode specifically
    if (!m_rootVnode) {
        // kira::kernel::console.add_message("[VFS] ERROR: m_rootVnode is NULL!", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    
    // Simple validation
    if (!path) {
        // kira::kernel::console.add_message("[VFS] ERROR: path is NULL!", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    
    
    VNode* vnode = nullptr;
    FSResult result = resolve_path(path, vnode);
    
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    if (vnode->get_type() != FileType::DIRECTORY) {
        vnode->release();
        return FSResult::NOT_DIRECTORY;
    }
    
    FSResult read_result = vnode->read_dir(index, entry);
    vnode->release();
    return read_result;
}

bool VFS::is_root_mounted() const {
    // kira::kernel::console.add_message("[VFS] Checking if root is mounted", VGA_CYAN_ON_BLUE);
    
    // With static allocation, this should work now
    bool result = (m_rootVnode != nullptr);
    
    return result;
}

FSResult VFS::resolve_path(const char* path, VNode*& vnode) {
    // kira::kernel::console.add_message("[VFS] resolve_path start", VGA_CYAN_ON_BLUE);
    
    if (!path || !m_rootVnode) {
        // kira::kernel::console.add_message("[VFS] invalid params", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    
    // kira::kernel::console.add_message("[VFS] checking root path", VGA_CYAN_ON_BLUE);
    // Handle root path - safe comparison without strcmp
    if (path[0] == '/' && path[1] == '\0') {
        // kira::kernel::console.add_message("[VFS] is root path", VGA_CYAN_ON_BLUE);
        vnode = m_rootVnode;
        if (vnode) vnode->retain();
        return FSResult::SUCCESS;
    }
    
    // Start from root
    VNode* current = m_rootVnode;
    if (current) current->retain();
    
    // Skip leading slash
    if (path[0] == '/') {
        path++;
    }
    
    // Parse path components
    char component[256];
    u32 component_len = 0;
    
    while (*path) {
        if (*path == '/') {
            if (component_len > 0) {
                component[component_len] = '\0';
                
                // Look up component in current directory
                VNode* next = nullptr;
                FSResult result = current->lookup(component, next);
                if (result != FSResult::SUCCESS) {
                    if (current) current->release();
                    return result;
                }
                
                // next is returned retained by FS; drop current and advance
                if (current) current->release();
                current = next;
                component_len = 0;
            }
        } else {
            if (component_len < sizeof(component) - 1) {
                component[component_len++] = *path;
            }
        }
        path++;
    }
    
    // Handle final component
    if (component_len > 0) {
        component[component_len] = '\0';
        
        VNode* next = nullptr;
        FSResult result = current->lookup(component, next);
        if (result != FSResult::SUCCESS) {
            if (current) current->release();
            return result;
        }
        
        if (current) current->release();
        current = next;
    }
    
    vnode = current;
    return FSResult::SUCCESS;
}

//=============================================================================
// Private Helper Methods
//=============================================================================

i32 VFS::allocate_fd() {
    for (i32 i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (!m_fileDescriptors[i]) {
            return i;
        }
    }
    return -1;
}

void VFS::free_fd(i32 fd) {
    if (fd >= 0 && fd < MAX_FILE_DESCRIPTORS) {
        m_fileDescriptors[fd] = nullptr;
    }
}

FSResult VFS::parse_path(const char* path, char* components[], u32& count) {
    // TODO: Implement path parsing helper if needed
    count = 0;
    return FSResult::SUCCESS;
}

} // namespace kira::fs 