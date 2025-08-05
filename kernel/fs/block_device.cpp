#include "fs/block_device.hpp"
#include "drivers/ata.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"
#include "display/console.hpp"

// External console reference from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}


namespace kira::fs {

using namespace kira::system;
using namespace kira::drivers;
using namespace kira::utils;
using namespace kira::display;

// Static member definitions
BlockDeviceManager* BlockDeviceManager::s_instance = nullptr;

//=============================================================================
// ATABlockDevice Implementation
//=============================================================================

ATABlockDevice::ATABlockDevice(u8 driveType)
    : m_driveType(driveType), m_initialized(false), m_blockCount(0), m_readOnly(false) {
}

FSResult ATABlockDevice::initialize() {
    // Initialize ATA driver if not already done
    if (!ATADriver::initialize()) {
        return FSResult::IO_ERROR;
    }

    // Check if our specific drive is present
    ATADriver::DriveType ataDrive = (m_driveType == 0) ? 
        ATADriver::DriveType::MASTER : ATADriver::DriveType::SLAVE;

    // Try to get drive information to verify it exists
    auto& memMgr = MemoryManager::get_instance();
    void* infoBuffer = memMgr.allocate_physical_page();
    if (!infoBuffer) {
        return FSResult::NO_SPACE;
    }

    bool driveExists = ATADriver::get_drive_info(ataDrive, infoBuffer);
    memMgr.free_physical_page(infoBuffer);

    if (!driveExists) {
        return FSResult::NOT_FOUND;
    }

    // For now, assume a reasonable disk size (we could parse IDENTIFY data later)
    // Most test environments will have small virtual disks
    m_blockCount = 20480;  // 10MB worth of 512-byte sectors
    m_readOnly = false;    // Assume writable
    m_initialized = true;

    return FSResult::SUCCESS;
}

FSResult ATABlockDevice::read_blocks(u32 blockNum, u32 blockCount, void* buffer) {
    if (!m_initialized) {
        return FSResult::IO_ERROR;
    }

    if (!buffer || blockCount == 0) {
        return FSResult::INVALID_PARAMETER;
    }

    if (blockNum + blockCount > m_blockCount) {
        return FSResult::INVALID_PARAMETER;
    }

    ATADriver::DriveType ataDrive = (m_driveType == 0) ? 
        ATADriver::DriveType::MASTER : ATADriver::DriveType::SLAVE;

    // ATA driver expects sector count as u8, so we need to handle large reads
    u8* byteBuffer = static_cast<u8*>(buffer);
    u32 blocksRemaining = blockCount;
    u32 currentBlock = blockNum;

    // k_printf("[ATABlockDevice] blocksRemaining: %d, m_blockCount: %d\n", blocksRemaining, m_blockCount);

    while (blocksRemaining > 0) {
        u32 blocksToRead = (blocksRemaining > 255) ? 255 : blocksRemaining;
        // k_printf("[ATABlockDevice] blocksToRead: %d, currentBlock: %d\n", blocksToRead, currentBlock);
        bool success = ATADriver::read_sectors(
            ataDrive, 
            currentBlock, 
            static_cast<u8>(blocksToRead), 
            byteBuffer
        );

        if (!success) {
            return FSResult::IO_ERROR;
        }

        byteBuffer += blocksToRead * BLOCK_SIZE;
        currentBlock += blocksToRead;
        blocksRemaining -= blocksToRead;
    }

    return FSResult::SUCCESS;
}

FSResult ATABlockDevice::write_blocks(u32 blockNum, u32 blockCount, const void* buffer) {
    // kira::kernel::console.add_message("[ATABlockDevice] write_blocks start", VGA_GREEN_ON_BLUE);
    if (!m_initialized) {
        kira::kernel::console.add_message("[ATABlockDevice] not initialized", VGA_RED_ON_BLUE);

        return FSResult::IO_ERROR;
    }
    

    if (m_readOnly) {
        kira::kernel::console.add_message("[ATABlockDevice] read only", VGA_RED_ON_BLUE);
        return FSResult::PERMISSION_DENIED;
    }

    if (!buffer || blockCount == 0) {
        kira::kernel::console.add_message("[ATABlockDevice] invalid parameter", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }

    // k_printf("blockNum: %d, blockCount: %d, m_blockCount: %d\n", blockNum, blockCount, m_blockCount);

    if (blockNum + blockCount > m_blockCount) {
        kira::kernel::console.add_message("[ATABlockDevice] invalid blockNum + blockCount > m_blockCoun", VGA_RED_ON_BLUE);
        return FSResult::INVALID_PARAMETER;
    }

    ATADriver::DriveType ataDrive = (m_driveType == 0) ? 
        ATADriver::DriveType::MASTER : ATADriver::DriveType::SLAVE;

    // Handle large writes similar to reads
    const u8* byteBuffer = static_cast<const u8*>(buffer);
    u32 blocksRemaining = blockCount;
    u32 currentBlock = blockNum;

    while (blocksRemaining > 0) {
        u32 blocksToWrite = (blocksRemaining > 255) ? 255 : blocksRemaining;
        
        bool success = ATADriver::write_sectors(
            ataDrive, 
            currentBlock, 
            static_cast<u8>(blocksToWrite), 
            byteBuffer
        );

        if (!success) {
            return FSResult::IO_ERROR;
        }

        byteBuffer += blocksToWrite * BLOCK_SIZE;
        currentBlock += blocksToWrite;
        blocksRemaining -= blocksToWrite;
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

i32 BlockDeviceManager::register_device(BlockDevice* device, const char* deviceName) {
    if (!device || !deviceName || m_deviceCount >= MAX_DEVICES) {
        return -1;
    }

    // Check for duplicate names
    for (u32 i = 0; i < m_deviceCount; i++) {
        if (m_devices[i].active && strcmp(m_devices[i].name, deviceName) == 0) {
            return -1;  // Name already exists
        }
    }

    // Find free slot
    for (u32 i = 0; i < MAX_DEVICES; i++) {
        if (!m_devices[i].active) {
            m_devices[i].device = device;
            strcpy_s(m_devices[i].name, deviceName, sizeof(m_devices[i].name));
            m_devices[i].active = true;
            
            if (i >= m_deviceCount) {
                m_deviceCount = i + 1;
            }
            
            return static_cast<i32>(i);
        }
    }

    return -1;  // No free slots
}

BlockDevice* BlockDeviceManager::get_device(i32 deviceId) {
    if (deviceId < 0 || static_cast<u32>(deviceId) >= m_deviceCount) {
        return nullptr;
    }

    if (!m_devices[deviceId].active) {
        return nullptr;
    }

    return m_devices[deviceId].device;
}

BlockDevice* BlockDeviceManager::get_device(const char* deviceName) {
    if (!deviceName) {
        return nullptr;
    }

    for (u32 i = 0; i < m_deviceCount; i++) {
        if (m_devices[i].active && strcmp(m_devices[i].name, deviceName) == 0) {
            return m_devices[i].device;
        }
    }

    return nullptr;
}

u32 BlockDeviceManager::initialize_devices() {
    u32 initializedCount = 0;

    for (u32 i = 0; i < m_deviceCount; i++) {
        if (!m_devices[i].active) {
            continue;
        }

        // Try to initialize the device
        // For now, assume all devices are ATA devices and need initialization
        // In a more complete implementation, we'd have a device type field
        ATABlockDevice* ataDevice = static_cast<ATABlockDevice*>(m_devices[i].device);
        if (ataDevice->initialize() == FSResult::SUCCESS) {
            initializedCount++;
        }
    }

    return initializedCount;
}

} // namespace kira::fs 