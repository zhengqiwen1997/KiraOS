#include "fs/fat32.hpp"
#include "display/vga.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"
#include "display/console.hpp"
#include "test/test_base.hpp"

// External console reference from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::fs {

using namespace kira::system;
using namespace kira::utils;
using namespace kira::display;

// Dynamic FAT32Node allocation backed by MemoryManager pages
static FAT32Node* allocate_dynamic_fat32_node(u32 inode, FileType type, FileSystem* fs, u32 firstCluster, u32 size) {
    auto& memMgr = MemoryManager::get_instance();
    void* memory = memMgr.allocate_physical_page();
    if (!memory) {
        return nullptr;
    }
    return new(memory) FAT32Node(inode, type, fs, firstCluster, size);
}

static void free_dynamic_fat32_node(FAT32Node* node) {
    if (!node) return;
    auto& memMgr = MemoryManager::get_instance();
    node->~FAT32Node();
    memMgr.free_physical_page(node);
}

// SAFE: Static allocation for main FAT32 filesystem (single instance)
static constexpr u32 FAT32_FS_SIZE = sizeof(FAT32);
static u8 s_static_fat32_filesystem[FAT32_FS_SIZE];
static bool s_fat32_fs_allocated = false;

// SAFE: Static FAT32 filesystem allocation with single instance
static FAT32* allocate_static_fat32_filesystem(BlockDevice* blockDevice) {
    k_printf("[FAT32] allocate_static_fat32_filesystem: s_fat32_fs_allocated=%d\n", s_fat32_fs_allocated);
    
    if (s_fat32_fs_allocated) {
        k_printf("[FAT32] Static FAT32 already allocated, returning nullptr\n");
        return nullptr; // Only one FAT32 filesystem supported
    }
    
    s_fat32_fs_allocated = true;
    k_printf("[FAT32] Creating static FAT32 at address %p\n", s_static_fat32_filesystem);
    
    // Use placement new to initialize the pre-allocated memory
    FAT32* result = new(s_static_fat32_filesystem) FAT32(blockDevice);
    k_printf("[FAT32] Static FAT32 created successfully at %p\n", result);
    return result;
}

static void free_static_fat32_filesystem(FAT32* fs) {
    if (!fs || !s_fat32_fs_allocated) return;
    
    // Call destructor manually
    fs->~FAT32();
    s_fat32_fs_allocated = false;
}

//=============================================================================
// FAT32Node Implementation
//=============================================================================

FAT32Node::FAT32Node(u32 inode, FileType type, FileSystem* fs, u32 firstCluster, u32 size)
    : VNode(inode, type, fs), m_firstCluster(firstCluster), m_size(size),
      m_dirEntries(nullptr), m_dirEntryCount(0), m_dirCacheValid(false) {
}

FAT32Node::~FAT32Node() {
    if (m_dirEntries) {
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(m_dirEntries);
        m_dirEntries = nullptr;
        m_dirCacheValid = false;
        m_dirEntryCount = 0;
    }
}

