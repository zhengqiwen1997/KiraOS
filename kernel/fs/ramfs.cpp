#include "fs/ramfs.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"

namespace kira::fs {

using namespace kira::system;
using namespace kira::utils;

//=============================================================================
// RamFSNode Implementation
//=============================================================================

RamFSNode::RamFSNode(u32 inode, FileType type, FileSystem* fs, const char* name)
    : VNode(inode, type, fs), m_parent(nullptr), m_data(nullptr), 
      m_size(0), m_capacity(0), m_childCount(0),
      m_createTime(0), m_modifyTime(0), m_accessTime(0) {
    
    // Copy name
    if (name) {
        strcpy_s(m_name, name, sizeof(m_name));
    } else {
        m_name[0] = '\0';
    }
    
    // Initialize children array
    for (u32 i = 0; i < MAX_CHILDREN; i++) {
        m_children[i] = nullptr;
    }
}

RamFSNode::~RamFSNode() {
    // Free file data buffer
    if (m_data) {
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(m_data);
    }
    
    // Clean up children (directories only)
    if (m_type == FileType::DIRECTORY) {
        for (u32 i = 0; i < m_childCount; i++) {
            if (m_children[i]) {
                // Manual cleanup instead of delete
                m_children[i]->~RamFSNode();
                auto& memMgr = MemoryManager::get_instance();
                memMgr.free_physical_page(m_children[i]);
            }
        }
    }
}

FSResult RamFSNode::read(u32 offset, u32 size, void* buffer) {
    if (!buffer || size == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    if (m_type != FileType::REGULAR) {
        return FSResult::IS_DIRECTORY;
    }
    
    if (offset >= m_size) {
        return FSResult::SUCCESS; // EOF
    }
    
    u32 bytes_to_read = size;
    if (offset + size > m_size) {
        bytes_to_read = m_size - offset;
    }
    
    if (bytes_to_read > 0 && m_data) {
        memcpy(buffer, m_data + offset, bytes_to_read);
    }
    
    m_accessTime++; // Simple timestamp increment
    return FSResult::SUCCESS;
}

FSResult RamFSNode::write(u32 offset, u32 size, const void* buffer) {
    if (!buffer || size == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    if (m_type != FileType::REGULAR) {
        return FSResult::IS_DIRECTORY;
    }
    
    u32 required_size = offset + size;
    
    // Resize buffer if needed
    if (required_size > m_capacity) {
        FSResult result = resize_buffer(required_size);
        if (result != FSResult::SUCCESS) {
            return result;
        }
    }
    
    // Write data
    memcpy(m_data + offset, buffer, size);
    
    // Update size if we extended the file
    if (required_size > m_size) {
        m_size = required_size;
    }
    
    m_modifyTime++; // Simple timestamp increment
    return FSResult::SUCCESS;
}

FSResult RamFSNode::get_size(u32& size) {
    size = m_size;
    return FSResult::SUCCESS;
}

FSResult RamFSNode::get_stat(FileStat& stat) {
    stat.size = m_size;
    stat.type = m_type;
    stat.mode = 0644; // Default permissions
    stat.uid = 0;
    stat.gid = 0;
    stat.atime = m_accessTime;
    stat.mtime = m_modifyTime;
    stat.ctime = m_createTime;
    
    return FSResult::SUCCESS;
}

FSResult RamFSNode::read_dir(u32 index, DirectoryEntry& entry) {
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    if (index >= m_childCount) {
        return FSResult::NOT_FOUND; // End of directory
    }
    
    RamFSNode* child = m_children[index];
    if (!child) {
        return FSResult::NOT_FOUND;
    }
    
    strcpy_s(entry.name, child->get_name(), sizeof(entry.name));
    entry.inode = child->get_inode();
    entry.type = child->get_type();
    
    return FSResult::SUCCESS;
}

FSResult RamFSNode::create_file(const char* name, FileType type) {
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    if (!name || strlen(name) == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Check if file already exists
    if (find_child(name)) {
        return FSResult::EXISTS;
    }
    
    // Check if we have space for another child
    if (m_childCount >= MAX_CHILDREN) {
        return FSResult::NO_SPACE;
    }
    
    // Create new node
    auto& memMgr = MemoryManager::get_instance();
    void* memory = memMgr.allocate_physical_page();
    if (!memory) {
        return FSResult::NO_SPACE;
    }
    
    // Get next inode from filesystem
    RamFS* ramfs = static_cast<RamFS*>(m_filesystem);
    u32 inode = ramfs->allocate_inode();
    
    RamFSNode* new_node = new(memory) RamFSNode(inode, type, m_filesystem, name);
    new_node->set_parent(this);
    new_node->set_create_time(m_modifyTime + 1);
    
    return add_child(new_node);
}

FSResult RamFSNode::delete_file(const char* name) {
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    return remove_child(name);
}

FSResult RamFSNode::lookup(const char* name, VNode*& result) {
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    RamFSNode* child = find_child(name);
    if (!child) {
        return FSResult::NOT_FOUND;
    }
    
    result = child;
    return FSResult::SUCCESS;
}

FSResult RamFSNode::resize_buffer(u32 new_size) {
    if (new_size <= m_capacity) {
        return FSResult::SUCCESS;
    }
    
    // Round up to page boundaries for efficiency
    u32 pages_needed = (new_size + 4095) / 4096;
    u32 new_capacity = pages_needed * 4096;
    
    auto& memMgr = MemoryManager::get_instance();
    
    // Allocate new buffer
    u8* new_buffer = nullptr;
    for (u32 i = 0; i < pages_needed; i++) {
        void* page = memMgr.allocate_physical_page();
        if (!page) {
            // Free any pages we already allocated
            if (new_buffer) {
                for (u32 j = 0; j < i; j++) {
                    memMgr.free_physical_page(new_buffer + (j * 4096));
                }
            }
            return FSResult::NO_SPACE;
        }
        
        if (i == 0) {
            new_buffer = static_cast<u8*>(page);
        }
    }
    
    // Copy existing data
    if (m_data && m_size > 0) {
        memcpy(new_buffer, m_data, m_size);
    }
    
    // Free old buffer
    if (m_data) {
        u32 old_pages = (m_capacity + 4095) / 4096;
        for (u32 i = 0; i < old_pages; i++) {
            memMgr.free_physical_page(m_data + (i * 4096));
        }
    }
    
    m_data = new_buffer;
    m_capacity = new_capacity;
    
    return FSResult::SUCCESS;
}

FSResult RamFSNode::add_child(RamFSNode* child) {
    if (m_childCount >= MAX_CHILDREN) {
        return FSResult::NO_SPACE;
    }
    
    m_children[m_childCount] = child;
    m_childCount++;
    m_modifyTime++;
    
    return FSResult::SUCCESS;
}

FSResult RamFSNode::remove_child(const char* name) {
    for (u32 i = 0; i < m_childCount; i++) {
        if (m_children[i] && strcmp(m_children[i]->get_name(), name) == 0) {
            // Manual cleanup instead of delete
            m_children[i]->~RamFSNode();
            auto& memMgr = MemoryManager::get_instance();
            memMgr.free_physical_page(m_children[i]);
            
            // Shift remaining children
            for (u32 j = i; j < m_childCount - 1; j++) {
                m_children[j] = m_children[j + 1];
            }
            
            m_childCount--;
            m_children[m_childCount] = nullptr;
            m_modifyTime++;
            
            return FSResult::SUCCESS;
        }
    }
    
    return FSResult::NOT_FOUND;
}

RamFSNode* RamFSNode::find_child(const char* name) {
    for (u32 i = 0; i < m_childCount; i++) {
        if (m_children[i] && strcmp(m_children[i]->get_name(), name) == 0) {
            return m_children[i];
        }
    }
    return nullptr;
}

//=============================================================================
// RamFS Implementation
//=============================================================================

RamFS::RamFS() : m_root(nullptr), m_nextInode(1) {
}

RamFS::~RamFS() {
    if (m_root) {
        // Manual cleanup instead of delete
        m_root->~RamFSNode();
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(m_root);
    }
}

FSResult RamFS::mount(const char* device) {
    if (m_mounted) {
        return FSResult::EXISTS;
    }
    
    // Create root directory
    auto& memMgr = MemoryManager::get_instance();
    void* memory = memMgr.allocate_physical_page();
    if (!memory) {
        return FSResult::NO_SPACE;
    }
    
    m_root = new(memory) RamFSNode(allocate_inode(), FileType::DIRECTORY, this, "/");
    m_root->set_create_time(1);
    
    m_mounted = true;
    return FSResult::SUCCESS;
}

FSResult RamFS::unmount() {
    if (!m_mounted) {
        return FSResult::INVALID_PARAMETER;
    }
    
    if (m_root) {
        delete m_root;
        m_root = nullptr;
    }
    
    m_mounted = false;
    return FSResult::SUCCESS;
}

FSResult RamFS::get_root(VNode*& root) {
    if (!m_mounted || !m_root) {
        return FSResult::INVALID_PARAMETER;
    }
    
    root = m_root;
    return FSResult::SUCCESS;
}

FSResult RamFS::create_vnode(u32 inode, FileType type, VNode*& vnode) {
    auto& memMgr = MemoryManager::get_instance();
    void* memory = memMgr.allocate_physical_page();
    if (!memory) {
        return FSResult::NO_SPACE;
    }
    
    RamFSNode* node = new(memory) RamFSNode(inode, type, this, "");
    vnode = node;
    
    return FSResult::SUCCESS;
}

FSResult RamFS::sync() {
    // RAM file system doesn't need syncing
    return FSResult::SUCCESS;
}

FSResult RamFS::create_file(const char* path, FileType type) {
    if (!path || !m_root) {
        return FSResult::INVALID_PARAMETER;
    }
    
    RamFSNode* parent;
    char filename[256];
    
    FSResult result = resolve_parent_path(path, parent, filename);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    return parent->create_file(filename, type);
}

FSResult RamFS::delete_file(const char* path) {
    if (!path || !m_root) {
        return FSResult::INVALID_PARAMETER;
    }
    
    RamFSNode* parent;
    char filename[256];
    
    FSResult result = resolve_parent_path(path, parent, filename);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    return parent->delete_file(filename);
}

FSResult RamFS::resolve_parent_path(const char* path, RamFSNode*& parent, char* filename) {
    if (!path || path[0] != '/') {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Find last slash
    const char* last_slash = nullptr;
    for (const char* p = path; *p; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }
    
    if (!last_slash || last_slash == path) {
        // File is in root directory
        parent = m_root;
        strcpy(filename, path + 1); // Skip leading slash
    } else {
        // Copy parent path
        u32 parent_len = last_slash - path;
        char parent_path[256];
        for (u32 i = 0; i < parent_len && i < 255; i++) {
            parent_path[i] = path[i];
        }
        parent_path[parent_len < 256 ? parent_len : 255] = '\0';
        
        // Resolve parent directory
        VNode* parent_vnode;
        VFS& vfs = VFS::get_instance();
        FSResult result = vfs.resolve_path(parent_path, parent_vnode);
        if (result != FSResult::SUCCESS) {
            return result;
        }
        
        parent = static_cast<RamFSNode*>(parent_vnode);
        strcpy(filename, last_slash + 1);
    }
    
    return FSResult::SUCCESS;
}

} // namespace kira::fs 