#include "fs/block_device.hpp"
#include "drivers/ata.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"

namespace kira::fs {

using namespace kira::system;
using namespace kira::drivers;
using namespace kira::utils;

// Static member definitions
BlockDeviceManager* BlockDeviceManager::s_instance = nullptr;

//=============================================================================
// ATABlockDevice Implementation
//=============================================================================

ATABlockDevice::ATABlockDevice(u8 drive_type)
    : m_driveType(drive_type), m_initialized(false), m_blockCount(0), m_readOnly(false) {
}

FSResult ATABlockDevice::initialize() {
    // Initialize ATA driver if not already done
    if (!ATADriver::initialize()) {
        return FSResult::IO_ERROR;
    }

    // Check if our specific drive is present
    ATADriver::DriveType ata_drive = (m_driveType == 0) ? 
        ATADriver::DriveType::MASTER : ATADriver::DriveType::SLAVE;

    // Try to get drive information to verify it exists
    auto& memMgr = MemoryManager::get_instance();
    void* info_buffer = memMgr.allocate_physical_page();
    if (!info_buffer) {
        return FSResult::NO_SPACE;
    }

    bool drive_exists = ATADriver::get_drive_info(ata_drive, info_buffer);
    memMgr.free_physical_page(info_buffer);

    if (!drive_exists) {
        return FSResult::NOT_FOUND;
    }

    // For now, assume a reasonable disk size (we could parse IDENTIFY data later)
    // Most test environments will have small virtual disks
    m_blockCount = 20480;  // 10MB worth of 512-byte sectors
    m_readOnly = false;    // Assume writable
    m_initialized = true;

    return FSResult::SUCCESS;
}

FSResult ATABlockDevice::read_blocks(u32 block_num, u32 block_count, void* buffer) {
    if (!m_initialized) {
        return FSResult::IO_ERROR;
    }

    if (!buffer || block_count == 0) {
        return FSResult::INVALID_PARAMETER;
    }

    if (block_num + block_count > m_blockCount) {
        return FSResult::INVALID_PARAMETER;
    }

    ATADriver::DriveType ata_drive = (m_driveType == 0) ? 
        ATADriver::DriveType::MASTER : ATADriver::DriveType::SLAVE;

    // ATA driver expects sector count as u8, so we need to handle large reads
    u8* byte_buffer = static_cast<u8*>(buffer);
    u32 blocks_remaining = block_count;
    u32 current_block = block_num;

    while (blocks_remaining > 0) {
        u32 blocks_to_read = (blocks_remaining > 255) ? 255 : blocks_remaining;
        
        bool success = ATADriver::read_sectors(
            ata_drive, 
            current_block, 
            static_cast<u8>(blocks_to_read), 
            byte_buffer
        );

        if (!success) {
            return FSResult::IO_ERROR;
        }

        byte_buffer += blocks_to_read * BLOCK_SIZE;
        current_block += blocks_to_read;
        blocks_remaining -= blocks_to_read;
    }

    return FSResult::SUCCESS;
}

FSResult ATABlockDevice::write_blocks(u32 block_num, u32 block_count, const void* buffer) {
    if (!m_initialized) {
        return FSResult::IO_ERROR;
    }

    if (m_readOnly) {
        return FSResult::PERMISSION_DENIED;
    }

    if (!buffer || block_count == 0) {
        return FSResult::INVALID_PARAMETER;
    }

    if (block_num + block_count > m_blockCount) {
        return FSResult::INVALID_PARAMETER;
    }

    ATADriver::DriveType ata_drive = (m_driveType == 0) ? 
        ATADriver::DriveType::MASTER : ATADriver::DriveType::SLAVE;

    // Handle large writes similar to reads
    const u8* byte_buffer = static_cast<const u8*>(buffer);
    u32 blocks_remaining = block_count;
    u32 current_block = block_num;

    while (blocks_remaining > 0) {
        u32 blocks_to_write = (blocks_remaining > 255) ? 255 : blocks_remaining;
        
        bool success = ATADriver::write_sectors(
            ata_drive, 
            current_block, 
            static_cast<u8>(blocks_to_write), 
            byte_buffer
        );

        if (!success) {
            return FSResult::IO_ERROR;
        }

        byte_buffer += blocks_to_write * BLOCK_SIZE;
        current_block += blocks_to_write;
        blocks_remaining -= blocks_to_write;
    }

    return FSResult::SUCCESS;
}

u32 ATABlockDevice::get_block_size() const {
    return BLOCK_SIZE;
}

u32 ATABlockDevice::get_block_count() const {
    return m_blockCount;
}

bool ATABlockDevice::is_read_only() const {
    return m_readOnly;
}

FSResult ATABlockDevice::flush() {
    // ATA driver handles flushing automatically in write operations
    return FSResult::SUCCESS;
}

//=============================================================================
// BlockDeviceManager Implementation
//=============================================================================

BlockDeviceManager& BlockDeviceManager::get_instance() {
    if (!s_instance) {
        auto& memMgr = MemoryManager::get_instance();
        void* memory = memMgr.allocate_physical_page();
        if (memory) {
            s_instance = new(memory) BlockDeviceManager();
        }
    }
    return *s_instance;
}

i32 BlockDeviceManager::register_device(BlockDevice* device, const char* device_name) {
    if (!device || !device_name || m_deviceCount >= MAX_DEVICES) {
        return -1;
    }

    // Check for duplicate names
    for (u32 i = 0; i < m_deviceCount; i++) {
        if (m_devices[i].active && strcmp(m_devices[i].name, device_name) == 0) {
            return -1;  // Name already exists
        }
    }

    // Find free slot
    for (u32 i = 0; i < MAX_DEVICES; i++) {
        if (!m_devices[i].active) {
            m_devices[i].device = device;
            strcpy_s(m_devices[i].name, device_name, sizeof(m_devices[i].name));
            m_devices[i].active = true;
            
            if (i >= m_deviceCount) {
                m_deviceCount = i + 1;
            }
            
            return static_cast<i32>(i);
        }
    }

    return -1;  // No free slots
}

BlockDevice* BlockDeviceManager::get_device(i32 device_id) {
    if (device_id < 0 || static_cast<u32>(device_id) >= m_deviceCount) {
        return nullptr;
    }

    if (!m_devices[device_id].active) {
        return nullptr;
    }

    return m_devices[device_id].device;
}

BlockDevice* BlockDeviceManager::get_device(const char* device_name) {
    if (!device_name) {
        return nullptr;
    }

    for (u32 i = 0; i < m_deviceCount; i++) {
        if (m_devices[i].active && strcmp(m_devices[i].name, device_name) == 0) {
            return m_devices[i].device;
        }
    }

    return nullptr;
}

u32 BlockDeviceManager::initialize_devices() {
    u32 initialized_count = 0;

    for (u32 i = 0; i < m_deviceCount; i++) {
        if (!m_devices[i].active) {
            continue;
        }

        // Try to initialize the device
        // For now, assume all devices are ATA devices and need initialization
        // In a more complete implementation, we'd have a device type field
        ATABlockDevice* ata_device = static_cast<ATABlockDevice*>(m_devices[i].device);
        if (ata_device->initialize() == FSResult::SUCCESS) {
            initialized_count++;
        }
    }

    return initialized_count;
}

} // namespace kira::fs 