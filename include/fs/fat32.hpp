#pragma once

#include "fs/vfs.hpp"
#include "fs/block_device.hpp"
#include "core/types.hpp"

namespace kira::fs {

using namespace kira::system;

// FAT32 filesystem structures (little-endian)
struct Fat32Bpb {
    u8  jump_boot[3];           // Jump instruction
    u8  oem_name[8];            // OEM name
    u16 bytes_per_sector;       // Bytes per sector (usually 512)
    u8  sectors_per_cluster;    // Sectors per cluster
    u16 reserved_sector_count;  // Reserved sectors
    u8  num_fats;               // Number of FATs (usually 2)
    u16 root_entry_count;       // Root directory entries (0 for FAT32)
    u16 total_sectors_16;       // Total sectors (0 for FAT32)
    u8  media;                  // Media descriptor
    u16 fat_size_16;            // FAT size in sectors (0 for FAT32)
    u16 sectors_per_track;      // Sectors per track
    u16 num_heads;              // Number of heads
    u32 hidden_sectors;         // Hidden sectors
    u32 total_sectors_32;       // Total sectors (FAT32)
    u32 fat_size_32;            // FAT size in sectors (FAT32)
    u16 ext_flags;              // Extended flags
    u16 fs_version;             // Filesystem version
    u32 root_cluster;           // Root directory cluster
    u16 fs_info;                // FSInfo sector
    u16 backup_boot_sector;     // Backup boot sector
    u8  reserved[12];           // Reserved
    u8  drive_number;           // Drive number
    u8  reserved1;              // Reserved
    u8  boot_signature;         // Boot signature (0x29)
    u32 volume_id;              // Volume ID
    u8  volume_label[11];       // Volume label
    u8  fs_type[8];             // Filesystem type
} __attribute__((packed));

struct Fat32DirEntry {
    u8  name[11];               // 8.3 filename
    u8  attr;                   // File attributes
    u8  nt_reserved;            // Reserved for Windows NT
    u8  create_time_tenth;      // Creation time (tenths of second)
    u16 create_time;            // Creation time
    u16 create_date;            // Creation date
    u16 last_access_date;       // Last access date
    u16 first_cluster_high;     // High word of first cluster
    u16 write_time;             // Write time
    u16 write_date;             // Write date
    u16 first_cluster_low;      // Low word of first cluster
    u32 file_size;              // File size
} __attribute__((packed));

// FAT32 file attributes
namespace Fat32Attr {
    constexpr u8 READ_ONLY = 0x01;
    constexpr u8 HIDDEN = 0x02;
    constexpr u8 SYSTEM = 0x04;
    constexpr u8 VOLUME_ID = 0x08;
    constexpr u8 DIRECTORY = 0x10;
    constexpr u8 ARCHIVE = 0x20;
    constexpr u8 LONG_NAME = 0x0F;  // Long filename entry
}

// FAT32 cluster values
namespace Fat32Cluster {
    constexpr u32 FREE = 0x00000000;
    constexpr u32 RESERVED_MIN = 0x0FFFFFF0;
    constexpr u32 RESERVED_MAX = 0x0FFFFFF6;
    constexpr u32 BAD = 0x0FFFFFF7;
    constexpr u32 END_MIN = 0x0FFFFFF8;
    constexpr u32 END_MAX = 0x0FFFFFFF;
    constexpr u32 MASK = 0x0FFFFFFF;  // 28-bit cluster numbers
}

/**
 * @brief FAT32 VNode Implementation
 */
class FAT32Node : public VNode {
public:
    FAT32Node(u32 inode, FileType type, FileSystem* fs, u32 firstCluster, u32 size = 0);
    virtual ~FAT32Node() = default;

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

    // FAT32 specific methods
    u32 get_first_cluster() const { return m_firstCluster; }
    void set_first_cluster(u32 cluster) { m_firstCluster = cluster; }
    void set_size(u32 size) { m_size = size; }

private:
    u32 m_firstCluster;        // First cluster of file/directory
    u32 m_size;                 // File size (directories: 0)
    
