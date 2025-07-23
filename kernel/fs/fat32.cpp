#include "fs/fat32.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"

namespace kira::fs {

using namespace kira::system;
using namespace kira::utils;

//=============================================================================
// FAT32Node Implementation
//=============================================================================

FAT32Node::FAT32Node(u32 inode, FileType type, FileSystem* fs, u32 firstCluster, u32 size)
    : VNode(inode, type, fs), m_firstCluster(firstCluster), m_size(size),
      m_dirEntries(nullptr), m_dirEntryCount(0), m_dirCacheValid(false) {
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
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    FSResult result = load_directory_cache();
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    if (index >= m_dirEntryCount) {
        return FSResult::NOT_FOUND; // End of directory
    }
    
    Fat32DirEntry& fatEntry = m_dirEntries[index];
    
    // Skip deleted entries and volume labels
    if (fatEntry.name[0] == 0xE5 || (fatEntry.attr & Fat32Attr::VOLUME_ID)) {
        return FSResult::NOT_FOUND;
    }
    
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
    
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    return fat32->create_file_in_directory(m_firstCluster, name, type);
}

FSResult FAT32Node::delete_file(const char* name) {
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    return fat32->delete_file_from_directory(m_firstCluster, name);
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
        return FSResult::SUCCESS;
    }
    
    if (m_type != FileType::DIRECTORY) {
        return FSResult::NOT_DIRECTORY;
    }
    
    FAT32* fat32 = static_cast<FAT32*>(m_filesystem);
    
    // Allocate buffer for directory entries
    auto& memMgr = MemoryManager::get_instance();
    if (!m_dirEntries) {
        m_dirEntries = static_cast<Fat32DirEntry*>(memMgr.allocate_physical_page());
        if (!m_dirEntries) {
            return FSResult::NO_SPACE;
        }
    }
    
    // Read directory data
    u32 dirSize = fat32->get_cluster_chain_size(m_firstCluster);
    u32 entriesPerPage = 4096 / sizeof(Fat32DirEntry);
    u32 maxEntries = (dirSize < 4096) ? (dirSize / sizeof(Fat32DirEntry)) : entriesPerPage;
    
    FSResult result = fat32->read_file_data(m_firstCluster, 0, maxEntries * sizeof(Fat32DirEntry), m_dirEntries);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    // Count valid entries
    m_dirEntryCount = 0;
    for (u32 i = 0; i < maxEntries; i++) {
        if (m_dirEntries[i].name[0] == 0x00) {
            break; // End of directory
        }
        if (m_dirEntries[i].name[0] != 0xE5) { // Not deleted
            m_dirEntryCount++;
        }
    }
    
    m_dirCacheValid = true;
    return FSResult::SUCCESS;
}

//=============================================================================
// FAT32 Implementation
//=============================================================================

FAT32::FAT32(BlockDevice* device) 
    : m_device(device), m_root(nullptr), m_fatStartSector(0), m_dataStartSector(0),
      m_sectorsPerCluster(0), m_bytesPerCluster(0), m_nextInode(1),
      m_fatCache(nullptr), m_fatCacheSector(0xFFFFFFFF), m_fatCacheDirty(false) {
}

FAT32::~FAT32() {
    if (m_fatCache) {
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(m_fatCache);
    }
    
    if (m_root) {
        m_root->~FAT32Node();
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(m_root);
    }
}

FSResult FAT32::mount(const char* device) {
    if (m_mounted) {
        return FSResult::EXISTS;
    }
    
    // Device should already be set in constructor
    if (!m_device) {
        return FSResult::NOT_FOUND;
    }
    
    // Parse BPB (BIOS Parameter Block)
    FSResult result = parse_bpb();
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    // Allocate FAT cache
    auto& memMgr = MemoryManager::get_instance();
    m_fatCache = static_cast<u32*>(memMgr.allocate_physical_page());
    if (!m_fatCache) {
        return FSResult::NO_SPACE;
    }
    
    // Create root directory node
    void* memory = memMgr.allocate_physical_page();
    if (!memory) {
        return FSResult::NO_SPACE;
    }
    
    m_root = new(memory) FAT32Node(allocate_inode(), FileType::DIRECTORY, this, m_bpb.root_cluster, 0);
    
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
        m_root->~FAT32Node();
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(m_root);
        m_root = nullptr;
    }
    
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
    auto& memMgr = MemoryManager::get_instance();
    void* memory = memMgr.allocate_physical_page();
    if (!memory) {
        return FSResult::NO_SPACE;
    }
    
    FAT32Node* node = new(memory) FAT32Node(inode, type, this, 0, 0);
    vnode = node;
    
    return FSResult::SUCCESS;
}

