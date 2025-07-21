#pragma once

#include "core/types.hpp"

namespace kira::fs {

using namespace kira::system;

// Forward declarations
class FileSystem;
class VNode;
class FileDescriptor;

// File types
enum class FileType : u8 {
    REGULAR = 0,
    DIRECTORY = 1,
    DEVICE = 2,
    SYMLINK = 3
};

// File access modes
enum class AccessMode : u32 {
    READ = 0x01,
    WRITE = 0x02,
    EXECUTE = 0x04,
    READ_WRITE = READ | WRITE
};

// File open flags
enum class OpenFlags : u32 {
    READ_ONLY = 0x00,
    WRITE_ONLY = 0x01,
    READ_WRITE = 0x02,
    CREATE = 0x40,
    TRUNCATE = 0x200,
    APPEND = 0x400
};

// File system operation results
enum class FSResult : i32 {
    SUCCESS = 0,
    NOT_FOUND = -1,
    PERMISSION_DENIED = -2,
    INVALID_PARAMETER = -3,
    NO_SPACE = -4,
    EXISTS = -5,
    NOT_DIRECTORY = -6,
    IS_DIRECTORY = -7,
    TOO_MANY_FILES = -8,
    IO_ERROR = -9
};

// File statistics structure
struct FileStat {
    u32 size;           // File size in bytes
    FileType type;      // File type
    u32 mode;          // Access permissions
    u32 uid;           // User ID
    u32 gid;           // Group ID
    u32 atime;         // Access time
    u32 mtime;         // Modification time
    u32 ctime;         // Creation time
} __attribute__((packed));

// Directory entry structure
struct DirectoryEntry {
    char name[256];     // File name
    u32 inode;         // Inode number
    FileType type;     // File type
} __attribute__((packed));

/**
 * @brief Virtual Node - represents a file or directory in the VFS
 */
class VNode {
public:
    VNode(u32 inode, FileType type, FileSystem* fs);
    virtual ~VNode() = default;

    // File operations
    virtual FSResult read(u32 offset, u32 size, void* buffer) = 0;
    virtual FSResult write(u32 offset, u32 size, const void* buffer) = 0;
    virtual FSResult get_size(u32& size) = 0;
    virtual FSResult get_stat(FileStat& stat) = 0;
    
    // Directory operations
    virtual FSResult read_dir(u32 index, DirectoryEntry& entry) = 0;
    virtual FSResult create_file(const char* name, FileType type) = 0;
    virtual FSResult delete_file(const char* name) = 0;
    virtual FSResult lookup(const char* name, VNode*& result) = 0;

    // Getters
    u32 get_inode() const { return m_inode; }
    FileType get_type() const { return m_type; }
    FileSystem* get_filesystem() const { return m_filesystem; }

protected:
    u32 m_inode;
    FileType m_type;
    FileSystem* m_filesystem;
};

/**
 * @brief File Descriptor - represents an open file
 */
class FileDescriptor {
public:
    FileDescriptor(VNode* vnode, OpenFlags flags);
    ~FileDescriptor();

    FSResult read(u32 size, void* buffer);
    FSResult write(u32 size, const void* buffer);
    FSResult seek(u32 position);
    FSResult get_position(u32& position) const;
    FSResult get_size(u32& size) const;

    VNode* get_vnode() const { return m_vnode; }
    OpenFlags get_flags() const { return m_flags; }

private:
    VNode* m_vnode;
    OpenFlags m_flags;
    u32 m_position;
};

/**
 * @brief Abstract File System interface
 */
class FileSystem {
public:
    virtual ~FileSystem() = default;

    // File system operations
    virtual FSResult mount(const char* device) = 0;
    virtual FSResult unmount() = 0;
    virtual FSResult get_root(VNode*& root) = 0;
    
    // File operations
    virtual FSResult create_vnode(u32 inode, FileType type, VNode*& vnode) = 0;
    virtual FSResult sync() = 0;

    // Information
    virtual const char* get_name() const = 0;
    virtual bool is_mounted() const = 0;

protected:
    bool m_mounted = false;
};

/**
 * @brief Virtual File System Manager
 * Central coordinator for all file system operations
 */
class VFS {
public:
    static VFS& get_instance();
    
    // Initialization
    FSResult initialize();
    
    // File system management
    FSResult register_filesystem(FileSystem* fs);
    FSResult mount(const char* device, const char* mountpoint, const char* fstype);
    FSResult unmount(const char* mountpoint);
    
    // File operations
    FSResult open(const char* path, OpenFlags flags, i32& fd);
    FSResult close(i32 fd);
    FSResult read(i32 fd, u32 size, void* buffer);
    FSResult write(i32 fd, u32 size, const void* buffer);
    FSResult seek(i32 fd, u32 position);
    FSResult stat(const char* path, FileStat& stat);
    
    // Directory operations
    FSResult mkdir(const char* path);
    FSResult rmdir(const char* path);
    FSResult readdir(const char* path, u32 index, DirectoryEntry& entry);
    
    // Path resolution
    FSResult resolve_path(const char* path, VNode*& vnode);

private:
    VFS() = default;
    static VFS* s_instance;
    
    // File descriptor management
    static constexpr u32 MAX_FILE_DESCRIPTORS = 256;
    FileDescriptor* m_file_descriptors[MAX_FILE_DESCRIPTORS];
    
    // File system registry
    static constexpr u32 MAX_FILESYSTEMS = 16;
    FileSystem* m_filesystems[MAX_FILESYSTEMS];
    u32 m_filesystem_count;
    
    // Mount points
    VNode* m_root_vnode;
    
    // Helper methods
    i32 allocate_fd();
    void free_fd(i32 fd);
    FSResult parse_path(const char* path, char* components[], u32& count);
};

} // namespace kira::fs 