FSResult FAT32Node::read(u32 offset, u32 size, void* buffer) {
    if (!buffer || size == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    if (m_type != FileType::REGULAR) {
        return FSResult::IS_DIRECTORY;
    }
    
    if (offset >= m_size) {
        return FSResult::SUCCESS; // EOF
    }
    
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    return fat32->read_file_data(m_firstCluster, offset, size, buffer);
}

FSResult FAT32Node::write(u32 offset, u32 size, const void* buffer) {
    if (!buffer || size == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    if (m_type != FileType::REGULAR) {
        return FSResult::IS_DIRECTORY;
    }
    
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    FSResult result = fat32->write_file_data(m_firstCluster, offset, size, buffer);
    
    if (result == FSResult::SUCCESS) {
        // Update file size if we extended it
        if (offset + size > m_size) {
            m_size = offset + size;
        }
    }
    
    return result;
}

FSResult FAT32Node::get_size(u32& size) {
    size = m_size;
    return FSResult::SUCCESS;
}

FSResult FAT32Node::get_stat(FileStat& stat) {
    stat.size = m_size;
    stat.type = m_type;
    stat.mode = 0644; // Default permissions
    stat.uid = 0;
    stat.gid = 0;
    stat.atime = 0; // TODO: Parse from directory entry
    stat.mtime = 0; // TODO: Parse from directory entry  
    stat.ctime = 0; // TODO: Parse from directory entry
    
    return FSResult::SUCCESS;
}

FSResult FAT32Node::read_dir(u32 index, DirectoryEntry& entry) {
    // kira::kernel::console.add_message("[FAT32 - read_dir] read_dir start", VGA_GREEN_ON_BLUE);

    if (m_type != FileType::DIRECTORY) {
        kira::kernel::console.add_message("[FAT32] not directory", VGA_RED_ON_BLUE);
        return FSResult::NOT_DIRECTORY;
    }
    
    FSResult result = load_directory_cache();
    if (result != FSResult::SUCCESS) {
        kira::kernel::console.add_message("[FAT32 - read_dir] load_directory_cache failed", VGA_RED_ON_BLUE);
        return result;
    }
    // kira::kernel::console.add_message("[FAT32 - read_dir] load_directory_cache success", VGA_GREEN_ON_BLUE);
    
    // Find the Nth valid entry (skipping deleted entries and volume labels)
    u32 validEntryCount = 0;
    u32 physicalIndex = 0;
    u32 maxEntries = Fat32Const::PageSize / sizeof(Fat32DirEntry); // Same as in load_directory_cache
    
    // k_printf("[FAT32 - read_dir] Looking for logical index %d\n", index);
    
    for (physicalIndex = 0; physicalIndex < maxEntries; physicalIndex++) {
        Fat32DirEntry& entry = m_dirEntries[physicalIndex];
        
        // End of directory
        if (entry.name[0] == Fat32DirEntryName::END_OF_DIR) {
            // k_printf("[FAT32 - read_dir] Hit end of directory at physical index %d\n", physicalIndex);
            break;
        }
        
        // Skip deleted entries and volume labels
        if (entry.name[0] == Fat32DirEntryName::DELETED || (entry.attr & Fat32Attr::VOLUME_ID)) {
            // k_printf("[FAT32 - read_dir] Skipping physical index %d (deleted=%s, volume=%s)\n", 
            //          physicalIndex, 
            //          (entry.name[0] == Fat32DirEntryName::DELETED) ? "yes" : "no",
            //          (entry.attr & Fat32Attr::VOLUME_ID) ? "yes" : "no");
            continue;
        }
        
        // This is a valid entry - check if it's the one we want
        if (validEntryCount == index) {
            // k_printf("[FAT32 - read_dir] Found target! logical=%d -> physical=%d, name=%.11s\n", 
            //          index, physicalIndex, entry.name);
            break;
        }
        
        validEntryCount++;
    }
    
    // Check if we found the requested entry
    if (validEntryCount != index || physicalIndex >= maxEntries) {
        k_printf("[FAT32 - read_dir] Entry not found: logical=%d, found_count=%d, physical=%d\n", 
                 index, validEntryCount, physicalIndex);
        return FSResult::NOT_FOUND;
    }
    
    Fat32DirEntry& fatEntry = m_dirEntries[physicalIndex];
    
    // Convert FAT32 name to standard format
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    fat32->convert_fat_name(fatEntry.name, entry.name);
    
    entry.inode = fat32->allocate_inode();
    entry.type = (fatEntry.attr & Fat32Attr::DIRECTORY) ? FileType::DIRECTORY : FileType::REGULAR;
    
    return FSResult::SUCCESS;
}

FSResult FAT32Node::create_file(const char* name, FileType type) {
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    if (!name || strlen(name) == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    k_printf("[FAT32] create_file start - create_file_in_directory\n");
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    FSResult result = fat32->create_file_in_directory(m_firstCluster, name, type);
    
    // Invalidate directory cache so it gets reloaded with the new file
    if (result == FSResult::SUCCESS) {
        k_printf("[FAT32] create_file start - create_file_in_directory - success\n");

        m_dirCacheValid = false;
        // Force filesystem sync to ensure directory changes are written to disk
        FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
        fat32->sync();
    }
    
    return result;
}

FSResult FAT32Node::delete_file(const char* name) {
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    FSResult result = fat32->delete_file_from_directory(m_firstCluster, name);
    
    // Invalidate directory cache so it gets reloaded without the deleted file
    if (result == FSResult::SUCCESS) {
        m_dirCacheValid = false;
        // Force filesystem sync to ensure directory changes are written to disk
        FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
        fat32->sync();
    }
    
    return result;
}

FSResult FAT32Node::lookup(const char* name, VNode*& result) {
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    return fat32->lookup_in_directory(m_firstCluster, name, result);
}

FSResult FAT32Node::load_directory_cache() {
    if (m_dirCacheValid) {
        // kira::kernel::console.add_message("[FAT32 - load_directory_cache] load_directory_cache m_dirCacheValid valid, returning success", VGA_GREEN_ON_BLUE);

        return FSResult::SUCCESS;
    }
    
    if (m_type != FileType::DIRECTORY) {
        kira::kernel::console.add_message("[FAT32 - load_directory_cache] load_directory_cache m_type not directory, returning not directory", VGA_RED_ON_BLUE);
        return FSResult::NOT_DIRECTORY;
    }
    
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    // Allocate buffer for directory entries
    auto& memMgr = MemoryManager::get_instance();
    if (!m_dirEntries) {
        m_dirEntries = static_cast<Fat32DirEntry*>(memMgr.allocate_physical_page());
        if (!m_dirEntries) {
            kira::kernel::console.add_message("[FAT32 - load_directory_cache] load_directory_cache m_dirEntries no space", VGA_RED_ON_BLUE);
            return FSResult::NO_SPACE;
        }
    }
    // k_printf("[FAT32 - load_directory_cache] load_directory_cache m_dirEntries name: %s\n", (char*)m_dirEntries->name);
    
    // Read directory data
    u32 dirSize = fat32->get_cluster_chain_size(m_firstCluster);
    u32 entriesPerPage = Fat32Const::PageSize / sizeof(Fat32DirEntry);
    u32 maxEntries = (dirSize < Fat32Const::PageSize) ? (dirSize / sizeof(Fat32DirEntry)) : entriesPerPage;
    // kira::kernel::console.add_message("[FAT32 - load_directory_cache] load_directory_cache Read directory data", VGA_GREEN_ON_BLUE);
    // k_printf("[FAT32 - load_directory_cache] load_directory_cache Read directory data entriesPerPage: %d, maxEntries: %d\n", entriesPerPage, maxEntries);

    FSResult result = fat32->read_file_data(m_firstCluster, 0, maxEntries * sizeof(Fat32DirEntry), m_dirEntries);
    if (result != FSResult::SUCCESS) {
        kira::kernel::console.add_message("[FAT32 - load_directory_cache] load_directory_cache Read directory data failed", VGA_RED_ON_BLUE);
        return result;
    }
    
    // Count valid entries (exclude volume labels and deleted entries)
    m_dirEntryCount = 0;
    for (u32 i = 0; i < maxEntries; i++) {
        if (m_dirEntries[i].name[0] == Fat32DirEntryName::END_OF_DIR) {
            break; // End of directory
        }
        if (m_dirEntries[i].name[0] != Fat32DirEntryName::DELETED && !(m_dirEntries[i].attr & Fat32Attr::VOLUME_ID)) {
            // Count only non-deleted, non-volume-label entries
            m_dirEntryCount++;
        }
    }
    // k_printf("[FAT32 - load_directory_cache] m_dirEntryCount: %d, maxEntries:%d\n", m_dirEntryCount, maxEntries);

    m_dirCacheValid = true;
    return FSResult::SUCCESS;
}

//=============================================================================
// FAT32 Implementation
//=============================================================================

FAT32::FAT32(BlockDevice* device) 
    : m_device(device), m_root(nullptr), m_mounted(false), m_fatStartSector(0), m_dataStartSector(0),
      m_sectorsPerCluster(0), m_bytesPerCluster(0), m_nextInode(1),
      m_fatCache(nullptr), m_fatCacheSector(Fat32Const::InvalidIndex), m_fatCacheDirty(false) {
}

FAT32::~FAT32() {
    auto& memMgr = MemoryManager::get_instance();
    
    if (m_fatCache) {
        memMgr.free_physical_page(m_fatCache);
    }
    
    free_all_nodes_in_cache();
}



FSResult FAT32::mount(const char* device) {    
    if (m_mounted) {
        kira::kernel::console.add_message("[FAT32] already mounted", VGA_RED_ON_BLUE);
        return FSResult::EXISTS;
    }
    
    // Device should already be set in constructor
    if (!m_device) {
        kira::kernel::console.add_message("[FAT32] no device", VGA_RED_ON_BLUE);
        return FSResult::NOT_FOUND;
    }
    
    // Parse BPB (BIOS Parameter Block)
    FSResult result = parse_bpb();
    if (result != FSResult::SUCCESS) {
        kira::kernel::console.add_message("[FAT32] BPB parsing failed", VGA_RED_ON_BLUE);
        return result;
    }
    
    // Allocate FAT cache
    auto& memMgr = MemoryManager::get_instance();
    m_fatCache = static_cast<u32*>(memMgr.allocate_physical_page());
    if (!m_fatCache) {
        kira::kernel::console.add_message("[FAT32] FAT cache allocation failed", VGA_RED_ON_BLUE);
        return FSResult::NO_SPACE;
    }
        
    // Initialize node cache
    init_node_cache();
    
    // Create or retrieve root directory node
    m_root = get_or_create_node(m_bpb.root_cluster, FileType::DIRECTORY, 0);
    if (!m_root) {
        kira::kernel::console.add_message("[FAT32] root node allocation failed", VGA_RED_ON_BLUE);
        return FSResult::NO_SPACE;
    }
    
    m_mounted = true;
    return FSResult::SUCCESS;
}

FSResult FAT32::unmount() {
    if (!m_mounted) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Flush any cached data
    sync();
    
    if (m_root) {
        m_root = nullptr;
    }
    free_all_nodes_in_cache();
    
    m_mounted = false;
    return FSResult::SUCCESS;
}

FSResult FAT32::get_root(VNode*& root) {
    if (!m_mounted || !m_root) {
        return FSResult::INVALID_PARAMETER;
    }
    
    root = m_root;
    return FSResult::SUCCESS;
}

FSResult FAT32::create_vnode(u32 inode, FileType type, VNode*& vnode) {
    FAT32Node* node = allocate_dynamic_fat32_node(inode, type, this, 0, 0);
    if (!node) {
        return FSResult::NO_SPACE;
    }
    vnode = node;
    return FSResult::SUCCESS;
}

FSResult FAT32::sync() {
    if (m_fatCacheDirty) {
        // 🚨 CRITICAL CHECK: Ensure we're not writing FAT data to boot sector!
        if (m_fatCacheSector == 0) {
            k_printf("[FAT32] 🚨 CRITICAL ERROR: sync() trying to write FAT to boot sector! Aborting.\n");
            k_printf("[FAT32] 🚨 DEBUG: m_fatCacheSector=%d, m_fatStartSector=%d\n", 
                     m_fatCacheSector, m_fatStartSector);
            return FSResult::IO_ERROR;
        }
        
        // Write back FAT cache
        k_printf("[FAT32 - sync] Writing FAT to sector %d (should be >=32)\n", m_fatCacheSector);
        FSResult result = m_device->write_blocks(m_fatCacheSector, 1, m_fatCache);
        if (result != FSResult::SUCCESS) {
            return result;
        }
        m_fatCacheDirty = false;
    }
    
    return FSResult::SUCCESS;
}



FSResult FAT32::parse_bpb() {    
    
    // m_fatCacheSector = 5;
    // 🎯 FIX: Use safe 512-byte buffer to prevent overflow corruption
    u8 bootSectorBuffer[Fat32Const::SectorSize];  // Exactly one sector
    FSResult result = m_device->read_blocks(0, 1, bootSectorBuffer);
    if (result != FSResult::SUCCESS) {
        kira::kernel::console.add_message("[FAT32] Boot sector read failed", VGA_RED_ON_BLUE);
        return result;
    }
    
    // Safely copy only the BPB portion (avoid buffer overflow)
    // Manual copy to avoid memcpy dependency issues
    u8* bpbBytes = reinterpret_cast<u8*>(&m_bpb);
    for (u32 i = 0; i < sizeof(Fat32Bpb); i++) {
        bpbBytes[i] = bootSectorBuffer[i];
    }
    
    // kira::kernel::console.add_message("[FAT32] Boot sector read successfully", VGA_GREEN_ON_BLUE);

    // Validate FAT32 signature
    if (m_bpb.bytes_per_sector != Fat32Const::SectorSize) {
        k_printf("[FAT32] Invalid bytes_per_sector (not 512): %d\n", m_bpb.bytes_per_sector);
        kira::kernel::console.add_message("[FAT32] Invalid bytes_per_sector (not 512)", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    
    if (m_bpb.num_fats == 0 || m_bpb.num_fats > 2) {
        kira::kernel::console.add_message("[FAT32] Invalid num_fats", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    
    // Calculate derived values
    m_sectorsPerCluster = m_bpb.sectors_per_cluster;
    m_bytesPerCluster = m_bpb.bytes_per_sector * m_sectorsPerCluster;
    
    // Safety check to prevent division by zero
    if (m_sectorsPerCluster == 0 || m_bytesPerCluster == 0) {
        kira::kernel::console.add_message("[FAT32] Invalid cluster size (zero)", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    
    kira::kernel::console.add_message("[FAT32] BPB validation passed", VGA_GREEN_ON_BLUE);
    
    u32 fatSize = m_bpb.fat_size_32;
    u32 rootDirSectors = 0; // FAT32 has no fixed root directory
    
    m_fatStartSector = m_bpb.reserved_sector_count;
    m_dataStartSector = m_bpb.reserved_sector_count + (m_bpb.num_fats * fatSize) + rootDirSectors;
    
    // 🔍 DEBUG: Show calculated values
    k_printf("[FAT32 BPB] reserved_sector_count=%d, num_fats=%d, fat_size_32=%d\n", 
             m_bpb.reserved_sector_count, m_bpb.num_fats, m_bpb.fat_size_32);
    k_printf("[FAT32 BPB] m_fatStartSector=%d, m_dataStartSector=%d\n", 
             m_fatStartSector, m_dataStartSector);
    k_printf("[FAT32 BPB] root_cluster=%d\n", m_bpb.root_cluster);
    
    return FSResult::SUCCESS;
}

// 🔍 DEBUG: Check if BPB has been corrupted
FSResult FAT32::check_bpb_integrity() {
    u8 bootSectorBuffer[Fat32Const::SectorSize];
    FSResult result = m_device->read_blocks(0, 1, bootSectorBuffer);
    if (result != FSResult::SUCCESS) {
        k_printf("[FAT32] 🚨 BPB integrity check: Cannot read boot sector!\n");
        return result;
    }
    
    u16 current_bytes_per_sector = *reinterpret_cast<u16*>(&bootSectorBuffer[Fat32Const::BpbBytesPerSectorOffset]);
    if (current_bytes_per_sector != Fat32Const::SectorSize) {
        k_printf("[FAT32] 🚨 BPB CORRUPTION DETECTED! bytes_per_sector=%d (should be 512)\n", 
                 current_bytes_per_sector);
        k_printf("[FAT32] 🚨 Boot sector first 32 bytes:\n");
        for (int i = 0; i < 32; i += 16) {
            k_printf("[FAT32] %02X: ", i);
            for (int j = 0; j < 16 && i+j < 32; j++) {
                k_printf("%02X ", bootSectorBuffer[i+j]);
            }
            k_printf("\n");
        }
        return FSResult::IO_ERROR;
    }
    
    k_printf("[FAT32] ✅ BPB integrity check passed: bytes_per_sector=%d\n", current_bytes_per_sector);
    return FSResult::SUCCESS;
}

u32 FAT32::cluster_to_sector(u32 cluster) {
    // k_printf("[FAT32] cluster_to_sector: cluster=%d, m_dataStartSector=%d, m_sectorsPerCluster=%d\n", 
            //  cluster, m_dataStartSector, m_sectorsPerCluster);
    
    if (cluster < Fat32Const::MinValidCluster) {
        k_printf("[FAT32] CRITICAL ERROR: Invalid cluster %d < 2, CANNOT RETURN BOOT SECTOR!\n", cluster);
        // NEVER return sector 0 (boot sector) - this would corrupt the BPB!
        // Return a safe invalid sector number instead
        return Fat32Const::InvalidIndex; // Invalid sector - will cause read/write to fail safely
    }
    
    u32 sector = m_dataStartSector + ((cluster - 2) * m_sectorsPerCluster);
    // k_printf("[FAT32] cluster_to_sector: cluster %d -> sector %d\n", cluster, sector);
    return sector;
}

FSResult FAT32::load_fat_sector(u32 sector) {
    if (m_fatCacheSector == sector) {
        // k_printf("[FAT32] load_fat_sector cache hit - already loaded\n");
        return FSResult::SUCCESS; // Already cached
    }
        
    // Write back dirty cache to THE OLD SECTOR before loading new one
    // 🎯 FIX: Only write back if m_fatCacheSector is a VALID sector number
    if (m_fatCacheDirty && m_fatCacheSector != Fat32Const::InvalidIndex) {
        k_printf("[FAT32] load_fat_sector writing back dirty cache to VALID sector: %d\n", m_fatCacheSector);
        FSResult result = m_device->write_blocks(m_fatCacheSector, 1, m_fatCache);
        if (result != FSResult::SUCCESS) {
            k_printf("[FAT32] load_fat_sector write_blocks failed\n");
            return result;
        }
        m_fatCacheDirty = false;
        // k_printf("[FAT32] load_fat_sector dirty cache written back successfully\n");
    } else if (m_fatCacheDirty && m_fatCacheSector == Fat32Const::InvalidIndex) {
        // k_printf("[FAT32] load_fat_sector SKIPPING writeback - m_fatCacheSector is INVALID (0xFFFFFFFF)\n");
        m_fatCacheDirty = false; // Reset dirty flag since we can't write back invalid sector
    }
    
    // NOW update the cached sector number
    m_fatCacheSector = sector;
    // k_printf("[FAT32] load_fat_sector loading new sector: %d\n", sector);

    // Load new sector
    FSResult result = m_device->read_blocks(sector, 1, m_fatCache);
    if (result != FSResult::SUCCESS) {
        // k_printf("[FAT32] load_fat_sector read_blocks failed\n");
        return result;
    }
    
    // k_printf("[FAT32] load_fat_sector success - sector %d loaded into cache\n", sector);

    return FSResult::SUCCESS;
}

FSResult FAT32::read_file_data(u32 firstCluster, u32 offset, u32 size, void* buffer) {
    if (!buffer || size == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Safety check to prevent division by zero
    if (m_bytesPerCluster == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    u8* byteBuffer = static_cast<u8*>(buffer);
    u32 bytesRead = 0;
    u32 currentCluster = firstCluster;
    u32 clusterOffset = offset % m_bytesPerCluster;
    u32 clustersToSkip = offset / m_bytesPerCluster;
    // k_printf("[FAT32 - read_file_data] start - firstCluster: %d, offset: %d, size: %d, clustersToSkip: %d\n", firstCluster, offset, size, clustersToSkip);
    // Skip to starting cluster
    for (u32 i = 0; i < clustersToSkip && currentCluster < Fat32Cluster::END_MIN; i++) {
        FSResult result = get_next_cluster(currentCluster, currentCluster);
        if (result != FSResult::SUCCESS) {
            kernel::console.add_message("[FAT32 - read_file_data] get_next_cluster failed", VGA_RED_ON_BLUE);
            return result;
        }
    }
    
    // Read data cluster by cluster
    while (bytesRead < size && currentCluster < Fat32Cluster::END_MIN) {
        // k_printf("[FAT32 - read_file_data] while loop - bytesRead: %d, currentCluster: %d\n", bytesRead, currentCluster);
        u32 sector = cluster_to_sector(currentCluster);
        u32 bytesToRead = size - bytesRead;
        u32 bytesInCluster = m_bytesPerCluster - clusterOffset;
        
        if (bytesToRead > bytesInCluster) {
            bytesToRead = bytesInCluster;
        }
        
        // 🎯 FIX: Use properly sized buffer to prevent overflow corruption
        auto& memMgr = MemoryManager::get_instance();
        u8* clusterBuffer = static_cast<u8*>(memMgr.allocate_physical_page());
        if (!clusterBuffer) {
            return FSResult::NO_SPACE;
        }
        
        FSResult result = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
        if (result != FSResult::SUCCESS) {
            kira::kernel::console.add_message("[FAT32 - read_file_data] m_device->read_blocks failed", VGA_RED_ON_BLUE);
            memMgr.free_physical_page(clusterBuffer);
            return result;
        }
        
        // Copy relevant portion
        memcpy(byteBuffer + bytesRead, clusterBuffer + clusterOffset, bytesToRead);
        bytesRead += bytesToRead;
        clusterOffset = 0; // Only first cluster might have offset
        
        // Free buffer before continuing
        memMgr.free_physical_page(clusterBuffer);
        
        // Move to next cluster
        result = get_next_cluster(currentCluster, currentCluster);
        if (result != FSResult::SUCCESS) {
            break;
        }
    }
    
    return FSResult::SUCCESS;
}

FSResult FAT32::write_file_data(u32 firstCluster, u32 offset, u32 size, const void* buffer) {
    if (!buffer || size == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Safety check to prevent division by zero
    if (m_bytesPerCluster == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    const u8* byteBuffer = static_cast<const u8*>(buffer);
    u32 bytesWritten = 0;
    u32 currentCluster = firstCluster;
    u32 clusterOffset = offset % m_bytesPerCluster;
    u32 clustersToSkip = offset / m_bytesPerCluster;
    
    // Skip to starting cluster
    for (u32 i = 0; i < clustersToSkip && currentCluster < Fat32Cluster::END_MIN; i++) {
        FSResult result = get_next_cluster(currentCluster, currentCluster);
        if (result != FSResult::SUCCESS) {
            return result;
        }
    }
    
    // Write data cluster by cluster
    while (bytesWritten < size) {
        // If we're at end of chain and need more space, extend it
        if (currentCluster >= Fat32Cluster::END_MIN) {
            u32 newCluster = allocate_cluster();
            if (newCluster == 0) {
                return FSResult::NO_SPACE;
            }
            
            // Link the previous cluster to the new one
            // (This is simplified - in a real implementation we'd need to track the previous cluster)
            FSResult linkResult = set_next_cluster(newCluster, Fat32Cluster::END_MAX);
            if (linkResult != FSResult::SUCCESS) {
                free_cluster(newCluster);
                return linkResult;
            }
            
            currentCluster = newCluster;
            clusterOffset = 0;
        }
        
        u32 sector = cluster_to_sector(currentCluster);
        u32 bytesToWrite = size - bytesWritten;
        u32 bytesInCluster = m_bytesPerCluster - clusterOffset;
        
        if (bytesToWrite > bytesInCluster) {
            bytesToWrite = bytesInCluster;
        }
        
        // 🎯 FIX: Use properly sized buffer to prevent overflow corruption
        auto& memMgr = MemoryManager::get_instance();
        u8* clusterBuffer = static_cast<u8*>(memMgr.allocate_physical_page());
        if (!clusterBuffer) {
            return FSResult::NO_SPACE;
        }
        
        if (clusterOffset != 0 || bytesToWrite < m_bytesPerCluster) {
            // Read existing cluster data
            FSResult result = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
            if (result != FSResult::SUCCESS) {
                memMgr.free_physical_page(clusterBuffer);
                return result;
            }
        }
        
        // Copy new data into cluster buffer
        memcpy(clusterBuffer + clusterOffset, byteBuffer + bytesWritten, bytesToWrite);
        
        // Write cluster back to disk
        FSResult result = m_device->write_blocks(sector, m_sectorsPerCluster, clusterBuffer);
        
        // Free buffer before continuing
        memMgr.free_physical_page(clusterBuffer);
        
        if (result != FSResult::SUCCESS) {
            return result;
        }
        
        bytesWritten += bytesToWrite;
        clusterOffset = 0; // Only first cluster might have offset
        
        // Move to next cluster
        if (bytesWritten < size) {
            result = get_next_cluster(currentCluster, currentCluster);
            if (result != FSResult::SUCCESS && currentCluster >= Fat32Cluster::END_MIN) {
                // Need to extend cluster chain
                continue; // Will be handled at the top of the loop
            } else if (result != FSResult::SUCCESS) {
                return result;
            }
        }
    }
    
    return FSResult::SUCCESS;
}

FSResult FAT32::get_next_cluster(u32 cluster, u32& nextCluster) {
    // k_printf("[FAT32 - get_next_cluster]: this=%p, m_fatCacheSector=%u (should be 0xFFFFFFFF=%u)\n", 
            //  this, m_fatCacheSector, 0xFFFFFFFF);
    
    if (cluster < Fat32Const::MinValidCluster) {
        kira::kernel::console.add_message("[FAT32] get_next_cluster < 2", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }
    // kira::kernel::console.add_message("[FAT32 - get_next_cluster] get_next_cluster cluster >= 2", VGA_GREEN_ON_BLUE);

    // Calculate FAT sector and offset
    u32 fatOffset = cluster * Fat32Const::FatEntrySize; // 4 bytes per FAT32 entry
    u32 fatSector = m_fatStartSector + (fatOffset / Fat32Const::SectorSize);
    u32 entryOffset = (fatOffset % Fat32Const::SectorSize) / Fat32Const::FatEntrySize;
    // k_printf("fatOffset: %d, fatSector: %d, entryOffset: %d\n", fatOffset, fatSector, entryOffset);
    // Load FAT sector
    FSResult result = load_fat_sector(fatSector);
    if (result != FSResult::SUCCESS) {
        kira::kernel::console.add_message("[FAT32] get_next_cluster load_fat_sector != suc", VGA_RED_ON_BLUE);

        return result;
    }
    
    // Get next cluster (mask off upper 4 bits)
    nextCluster = m_fatCache[entryOffset] & 0x0FFFFFFF;
    // k_printf("[FAT32] get_next_cluster: cluster=%u -> fatCache[%u]=0x%08X -> nextCluster=%u\n", 
            //  cluster, entryOffset, m_fatCache[entryOffset], nextCluster);
    // k_printf("[FAT32] ✅ nextCluster=%u (0x%08X) - EOC=%s\n", 
    //          nextCluster, nextCluster, (nextCluster >= 0x0FFFFFF8) ? "YES" : "NO");
    return FSResult::SUCCESS;
}

u32 FAT32::get_cluster_chain_size(u32 firstCluster) {
    u32 size = 0;
    u32 currentCluster = firstCluster;
    u32 clusterCount = 0;
    
    while (currentCluster < Fat32Cluster::END_MIN && clusterCount < Fat32Const::MaxClusterScan) { // Prevent infinite loops
        size += m_bytesPerCluster;
        clusterCount++;
        
        FSResult result = get_next_cluster(currentCluster, currentCluster);
        if (result != FSResult::SUCCESS) {
            kira::kernel::console.add_message("[FAT32] get_cluster_chain_size get_next_cluster failed", VGA_RED_ON_BLUE);
            break;
        }
    }
    
    // k_printf("[FAT32] get_cluster_chain_size: %u clusters, %u bytes\n", clusterCount, size);
    return size;
}

void FAT32::convert_fat_name(const u8* fatName, char* standardName) {
    // Convert 8.3 FAT name to standard format
    int pos = 0;
    
    // Copy name part (8 chars)
    for (int i = 0; i < Fat32Const::NameLen && fatName[i] != ' '; i++) {
        standardName[pos++] = fatName[i];
    }
    
    // Add extension if present
    if (fatName[Fat32Const::NameLen] != ' ') {
        standardName[pos++] = '.';
        for (int i = Fat32Const::NameLen; i < Fat32Const::ShortNameLen && fatName[i] != ' '; i++) {
            standardName[pos++] = fatName[i];
        }
    }
    
    standardName[pos] = '\0';
}

void FAT32::convert_standard_name_to_fat(const char* standardName, u8* fatName) {
    // Initialize with spaces
    for (int i = 0; i < Fat32Const::ShortNameLen; i++) {
        fatName[i] = ' ';
    }
    fatName[Fat32Const::ShortNameLen] = '\0';
    // k_printf("[FAT32 - convert_standard_name_to_fat] standardName: %s\n", standardName);
    int nameLen = strlen(standardName);
    int dotPos = -1;
    
    // Find the dot (extension separator)
    for (int i = 0; i < nameLen; i++) {
        if (standardName[i] == '.') {
            dotPos = i;
            break;
        }
    }
    
    // Copy name part (up to 8 characters)
    int nameEnd = (dotPos >= 0) ? dotPos : nameLen;
    int namePartLen = (nameEnd > Fat32Const::NameLen) ? Fat32Const::NameLen : nameEnd;
    
    for (int i = 0; i < namePartLen; i++) {
        fatName[i] = toupper(standardName[i]);
    }    
    // Copy extension part (up to 3 characters)
    if (dotPos >= 0 && dotPos < nameLen - 1) {
        int extStart = dotPos + 1;
        int extLen = nameLen - extStart;
        if (extLen > Fat32Const::ExtLen) extLen = Fat32Const::ExtLen;
        for (int i = 0; i < extLen; i++) {
            fatName[Fat32Const::NameLen + i] = toupper(standardName[extStart + i]);
        }
    }
    // k_printf("[FAT32 - convert_standard_name_to_fat] fatName: %s\n", (char*)fatName);
}

FSResult FAT32::create_file_in_directory(u32 dirCluster, const char* name, FileType type) {
    // k_printf("[FAT32 - create_file_in_directory] CRITICAL: dirCluster=%d, name=%s\n", dirCluster, name);
    // k_printf("[FAT32 - create_file_in_directory] CRITICAL: m_dataStartSector=%d, m_sectorsPerCluster=%d\n", 
            //  m_dataStartSector, m_sectorsPerCluster);

    if (!name || strlen(name) == 0) {
        k_printf("[FAT32 - create_file_in_directory] create_file_in_directory - invalid parameter\n");
        return FSResult::INVALID_PARAMETER;
    }
    // k_printf("[FAT32 - create_file_in_directory] checking existingNode \n");

    // Check if file already exists
    VNode* existingNode;
    if (lookup_in_directory(dirCluster, name, existingNode) == FSResult::SUCCESS) {
        k_printf("[FAT32 - create_file_in_directory] checking existingNode - exists\n");
        return FSResult::EXISTS;
    }
    // k_printf("[FAT32 - create_file_in_directory] checking existingNode - not exists\n");

    // Convert name to FAT format
    u8 fatName[12];
    convert_standard_name_to_fat(name, fatName);
    // k_printf("[FAT32 - create_file_in_directory] convert_standard_name_to_fat - name: %s and fatName: %s\n", name, fatName);

    // Allocate a new cluster for the file/directory
    u32 newCluster = allocate_cluster();
    if (newCluster == 0) {
        return FSResult::NO_SPACE;
    }
    
    // Find a free directory entry
    u32 currentCluster = dirCluster;
    u32 entryOffset = 0;
    bool foundFreeEntry = false;
    
    // 🎯 FIX: Use properly sized buffer to prevent overflow corruption
    auto& memMgr = MemoryManager::get_instance();
    u8* clusterBuffer = static_cast<u8*>(memMgr.allocate_physical_page());
    if (!clusterBuffer) {
        free_cluster(newCluster);
        return FSResult::NO_SPACE;
    }
    
    u32 targetSector = 0; // Track which sector we need to write back
    
    while (currentCluster < Fat32Cluster::END_MIN && !foundFreeEntry) {
        u32 sector = cluster_to_sector(currentCluster);
        // k_printf("[FAT32 - create_file_in_directory] currentCluster: %d, sector: %d\n", currentCluster, sector);
        
        // 🚨 CRITICAL CHECK: If sector is 0, we're about to corrupt the boot sector!
        if (sector == 0) {
            k_printf("[FAT32] 🚨 CRITICAL ERROR: sector=0 would corrupt boot sector! Aborting.\n");
            free_cluster(newCluster);
            memMgr.free_physical_page(clusterBuffer);
            return FSResult::IO_ERROR;
        }
        
        // Read cluster containing directory entries
        
        FSResult readResult = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
        if (readResult != FSResult::SUCCESS) {
            k_printf("[FAT32 - create_file_in_directory] read_blocks failed\n");
            free_cluster(newCluster);
            memMgr.free_physical_page(clusterBuffer);
            return readResult;
        }
        
        // Parse directory entries in this cluster
        u32 entriesPerCluster = m_bytesPerCluster / sizeof(Fat32DirEntry);
        Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(clusterBuffer);
        // k_printf("[FAT32 - create_file_in_directory] entriesPerCluster: %d\n", entriesPerCluster);
        for (u32 i = 0; i < entriesPerCluster; i++) {
            Fat32DirEntry& entry = entries[i];
            // k_printf("[FAT32 - create_file_in_directory] entry: %s\n", entry.name);

            // Check for end of directory or free entry
            if (entry.name[0] == Fat32DirEntryName::END_OF_DIR || entry.name[0] == Fat32DirEntryName::DELETED) {
                // Found a free entry
                k_printf("[FAT32 - create_file_in_directory] found a free entry\n");
                foundFreeEntry = true;
                entryOffset = i;
                targetSector = sector; // Remember which sector to write back
                break;
            }
        }
        
        if (!foundFreeEntry) {
            // Move to next cluster in directory
            // k_printf("[FAT32] 🔄 No free entries in cluster %d, trying next cluster...\n", currentCluster);
            u32 nextCluster;
            FSResult nextResult = get_next_cluster(currentCluster, nextCluster);
            if (nextResult == FSResult::SUCCESS) {
                // Successfully found next cluster in chain - continue searching
                // k_printf("[FAT32] 🔄 Moving from cluster %d to cluster %d\n", currentCluster, nextCluster);
                currentCluster = nextCluster;
                // Continue to next iteration of while loop
            } else {
                // Reached end of directory chain - need to extend
                // k_printf("[FAT32] 🆕 Reached end of directory chain, extending...\n");
                u32 newDirCluster = allocate_cluster();
                if (newDirCluster == 0) {
                    k_printf("[FAT32 - create_file_in_directory] allocate_cluster failed: NO_SPACE\n");
                    free_cluster(newCluster);
                    memMgr.free_physical_page(clusterBuffer);
                    return FSResult::NO_SPACE;
                }
                
                // Link new cluster to directory chain
                set_next_cluster(currentCluster, newDirCluster);
                currentCluster = newDirCluster;
                entryOffset = 0;

                // Initialize new cluster with zeros
                memset(clusterBuffer, 0, 4096);  // 🎯 FIX: Clear entire page, not just pointer size
                
                // 🎯 FIX: We found a free entry in the new cluster!
                foundFreeEntry = true;
                targetSector = cluster_to_sector(currentCluster);
                // k_printf("[FAT32] 🆕 Extended directory: newCluster=%d, entryOffset=%d, targetSector=%d\n", 
                        //  newDirCluster, entryOffset, targetSector);
            }
        }
    }
    
    if (!foundFreeEntry) {
        k_printf("[FAT32 - create_file_in_directory] !foundFreeEntry, allocate_cluster failed: NO_SPACE\n");
        free_cluster(newCluster);
        memMgr.free_physical_page(clusterBuffer);
        return FSResult::NO_SPACE;
    }
    
    // Create the directory entry
    Fat32DirEntry newEntry;
    memset(&newEntry, 0, sizeof(newEntry));
    
    // Copy name
    memcpy(newEntry.name, fatName, 11);
    
    // Set attributes
    newEntry.attr = (type == FileType::DIRECTORY) ? Fat32Attr::DIRECTORY : Fat32Attr::ARCHIVE;
    
    // Set timestamps (current time - simplified)
    newEntry.create_time = 0;
    newEntry.create_date = 0;
    newEntry.write_time = 0;
    newEntry.write_date = 0;
    newEntry.last_access_date = 0;
    
    // Set first cluster
    newEntry.first_cluster_low = newCluster & 0xFFFF;
    newEntry.first_cluster_high = (newCluster >> 16) & 0xFFFF;
    
    // Set file size (0 for directories)
    newEntry.file_size = (type == FileType::DIRECTORY) ? 0 : 0;
    
    // Write the directory entry (reuse the cluster buffer we already have)
    Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(clusterBuffer);
    entries[entryOffset] = newEntry;
    
    // 🚨 CRITICAL CHECK: Ensure we're not writing to boot sector!
    if (targetSector == 0) {
        k_printf("[FAT32] 🚨 CRITICAL ERROR: targetSector=0 would corrupt boot sector! Aborting write.\n");
        k_printf("[FAT32] 🚨 DEBUG: currentCluster=%d, dirCluster=%d, newCluster=%d\n", 
                 currentCluster, dirCluster, newCluster);
        free_cluster(newCluster);
        memMgr.free_physical_page(clusterBuffer);
        return FSResult::IO_ERROR;
    }
    
    // Write the cluster back to disk
    // k_printf("[FAT32] Writing directory entry to targetSector=%d (should be ~3184)\n", targetSector);
    FSResult writeResult = m_device->write_blocks(targetSector, m_sectorsPerCluster, clusterBuffer);
    if (writeResult != FSResult::SUCCESS) {
        free_cluster(newCluster);
        memMgr.free_physical_page(clusterBuffer);
        return writeResult;
    }
    
    // If creating a directory, initialize it with . and .. entries
    if (type == FileType::DIRECTORY) {
        FSResult initResult = initialize_directory(newCluster, dirCluster);
        if (initResult != FSResult::SUCCESS) {
            // Clean up on failure
            free_cluster(newCluster);
            memMgr.free_physical_page(clusterBuffer);
            return initResult;
        }
    }
    
    // Clean up and return success
    memMgr.free_physical_page(clusterBuffer);
    
    // 🔍 DEBUG: Check BPB integrity after file creation
    // k_printf("[FAT32] 🔍 Checking BPB integrity after file creation...\n");
    check_bpb_integrity();
    
    return FSResult::SUCCESS;
}

FSResult FAT32::delete_file_from_directory(u32 dirCluster, const char* name) {
    if (!name || name[0] == '\0') {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Convert name to FAT format for comparison
    u8 fatName[11];
    convert_standard_name_to_fat(name, fatName);
    // k_printf("[FAT32 - delete_file_from_directory] delete_file_from_directory - name: %s, fatName: %s\n", name, fatName);
    // Get memory manager
    auto& memMgr = MemoryManager::get_instance();
    
    // Read directory cluster by cluster to find the file
    u32 currentCluster = dirCluster;
    
    while (currentCluster >= 2 && currentCluster < Fat32Cluster::END_MIN) {
        // Allocate buffer for this cluster
        u8* clusterBuffer = static_cast<u8*>(memMgr.allocate_physical_page());
        if (!clusterBuffer) {
            return FSResult::NO_SPACE;
        }
        
        u32 sector = cluster_to_sector(currentCluster);
        // k_printf("[FAT32 - delete_file_from_directory] delete_file_from_directory - sector: %d\n", sector);
        FSResult readResult = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
        if (readResult != FSResult::SUCCESS) {
            memMgr.free_physical_page(clusterBuffer);
            return FSResult::IO_ERROR;
        }
        
        // Search through directory entries in this cluster
        Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(clusterBuffer);
        u32 entriesPerCluster = m_bytesPerCluster / sizeof(Fat32DirEntry);
        
        for (u32 i = 0; i < entriesPerCluster; i++) {
            Fat32DirEntry& entry = entries[i];
            
            // End of directory
            if (entry.name[0] == Fat32DirEntryName::END_OF_DIR) {
                memMgr.free_physical_page(clusterBuffer);
                return FSResult::NOT_FOUND;
            }
            
            // Skip deleted entries
            if (entry.name[0] == Fat32DirEntryName::DELETED) {
                continue;
            }
            
            // Skip volume label and system files
            if (entry.attr & (Fat32Attr::VOLUME_ID | Fat32Attr::SYSTEM)) {
                continue;
            }
            
            // Check if this matches our target file
            if (memcmp(entry.name, fatName, 11) == 0) {
                k_printf("[FAT32 - delete_file_from_directory] delete_file_from_directory - found the file, entry.name: %s, fatName: %s\n", entry.name, fatName);
                // Found it! Get file info before deleting
                u32 firstCluster = (static_cast<u32>(entry.first_cluster_high) << 16) | entry.first_cluster_low;
                bool isDirectory = (entry.attr & Fat32Attr::DIRECTORY) != 0;
                
                // Mark as deleted
                entry.name[0] = Fat32DirEntryName::DELETED;
                
                // Write the modified cluster back
                FSResult writeResult = m_device->write_blocks(sector, m_sectorsPerCluster, clusterBuffer);
                
                // Free buffer before continuing
                memMgr.free_physical_page(clusterBuffer);
                
                if (writeResult != FSResult::SUCCESS) {
                    return FSResult::IO_ERROR;
                }
                
                // Free the file's cluster chain if it has one
                if (firstCluster >= 2) {
                    if (isDirectory) {
                        // For directories, recursively delete contents first
                        k_printf("[FAT32 - delete_file_from_directory] delete_file_from_directory - deleting directory contents\n");
                        delete_directory_contents(firstCluster);
                    }
                    free_cluster_chain(firstCluster);
                }
                
                // Sync to ensure changes are written
                sync();
                
                return FSResult::SUCCESS;
            }
        }
        
        // Free buffer before moving to next cluster
        memMgr.free_physical_page(clusterBuffer);
        
        // Move to next cluster in the chain
        u32 nextCluster;
        FSResult nextResult = get_next_cluster(currentCluster, nextCluster);
        if (nextResult != FSResult::SUCCESS) {
            break;
        }
        currentCluster = nextCluster;
    }
    
    return FSResult::NOT_FOUND;
}

FSResult FAT32::lookup_in_directory(u32 dirCluster, const char* name, VNode*& result) {
    // k_printf("[FAT32 - lookup_in_directory] lookup_in_directory start\n");
    if (!name || strlen(name) == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    // k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - name: %s\n", name);
    // Convert search name to FAT format for comparison
    u8 searchFatName[11];
    convert_standard_name_to_fat(name, searchFatName);
    // k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - searchFatName: %s\n", searchFatName);

    // Use the same method as VFS cache - read_file_data
    u32 dirSize = get_cluster_chain_size(dirCluster);
    u32 maxEntries = dirSize / sizeof(Fat32DirEntry);
    // k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - maxEntries: %d\n", maxEntries);

    // Allocate buffer for directory entries
    auto& memMgr = MemoryManager::get_instance();
    Fat32DirEntry* dirEntries = static_cast<Fat32DirEntry*>(memMgr.allocate_physical_page());
    if (!dirEntries) {
        return FSResult::NO_SPACE;
    }
    // k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - midddddd\n");

    // Read directory data using read_file_data (same as VFS cache)
    FSResult readResult = read_file_data(dirCluster, 0, maxEntries * sizeof(Fat32DirEntry), dirEntries);
    if (readResult != FSResult::SUCCESS) {
        k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - read_file_data failed\n");
        memMgr.free_physical_page(dirEntries);
        return readResult;
    }
    
    // Search through all entries
    for (u32 i = 0; i < maxEntries; i++) {
        Fat32DirEntry& entry = dirEntries[i];
        // k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - entry: %s\n", entry.name);

        // Skip deleted entries
        if (entry.name[0] == Fat32DirEntryName::DELETED) {
            continue;
        }
        
        // Skip end of directory marker
        if (entry.name[0] == Fat32DirEntryName::END_OF_DIR) {
            break;
        }
        
        // Skip volume label and system files
        if (entry.attr & (Fat32Attr::VOLUME_ID | Fat32Attr::SYSTEM)) {
            continue;
        }

        // for (int j = 0; j < 11; j++) {
        //     k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - entry.name[%d]: %c, searchFatName[%d]: %c\n", j, entry.name[j], j, searchFatName[j]);
        // }
        // if (entry.name[10] != '\0') {
        //     k_printf("[FAT32 - lookup_in_directory] haha, lookup_in_directory - entry.name[11]: \\0\n");
        // }

        // Check if this is the file we're looking for
        if (memcmp(entry.name, searchFatName, 11) == 0) {
            // Found the file! Create a VNode for it using static allocation
            // k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - found the file, entry.name: %s, searchFatName: %s!\n", entry.name, searchFatName);

            // Get first cluster
            u32 firstCluster = (static_cast<u32>(entry.first_cluster_high) << 16) | entry.first_cluster_low;
            
            // Determine file type
            FileType type = (entry.attr & Fat32Attr::DIRECTORY) ? FileType::DIRECTORY : FileType::REGULAR;
            
            // Create or reuse FAT32Node via cache using firstCluster as identity
            FAT32Node* node = get_or_create_node(firstCluster, type, entry.file_size);
            if (!node) {
                k_printf("[FAT32 - lookup_in_directory] get_or_create_node failed\n");
                memMgr.free_physical_page(dirEntries);
                return FSResult::NO_SPACE;
            }
            result = node;
            
            memMgr.free_physical_page(dirEntries);
            // k_printf("[FAT32 - lookup_in_directory] lookup_in_directory - found the file, return success\n");

            return FSResult::SUCCESS;
        }
    }
    
    memMgr.free_physical_page(dirEntries);
    return FSResult::NOT_FOUND;
}

u32 FAT32::allocate_cluster() {
    // Start searching from cluster 2 (first data cluster)
    for (u32 cluster = 2; cluster < 0x0FFFFFF0; cluster++) {
        u32 fatOffset = cluster * 4; // 4 bytes per FAT32 entry
        u32 fatSector = m_fatStartSector + (fatOffset / 512);
        u32 entryOffset = (fatOffset % 512) / 4;
        
        // Load FAT sector
        // k_printf("[FAT32 - allocate_cluster] allocate_cluster - m_fatCacheSector: %d, fatSector: %d\n", m_fatCacheSector, fatSector);
        FSResult result = load_fat_sector(fatSector);
        if (result != FSResult::SUCCESS) {
            return 0; // Failed to load FAT
        }
        
        // Check if cluster is free
        if ((m_fatCache[entryOffset] & 0x0FFFFFFF) == Fat32Cluster::FREE) {
            // Mark cluster as end of chain
            // k_printf("[FAT32 - allocate_cluster] allocate_cluster - cluster: %d, entryOffset: %d, m_fatCache[entryOffset]: %d\n", cluster, entryOffset, m_fatCache[entryOffset]);
            m_fatCache[entryOffset] = Fat32Cluster::END_MAX;
            m_fatCacheDirty = true;
            
            // Flush changes to disk
            sync();
            
            return cluster;
        }
    }
    
    return 0; // No free clusters found
}

FSResult FAT32::free_cluster(u32 cluster) {
    if (cluster < 2) {
        return FSResult::INVALID_PARAMETER;
    }
    
    u32 fatOffset = cluster * 4; // 4 bytes per FAT32 entry
    u32 fatSector = m_fatStartSector + (fatOffset / 512);
    u32 entryOffset = (fatOffset % 512) / 4;
    
    // Load FAT sector
    FSResult result = load_fat_sector(fatSector);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    // Mark cluster as free
    m_fatCache[entryOffset] = Fat32Cluster::FREE;
    m_fatCacheDirty = true;
    
    return FSResult::SUCCESS;
}

//============================
// Node cache implementation
//============================

void FAT32::init_node_cache() {
    for (u32 i = 0; i < NODE_CACHE_CAPACITY; i++) {
        m_nodeCache[i].cluster = 0xFFFFFFFFu;
        m_nodeCache[i].node = nullptr;
        m_nodeCache[i].lruPrev = 0xFFFFFFFFu;
        m_nodeCache[i].lruNext = 0xFFFFFFFFu;
        m_nodeCache[i].refCount = 0;
    }
    m_nodeCacheCount = 0;
    m_lruHead = 0xFFFFFFFFu;
    m_lruTail = 0xFFFFFFFFu;
}

void FAT32::free_all_nodes_in_cache() {
    for (u32 i = 0; i < NODE_CACHE_CAPACITY; i++) {
        if (m_nodeCache[i].node) {
            free_dynamic_fat32_node(m_nodeCache[i].node);
            m_nodeCache[i].node = nullptr;
            m_nodeCache[i].cluster = 0xFFFFFFFFu;
            m_nodeCache[i].lruPrev = 0xFFFFFFFFu;
            m_nodeCache[i].lruNext = 0xFFFFFFFFu;
            m_nodeCache[i].refCount = 0;
        }
    }
    m_nodeCacheCount = 0;
    m_lruHead = 0xFFFFFFFFu;
    m_lruTail = 0xFFFFFFFFu;
}

static u32 node_cache_hash(u32 cluster) {
    // Simple mix/hash for 32-bit key
    cluster ^= cluster >> 16;
    cluster *= 0x7feb352dU;
    cluster ^= cluster >> 15;
    cluster *= 0x846ca68bU;
    cluster ^= cluster >> 16;
    return cluster;
}

FAT32Node* FAT32::get_or_create_node(u32 firstCluster, FileType type, u32 size) {
    if (firstCluster == 0) {
        return nullptr;
    }
    kira::sync::SpinlockGuard guard(m_nodeCacheLock);
    // Open addressing with linear probing
    u32 idx = node_cache_hash(firstCluster) % NODE_CACHE_CAPACITY;
    u32 firstFree = 0xFFFFFFFFu;
    for (u32 probe = 0; probe < NODE_CACHE_CAPACITY; probe++) {
        u32 slot = (idx + probe) % NODE_CACHE_CAPACITY;
        if (m_nodeCache[slot].cluster == firstCluster) {
            // Touch LRU: move to front
            lru_remove(slot);
            lru_push_front(slot);
            if (m_nodeCache[slot].node) {
                m_nodeCache[slot].node->retain();
                m_nodeCache[slot].refCount = m_nodeCache[slot].node->get_refcount();
            }
            return m_nodeCache[slot].node;
        }
        if (m_nodeCache[slot].cluster == 0xFFFFFFFFu && firstFree == 0xFFFFFFFFu) {
            firstFree = slot;
        }
    }
    // Not found, insert if space available
    if (firstFree != 0xFFFFFFFFu) {
        FAT32Node* node = allocate_dynamic_fat32_node(allocate_inode(), type, this, firstCluster, size);
        if (!node) return nullptr;
        m_nodeCache[firstFree].cluster = firstCluster;
        m_nodeCache[firstFree].node = node;
        node->retain(); // retain for caller
        m_nodeCache[firstFree].refCount = node->get_refcount();
        m_nodeCacheCount++;
        lru_push_front(firstFree);
        return node;
    }
    // Cache full: try to evict a zero-ref node (tail)
    if (try_evict_one_zero_ref()) {
        // Retry insertion once
        return get_or_create_node(firstCluster, type, size);
    }
    // Fallback without caching if no eviction possible
    return allocate_dynamic_fat32_node(allocate_inode(), type, this, firstCluster, size);
}

void FAT32::lru_remove(u32 idx) {
    if (idx >= NODE_CACHE_CAPACITY) return;
    u32 prev = m_nodeCache[idx].lruPrev;
    u32 next = m_nodeCache[idx].lruNext;
    if (prev != 0xFFFFFFFFu) {
        m_nodeCache[prev].lruNext = next;
    } else if (m_lruHead == idx) {
        m_lruHead = next;
    }
    if (next != 0xFFFFFFFFu) {
        m_nodeCache[next].lruPrev = prev;
    } else if (m_lruTail == idx) {
        m_lruTail = prev;
    }
    m_nodeCache[idx].lruPrev = 0xFFFFFFFFu;
    m_nodeCache[idx].lruNext = 0xFFFFFFFFu;
}

void FAT32::lru_push_front(u32 idx) {
    if (idx >= NODE_CACHE_CAPACITY) return;
    m_nodeCache[idx].lruPrev = 0xFFFFFFFFu;
    m_nodeCache[idx].lruNext = m_lruHead;
    if (m_lruHead != 0xFFFFFFFFu) {
        m_nodeCache[m_lruHead].lruPrev = idx;
    } else {
        m_lruTail = idx;
    }
    m_lruHead = idx;
}

bool FAT32::try_evict_one_zero_ref() {
    // Walk from tail until a zero-ref node is found
    u32 idx = m_lruTail;
    while (idx != 0xFFFFFFFFu) {
        NodeCacheEntry& e = m_nodeCache[idx];
        // Consider nodes without outstanding VFS references as evictable
        if (e.node && e.node->get_refcount() == 1) { // only cached owner holds one ref
            // No refcount tracking wired yet; assume evictable if not root
            if (e.cluster != m_bpb.root_cluster) {
                FAT32Node* victim = e.node;
                e.node = nullptr;
                u32 next = e.lruPrev; // preserve before remove
                lru_remove(idx);
                e.cluster = 0xFFFFFFFFu;
                free_dynamic_fat32_node(victim);
                m_nodeCacheCount--;
                return true;
            }
        }
        idx = e.lruPrev;
    }
    return false;
}

FSResult FAT32::set_next_cluster(u32 cluster, u32 nextCluster) {
    if (cluster < 2) {
        return FSResult::INVALID_PARAMETER;
    }
    
    u32 fatOffset = cluster * 4; // 4 bytes per FAT32 entry
    u32 fatSector = m_fatStartSector + (fatOffset / 512);
    u32 entryOffset = (fatOffset % 512) / 4;
    
    // Load FAT sector
    FSResult result = load_fat_sector(fatSector);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    // Set next cluster (preserve upper 4 bits)
    m_fatCache[entryOffset] = (m_fatCache[entryOffset] & 0xF0000000) | (nextCluster & 0x0FFFFFFF);
    m_fatCacheDirty = true;
    
    return FSResult::SUCCESS;
}

FSResult FAT32::extend_cluster_chain(u32 lastCluster, u32& newCluster) {
    // Allocate a new cluster
    newCluster = allocate_cluster();
    if (newCluster == 0) {
        return FSResult::NO_SPACE;
    }
    
    // Link the last cluster to the new cluster
    FSResult result = set_next_cluster(lastCluster, newCluster);
    if (result != FSResult::SUCCESS) {
        free_cluster(newCluster);
        newCluster = 0;
        return result;
    }
    
    return FSResult::SUCCESS;
}

FSResult FAT32::initialize_directory(u32 dirCluster, u32 parentCluster) {
    // Read the directory cluster
    u32 sector = cluster_to_sector(dirCluster);
    
    // 🎯 FIX: Use properly sized buffer to prevent overflow corruption
    auto& memMgr = MemoryManager::get_instance();
    u8* clusterBuffer = static_cast<u8*>(memMgr.allocate_physical_page());
    if (!clusterBuffer) {
        return FSResult::NO_SPACE;
    }
    
    FSResult readResult = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
    if (readResult != FSResult::SUCCESS) {
        memMgr.free_physical_page(clusterBuffer);
        return readResult;
    }
    
    Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(clusterBuffer);
    
    // Create . entry (points to self)
    Fat32DirEntry& dotEntry = entries[0];
    memset(&dotEntry, 0, sizeof(dotEntry));
    
    // Set name to "."
    dotEntry.name[0] = '.';
    for (int i = 1; i < 11; i++) {
        dotEntry.name[i] = ' ';
    }
    // dotEntry.name[11] = '\0';
    // Set attributes
    dotEntry.attr = Fat32Attr::DIRECTORY;
    
    // Set first cluster to self
    dotEntry.first_cluster_low = dirCluster & 0xFFFF;
    dotEntry.first_cluster_high = (dirCluster >> 16) & 0xFFFF;
    
    // Create .. entry (points to parent)
    Fat32DirEntry& dotDotEntry = entries[1];
    memset(&dotDotEntry, 0, sizeof(dotDotEntry));
    
    // Set name to ".."
    dotDotEntry.name[0] = '.';
    dotDotEntry.name[1] = '.';
    for (int i = 2; i < 11; i++) {
        dotDotEntry.name[i] = ' ';
    }
    // dotDotEntry.name[11] = '\0';
    // Set attributes
    dotDotEntry.attr = Fat32Attr::DIRECTORY;
    
    // Set first cluster to parent
    dotDotEntry.first_cluster_low = parentCluster & 0xFFFF;
    dotDotEntry.first_cluster_high = (parentCluster >> 16) & 0xFFFF;
    
    // Mark end of directory
    Fat32DirEntry& endEntry = entries[2];
    endEntry.name[0] = Fat32DirEntryName::END_OF_DIR;
    
    // Write back the cluster
    FSResult writeResult = m_device->write_blocks(sector, m_sectorsPerCluster, clusterBuffer);
    memMgr.free_physical_page(clusterBuffer);
    
    if (writeResult != FSResult::SUCCESS) {
        return writeResult;
    }
    
    return FSResult::SUCCESS;
}

FSResult FAT32::delete_directory_contents(u32 dirCluster) {
    // Read directory entries cluster by cluster
    u32 currentCluster = dirCluster;
    auto& memMgr = MemoryManager::get_instance();
    
    while (currentCluster < Fat32Cluster::END_MIN) {
        k_printf("[FAT32 - delete_directory_contents] delete_directory_contents - currentCluster: %d\n", currentCluster);
        u32 sector = cluster_to_sector(currentCluster);
        k_printf("[FAT32 - delete_directory_contents] delete_directory_contents - sector: %d\n", sector);
        // 🎯 FIX: Use properly sized buffer to prevent overflow corruption
        u8* clusterBuffer = static_cast<u8*>(memMgr.allocate_physical_page());
        if (!clusterBuffer) {
            return FSResult::NO_SPACE;
        }
        
        FSResult readResult = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
        if (readResult != FSResult::SUCCESS) {
            memMgr.free_physical_page(clusterBuffer);
            return readResult;
        }
        
        // Parse directory entries in this cluster
        u32 entriesPerCluster = m_bytesPerCluster / sizeof(Fat32DirEntry);
        Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(clusterBuffer);
        
        for (u32 i = 0; i < entriesPerCluster; i++) {
            Fat32DirEntry& entry = entries[i];
            
            // Check for end of directory
            if (entry.name[0] == Fat32DirEntryName::END_OF_DIR) {
                memMgr.free_physical_page(clusterBuffer);
                return FSResult::SUCCESS; // End of directory
            }
            
            // Skip deleted entries and volume labels
            if (entry.name[0] == Fat32DirEntryName::DELETED || (entry.attr & Fat32Attr::VOLUME_ID)) {
                continue;
            }
            
            // Skip "." and ".." entries to prevent infinite recursion
            if ((entry.name[0] == '.' && entry.name[1] == ' ') ||
                (entry.name[0] == '.' && entry.name[1] == '.' && entry.name[2] == ' ')) {
                continue;
            }
            
            // Get first cluster of the file/directory
            u32 firstCluster = (static_cast<u32>(entry.first_cluster_high) << 16) | entry.first_cluster_low;
            
            // If it's a directory, recursively delete its contents
            if (entry.attr & Fat32Attr::DIRECTORY) {
                k_printf("[FAT32 - delete_directory_contents] delete_directory_contents - deleting directory contents, firstCluster: %d\n", firstCluster);
                FSResult deleteResult = delete_directory_contents(firstCluster);
                if (deleteResult != FSResult::SUCCESS) {
                    memMgr.free_physical_page(clusterBuffer);
                    return deleteResult;
                }
            }
            
            // Free the cluster chain
            FSResult freeResult = free_cluster_chain(firstCluster);
            if (freeResult != FSResult::SUCCESS) {
                memMgr.free_physical_page(clusterBuffer);
                return freeResult;
            }
            
            // Mark directory entry as deleted
            entry.name[0] = Fat32DirEntryName::DELETED;
            
            // Write back the modified cluster
            FSResult writeResult = m_device->write_blocks(sector, m_sectorsPerCluster, clusterBuffer);
            if (writeResult != FSResult::SUCCESS) {
                memMgr.free_physical_page(clusterBuffer);
                return writeResult;
            }
        }
        
        // Free buffer before moving to next cluster
        memMgr.free_physical_page(clusterBuffer);
        
        // Move to next cluster in directory
        FSResult nextResult = get_next_cluster(currentCluster, currentCluster);
        k_printf("[FAT32 - delete_directory_contents] delete_directory_contents - next currentCluster: %d\n", currentCluster);
        if (nextResult != FSResult::SUCCESS) {
            break;
        }
    }
    
    return FSResult::SUCCESS;
}

FSResult FAT32::free_cluster_chain(u32 firstCluster) {
    u32 currentCluster = firstCluster;
    
    while (currentCluster < Fat32Cluster::END_MIN) {
        FSResult result = free_cluster(currentCluster);
        if (result != FSResult::SUCCESS) {
            return result;
        }
        
        FSResult nextResult = get_next_cluster(currentCluster, currentCluster);
        if (nextResult != FSResult::SUCCESS) {
            break;
        }
    }
    
    return FSResult::SUCCESS;
}

//=============================================================================
// Static Factory Methods for FAT32 Filesystem
//=============================================================================

FAT32* FAT32::create_static_instance(BlockDevice* blockDevice) {
    return allocate_static_fat32_filesystem(blockDevice);
}

void FAT32::destroy_static_instance(FAT32* fs) {
    free_static_fat32_filesystem(fs);
}

} // namespace kira::fs 