FSResult FAT32::sync() {
    if (m_fatCacheDirty) {
        // Write back FAT cache
        FSResult result = m_device->write_blocks(m_fatCacheSector, 1, m_fatCache);
        if (result != FSResult::SUCCESS) {
            return result;
        }
        m_fatCacheDirty = false;
    }
    
    return FSResult::SUCCESS;
}



FSResult FAT32::parse_bpb() {
    // Read boot sector
    FSResult result = m_device->read_blocks(0, 1, &m_bpb);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    // Validate FAT32 signature
    if (m_bpb.bytes_per_sector != 512) {
        return FSResult::INVALID_PARAMETER;
    }
    
    if (m_bpb.num_fats == 0 || m_bpb.num_fats > 2) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Calculate derived values
    m_sectorsPerCluster = m_bpb.sectors_per_cluster;
    m_bytesPerCluster = m_bpb.bytes_per_sector * m_sectorsPerCluster;
    
    // Safety check to prevent division by zero
    if (m_sectorsPerCluster == 0 || m_bytesPerCluster == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    u32 fatSize = m_bpb.fat_size_32;
    u32 rootDirSectors = 0; // FAT32 has no fixed root directory
    
    m_fatStartSector = m_bpb.reserved_sector_count;
    m_dataStartSector = m_bpb.reserved_sector_count + (m_bpb.num_fats * fatSize) + rootDirSectors;
    
    return FSResult::SUCCESS;
}

u32 FAT32::cluster_to_sector(u32 cluster) {
    if (cluster < 2) {
        return 0; // Invalid cluster
    }
    
    return m_dataStartSector + ((cluster - 2) * m_sectorsPerCluster);
}

FSResult FAT32::load_fat_sector(u32 sector) {
    if (m_fatCacheSector == sector) {
        return FSResult::SUCCESS; // Already cached
    }
    
    // Write back dirty cache
    if (m_fatCacheDirty) {
        FSResult result = m_device->write_blocks(m_fatCacheSector, 1, m_fatCache);
        if (result != FSResult::SUCCESS) {
            return result;
        }
        m_fatCacheDirty = false;
    }
    
    // Load new sector
    FSResult result = m_device->read_blocks(sector, 1, m_fatCache);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    m_fatCacheSector = sector;
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
    
    // Skip to starting cluster
    for (u32 i = 0; i < clustersToSkip && currentCluster < Fat32Cluster::END_MIN; i++) {
        FSResult result = get_next_cluster(currentCluster, currentCluster);
        if (result != FSResult::SUCCESS) {
            return result;
        }
    }
    
    // Read data cluster by cluster
    while (bytesRead < size && currentCluster < Fat32Cluster::END_MIN) {
        u32 sector = cluster_to_sector(currentCluster);
        u32 bytesToRead = size - bytesRead;
        u32 bytesInCluster = m_bytesPerCluster - clusterOffset;
        
        if (bytesToRead > bytesInCluster) {
            bytesToRead = bytesInCluster;
        }
        
        // Read cluster data
        u8 clusterBuffer[4096]; // Assume max cluster size
        FSResult result = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
        if (result != FSResult::SUCCESS) {
            return result;
        }
        
        // Copy relevant portion
        memcpy(byteBuffer + bytesRead, clusterBuffer + clusterOffset, bytesToRead);
        bytesRead += bytesToRead;
        clusterOffset = 0; // Only first cluster might have offset
        
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
        
        // For partial cluster writes, we need to read-modify-write
        u8 clusterBuffer[4096]; // Assume max cluster size
        
        if (clusterOffset != 0 || bytesToWrite < m_bytesPerCluster) {
            // Read existing cluster data
            FSResult result = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
            if (result != FSResult::SUCCESS) {
                return result;
            }
        }
        
        // Copy new data into cluster buffer
        memcpy(clusterBuffer + clusterOffset, byteBuffer + bytesWritten, bytesToWrite);
        
        // Write cluster back to disk
        FSResult result = m_device->write_blocks(sector, m_sectorsPerCluster, clusterBuffer);
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
    if (cluster < 2) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Calculate FAT sector and offset
    u32 fatOffset = cluster * 4; // 4 bytes per FAT32 entry
    u32 fatSector = m_fatStartSector + (fatOffset / 512);
    u32 entryOffset = (fatOffset % 512) / 4;
    
    // Load FAT sector
    FSResult result = load_fat_sector(fatSector);
    if (result != FSResult::SUCCESS) {
        return result;
    }
    
    // Get next cluster (mask off upper 4 bits)
    nextCluster = m_fatCache[entryOffset] & 0x0FFFFFFF;
    
    return FSResult::SUCCESS;
}

u32 FAT32::get_cluster_chain_size(u32 firstCluster) {
    u32 size = 0;
    u32 currentCluster = firstCluster;
    
    while (currentCluster < Fat32Cluster::END_MIN) {
        size += m_bytesPerCluster;
        
        FSResult result = get_next_cluster(currentCluster, currentCluster);
        if (result != FSResult::SUCCESS) {
            break;
        }
    }
    
    return size;
}

void FAT32::convert_fat_name(const u8* fatName, char* standardName) {
    // Convert 8.3 FAT name to standard format
    int pos = 0;
    
    // Copy name part (8 chars)
    for (int i = 0; i < 8 && fatName[i] != ' '; i++) {
        standardName[pos++] = fatName[i];
    }
    
    // Add extension if present
    if (fatName[8] != ' ') {
        standardName[pos++] = '.';
        for (int i = 8; i < 11 && fatName[i] != ' '; i++) {
            standardName[pos++] = fatName[i];
        }
    }
    
    standardName[pos] = '\0';
}

FSResult FAT32::create_file_in_directory(u32 dirCluster, const char* name, FileType type) {
    // TODO: Implement directory file creation
    return FSResult::INVALID_PARAMETER;
}

FSResult FAT32::delete_file_from_directory(u32 dirCluster, const char* name) {
    // TODO: Implement directory file deletion
    return FSResult::INVALID_PARAMETER;
}

FSResult FAT32::lookup_in_directory(u32 dirCluster, const char* name, VNode*& result) {
    if (!name || strlen(name) == 0) {
        return FSResult::INVALID_PARAMETER;
    }
    
    // Read directory entries cluster by cluster
    u32 currentCluster = dirCluster;
    
    while (currentCluster < Fat32Cluster::END_MIN) {
        u32 sector = cluster_to_sector(currentCluster);
        
        // Read cluster containing directory entries
        u8 clusterBuffer[4096]; // Assume max cluster size
        FSResult readResult = m_device->read_blocks(sector, m_sectorsPerCluster, clusterBuffer);
        if (readResult != FSResult::SUCCESS) {
            return readResult;
        }
        
        // Parse directory entries in this cluster
        u32 entriesPerCluster = m_bytesPerCluster / sizeof(Fat32DirEntry);
        Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(clusterBuffer);
        
        for (u32 i = 0; i < entriesPerCluster; i++) {
            Fat32DirEntry& entry = entries[i];
            
            // Check for end of directory
            if (entry.name[0] == 0x00) {
                return FSResult::NOT_FOUND;
            }
            
            // Skip deleted entries and volume labels
            if (entry.name[0] == 0xE5 || (entry.attr & Fat32Attr::VOLUME_ID)) {
                continue;
            }
            
            // Convert FAT name to standard format
            char standardName[256];
            convert_fat_name(entry.name, standardName);
            
            // Check if this is the file we're looking for
            if (strcmp(standardName, name) == 0) {
                // Found the file! Create a VNode for it
                auto& memMgr = MemoryManager::get_instance();
                void* memory = memMgr.allocate_physical_page();
                if (!memory) {
                    return FSResult::NO_SPACE;
                }
                
                // Get first cluster
                u32 firstCluster = (static_cast<u32>(entry.first_cluster_high) << 16) | entry.first_cluster_low;
                
                // Determine file type
                FileType type = (entry.attr & Fat32Attr::DIRECTORY) ? FileType::DIRECTORY : FileType::REGULAR;
                
                // Create FAT32Node
                FAT32Node* node = new(memory) FAT32Node(allocate_inode(), type, this, firstCluster, entry.file_size);
                result = node;
                
                return FSResult::SUCCESS;
            }
        }
        
        // Move to next cluster in directory
        FSResult nextResult = get_next_cluster(currentCluster, currentCluster);
        if (nextResult != FSResult::SUCCESS) {
            break;
        }
    }
    
    return FSResult::NOT_FOUND;
}

u32 FAT32::allocate_cluster() {
    // Start searching from cluster 2 (first data cluster)
    for (u32 cluster = 2; cluster < 0x0FFFFFF0; cluster++) {
        u32 fatOffset = cluster * 4; // 4 bytes per FAT32 entry
        u32 fatSector = m_fatStartSector + (fatOffset / 512);
        u32 entryOffset = (fatOffset % 512) / 4;
        
        // Load FAT sector
        FSResult result = load_fat_sector(fatSector);
        if (result != FSResult::SUCCESS) {
            return 0; // Failed to load FAT
        }
        
        // Check if cluster is free
        if ((m_fatCache[entryOffset] & 0x0FFFFFFF) == Fat32Cluster::FREE) {
            // Mark cluster as end of chain
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

} // namespace kira::fs 