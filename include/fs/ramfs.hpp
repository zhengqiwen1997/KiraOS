#pragma once

#include "fs/vfs.hpp"
#include "core/utils.hpp"

namespace kira::fs {

using namespace kira::system;

/**
 * @brief RAM-based File System Node
 * Simple in-memory file system for testing VFS infrastructure
 */
class RamFSNode : public VNode {
public:
    RamFSNode(u32 inode, FileType type, FileSystem* fs, const char* name);
    virtual ~RamFSNode();

    // File operations
    FSResult read(u32 offset, u32 size, void* buffer) override;
    FSResult write(u32 offset, u32 size, const void* buffer) override;
    FSResult get_size(u32& size) override;
    FSResult get_stat(FileStat& stat) override;
    
    // Directory operations
    FSResult read_dir(u32 index, DirectoryEntry& entry) override;
    FSResult create_file(const char* name, FileType type) override;
    FSResult delete_file(const char* name) override;
    FSResult lookup(const char* name, VNode*& result) override;

    // RamFS specific methods
    const char* get_name() const { return m_name; }
    void set_parent(RamFSNode* parent) { m_parent = parent; }
    RamFSNode* get_parent() const { return m_parent; }
    void set_create_time(u32 time) { m_createTime = time; }

private:
    char m_name[256];                    // File/directory name
    RamFSNode* m_parent;                 // Parent directory
    
    // File data
    u8* m_data;                         // File content buffer
    u32 m_size;                         // Current file size
    u32 m_capacity;                     // Buffer capacity
    
    // Directory children
    static constexpr u32 MAX_CHILDREN = 64;
    RamFSNode* m_children[MAX_CHILDREN];
    u32 m_childCount;
    
    // Timestamps
    u32 m_createTime;
    u32 m_modifyTime;
    u32 m_accessTime;
    
    // Helper methods
    FSResult resize_buffer(u32 new_size);
    FSResult add_child(RamFSNode* child);
    FSResult remove_child(const char* name);
    RamFSNode* find_child(const char* name);
};

/**
 * @brief RAM-based File System
 * Simple in-memory file system implementation for testing
 */
class RamFS : public FileSystem {
public:
    RamFS();
    virtual ~RamFS();

    // FileSystem interface
    FSResult mount(const char* device) override;
    FSResult unmount() override;
    FSResult get_root(VNode*& root) override;
    FSResult create_vnode(u32 inode, FileType type, VNode*& vnode) override;
    FSResult sync() override;

    const char* get_name() const override { return "ramfs"; }
    bool is_mounted() const override { return m_mounted; }

    // RamFS specific methods
    FSResult create_file(const char* path, FileType type);
    FSResult delete_file(const char* path);
    u32 allocate_inode() { return m_nextInode++; }

private:
    RamFSNode* m_root;
    u32 m_nextInode;
    
    // Helper methods
    FSResult resolve_parent_path(const char* path, RamFSNode*& parent, char* filename);
};

} // namespace kira::fs 