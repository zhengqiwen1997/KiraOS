#include "fs/ramfs.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"
#include "display/console.hpp"

// External console reference from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::fs {

using namespace kira::system;
using namespace kira::utils;
using namespace kira::display;

// TEMPORARY: Static memory pools disabled to debug kernel crash
// Static memory pool to avoid dynamic allocation memory mapping issues
//static constexpr u32 MAX_STATIC_NODES = 4;
//static constexpr u32 NODE_SIZE = sizeof(RamFSNode);
//static u8 s_static_memory_pool[MAX_STATIC_NODES * NODE_SIZE];
//static bool s_node_pool_used[MAX_STATIC_NODES] = {false};
//static u32 s_next_node_index = 0;

// TEMPORARY: Static node allocation disabled - fallback to dynamic
static RamFSNode* allocate_static_node(u32 inode, FileType type, FileSystem* fs, const char* name) {
    // DISABLED: Return nullptr to force dynamic allocation fallback
    return nullptr;
}

static void free_static_node(RamFSNode* node) {
    // DISABLED: Static allocation disabled
    return;
}

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
                // Use static deallocation instead of physical page free
                free_static_node(m_children[i]);
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
    kira::kernel::console.add_message("[RAMFS] read_dir start", VGA_GREEN_ON_BLUE);
    
    if (m_type != FileType::DIRECTORY) {
        kira::kernel::console.add_message("[RAMFS] not directory", VGA_RED_ON_BLUE);
        return FSResult::NOT_DIRECTORY;
    }
    
    kira::kernel::console.add_message("[RAMFS] checking child count", VGA_GREEN_ON_BLUE);
    
    // Debug: show child count
    char debugMsg[64];
    kira::utils::number_to_decimal(debugMsg, m_childCount);
    kira::kernel::console.add_message(debugMsg, VGA_WHITE_ON_BLUE);
    
    if (index >= m_childCount) {
        kira::kernel::console.add_message("[RAMFS] index out of range", VGA_RED_ON_BLUE);
        return FSResult::NOT_FOUND; // End of directory
    }
    
    kira::kernel::console.add_message("[RAMFS] getting child", VGA_GREEN_ON_BLUE);
    RamFSNode* child = m_children[index];
    if (!child) {
        kira::kernel::console.add_message("[RAMFS] child is null", VGA_RED_ON_BLUE);
        return FSResult::NOT_FOUND;
    }
    
    kira::kernel::console.add_message("[RAMFS] getting child name", VGA_GREEN_ON_BLUE);
    const char* child_name = child->get_name();
    if (!child_name) {
        kira::kernel::console.add_message("[RAMFS] child name null", VGA_RED_ON_BLUE);
        return FSResult::NOT_FOUND;
    }
    
    kira::kernel::console.add_message("[RAMFS] copying name", VGA_GREEN_ON_BLUE);
    // Use safer manual string copy instead of strcpy_s
    u32 i = 0;
    while (i < sizeof(entry.name) - 1 && child_name[i] != '\0') {
        entry.name[i] = child_name[i];
        i++;
    }
    entry.name[i] = '\0';
    
    kira::kernel::console.add_message("[RAMFS] setting inode/type", VGA_GREEN_ON_BLUE);
    entry.inode = child->get_inode();
    entry.type = child->get_type();
    
    kira::kernel::console.add_message("[RAMFS] read_dir success", VGA_GREEN_ON_BLUE);
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
    // Get next inode from filesystem
    RamFS* ramfs = static_cast<RamFS*>(m_filesystem);
    u32 inode = ramfs->allocate_inode();
    
    // Use static allocation instead of dynamic
    RamFSNode* new_node = allocate_static_node(inode, type, m_filesystem, name);
    if (!new_node) {
        return FSResult::NO_SPACE;
    }
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
            // Use static deallocation instead of physical page free
            free_static_node(m_children[i]);
            
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
    kira::kernel::console.add_message("[RAMFS] mount called", VGA_YELLOW_ON_BLUE);
    
    if (m_mounted) {
        kira::kernel::console.add_message("[RAMFS] already mounted", VGA_YELLOW_ON_BLUE);
        return FSResult::EXISTS;
    }
    
    kira::kernel::console.add_message("[RAMFS] creating root directory", VGA_YELLOW_ON_BLUE);
    // Create root directory using static allocation
    kira::kernel::console.add_message("[RAMFS] allocating root node from static pool", VGA_YELLOW_ON_BLUE);
    m_root = allocate_static_node(allocate_inode(), FileType::DIRECTORY, this, "/");
    if (!m_root) {
        kira::kernel::console.add_message("[RAMFS] ERROR: static node pool exhausted", VGA_RED_ON_BLUE);
        return FSResult::NO_SPACE;
    }
    m_root->set_create_time(1);
    
    kira::kernel::console.add_message("[RAMFS] setting mounted flag", VGA_YELLOW_ON_BLUE);
    m_mounted = true;
    kira::kernel::console.add_message("[RAMFS] mount completed successfully", VGA_GREEN_ON_BLUE);
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
    kira::kernel::console.add_message("[RAMFS] get_root called", VGA_CYAN_ON_BLUE);
    
    if (!m_mounted) {
        kira::kernel::console.add_message("[RAMFS] ERROR: not mounted", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    
    if (!m_root) {
        kira::kernel::console.add_message("[RAMFS] ERROR: m_root is NULL", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    
    kira::kernel::console.add_message("[RAMFS] returning root vnode", VGA_GREEN_ON_BLUE);
    root = m_root;
    return FSResult::SUCCESS;
}

FSResult RamFS::create_vnode(u32 inode, FileType type, VNode*& vnode) {
    // Use static allocation instead of dynamic
    RamFSNode* node = allocate_static_node(inode, type, this, "");
    if (!node) {
        return FSResult::NO_SPACE;
    }
    
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
        strcpy_s(filename, path + 1, 256); // Skip leading slash
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
        strcpy_s(filename, last_slash + 1, 256);
    }
    
    return FSResult::SUCCESS;
}

} // namespace kira::fs 