    // Cached directory entries for directories
    Fat32DirEntry* m_dirEntries;
    u32 m_dirEntryCount;
    bool m_dirCacheValid;
    
    FSResult load_directory_cache();
    void invalidate_directory_cache();
    FSResult convert_fat_entry_to_vfs(const Fat32DirEntry& fatEntry, DirectoryEntry& vfsEntry);
};

/**
 * @brief FAT32 File System Implementation
 */
class FAT32 : public FileSystem {
public:
    explicit FAT32(BlockDevice* device);
    virtual ~FAT32();

    // FileSystem interface
    FSResult mount(const char* device) override;
    FSResult unmount() override;
    FSResult get_root(VNode*& root) override;
    FSResult create_vnode(u32 inode, FileType type, VNode*& vnode) override;
    FSResult sync() override;

    const char* get_name() const override { return "fat32"; }
    bool is_mounted() const override { return m_mounted; }

    // FAT32 specific operations
    FSResult read_cluster(u32 cluster, void* buffer);
    FSResult write_cluster(u32 cluster, const void* buffer);
    u32 get_next_cluster(u32 cluster);
    FSResult set_next_cluster(u32 cluster, u32 nextCluster);
    u32 allocate_cluster();
    FSResult free_cluster(u32 cluster);
    u32 get_m_fatCacheSector() const { return m_fatCacheSector; }
    u32 get_m_fatStartSector() const { return m_fatStartSector; }
    u32 get_m_dataStartSector() const { return m_dataStartSector; }
    u32 get_m_bytesPerCluster() const { return m_bytesPerCluster; }
    u32 get_m_sectorsPerCluster() const { return m_sectorsPerCluster; }
    u32 get_m_nextInode() const { return m_nextInode; }
 
private:
    BlockDevice* m_device;
    FAT32Node* m_root;
    bool m_mounted;
    Fat32Bpb m_bpb;
    u32 m_fatStartSector;
    u32 m_dataStartSector;
    u32 m_sectorsPerCluster;
    u32 m_bytesPerCluster;
    u32 m_nextInode;

    // FAT cache (simple cache for performance)
    u32* m_fatCache;
    u32 m_fatCacheSector;
    bool m_fatCacheDirty;
    
    FSResult load_fat_sector(u32 sector);
    FSResult flush_fat_cache();
    u32 cluster_to_sector(u32 cluster);
    FSResult parse_bpb();
    
    // Cluster allocation and management
    FSResult extend_cluster_chain(u32 lastCluster, u32& newCluster);

public:
    u32 allocate_inode() { return m_nextInode++; }
    
    // Static allocation factory to avoid memory mapping issues
    static FAT32* create_static_instance(BlockDevice* blockDevice);
    static void destroy_static_instance(FAT32* fs);
    
    // FAT32 specific operations
    FSResult read_file_data(u32 firstCluster, u32 offset, u32 size, void* buffer);
    FSResult write_file_data(u32 firstCluster, u32 offset, u32 size, const void* buffer);
    FSResult get_next_cluster(u32 cluster, u32& nextCluster);
    u32 get_cluster_chain_size(u32 firstCluster);
    void convert_fat_name(const u8* fatName, char* standardName);
    void convert_standard_name_to_fat(const char* standardName, u8* fatName);
    
    // Directory operations
    FSResult create_file_in_directory(u32 dirCluster, const char* name, FileType type);
    FSResult delete_file_from_directory(u32 dirCluster, const char* name);
    FSResult lookup_in_directory(u32 dirCluster, const char* name, VNode*& result);
    
    // Directory helper functions
    FSResult initialize_directory(u32 dirCluster, u32 parentCluster);
    FSResult delete_directory_contents(u32 dirCluster);
    FSResult free_cluster_chain(u32 firstCluster);
};

} // namespace kira::fs 