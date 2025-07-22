#include "fs/vfs.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"

namespace kira::fs {

using namespace kira::system;
using namespace kira::utils;

// Static member definitions
VFS* VFS::s_instance = nullptr;

//=============================================================================
// VNode Implementation
//=============================================================================

VNode::VNode(u32 inode, FileType type, FileSystem* fs)
    : m_inode(inode), m_type(type), m_filesystem(fs) {
}

//=============================================================================
// FileDescriptor Implementation
//=============================================================================

FileDescriptor::FileDescriptor(VNode* vnode, OpenFlags flags)
    : m_vnode(vnode), m_flags(flags), m_position(0) {
}

FileDescriptor::~FileDescriptor() {
    // VNode cleanup is handled by the file system
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

VFS& VFS::get_instance() {
    if (!s_instance) {
        // Allocate VFS instance using placement new
        auto& memMgr = MemoryManager::get_instance();
        void* memory = memMgr.allocate_physical_page();
        if (memory) {
            s_instance = new(memory) VFS();
        }
    }
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
        if (m_filesystems[i] && strcmp(m_filesystems[i]->get_name(), fstype) == 0) {
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
    if (strcmp(mountpoint, "/") == 0) {
        result = fs->get_root(m_rootVnode);
        if (result != FSResult::SUCCESS) {
            fs->unmount();
            return result;
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
    if (strcmp(mountpoint, "/") == 0 && m_rootVnode) {
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
            return result;
        }
        
        // Now resolve the newly created file
        result = resolve_path(path, vnode);
        if (result != FSResult::SUCCESS) {
            return result;
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
        return FSResult::NO_SPACE;
    }
    
    m_fileDescriptors[new_fd] = new(memory) FileDescriptor(vnode, flags);
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
    
    return vnode->get_stat(stat);
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
    if (!path) {
        return FSResult::INVALID_PARAMETER;
    }
    
    VNode* vnode = nullptr;
    FSResult result = resolve_path(path, vnode);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    if (vnode->get_type() != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    return vnode->read_dir(index, entry);
}

FSResult VFS::resolve_path(const char* path, VNode*& vnode) {
    if (!path || !m_rootVnode) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Handle root path
    if (strcmp(path, "/") == 0) {
        vnode = m_rootVnode;
        return FSResult::SUCCESS;
    }
    
    // Start from root
    VNode* current = m_rootVnode;
    
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
                    return result;
                }
                
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
            return result;
        }
        
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