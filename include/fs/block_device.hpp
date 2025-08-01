#pragma once

#include "core/types.hpp"
#include "fs/vfs.hpp"

namespace kira::fs {

using namespace kira::system;

/**
 * @brief Block Device Interface
 * 
 * Abstract interface for block-based storage devices.
 * Provides a standard interface for file systems to access storage.
 */
class BlockDevice {
public:
    virtual ~BlockDevice() = default;

    /**
     * @brief Read blocks from device
     * @param blockNum Starting block number
     * @param blockCount Number of blocks to read
     * @param buffer Buffer to store read data
     * @return FSResult::SUCCESS on success
     */
    virtual FSResult read_blocks(u32 blockNum, u32 blockCount, void* buffer) = 0;

    /**
     * @brief Write blocks to device
     * @param blockNum Starting block number
     * @param blockCount Number of blocks to write
     * @param buffer Buffer containing data to write
     * @return FSResult::SUCCESS on success
     */
    virtual FSResult write_blocks(u32 blockNum, u32 blockCount, const void* buffer) = 0;

    /**
     * @brief Get device information
     */
    virtual u32 get_block_size() const = 0;
    virtual u32 get_block_count() const = 0;
    virtual bool is_read_only() const = 0;

    /**
     * @brief Flush any cached data to device
     * @return FSResult::SUCCESS on success
     */
    virtual FSResult flush() = 0;
};

/**
 * @brief ATA Block Device Wrapper
 * 
 * Wraps the ATA driver to provide a block device interface for file systems.
 */
class ATABlockDevice : public BlockDevice {
public:
    /**
     * @brief Constructor
     * @param driveType ATA drive type (MASTER/SLAVE)
     */
    explicit ATABlockDevice(u8 driveType);
    virtual ~ATABlockDevice() = default;

    // BlockDevice interface
    FSResult read_blocks(u32 blockNum, u32 blockCount, void* buffer) override;
    FSResult write_blocks(u32 blockNum, u32 blockCount, const void* buffer) override;
    u32 get_block_size() const override;
    u32 get_block_count() const override;
    bool is_read_only() const override;
    FSResult flush() override;

    /**
     * @brief Initialize the ATA block device
     * @return FSResult::SUCCESS if device is available
     */
    FSResult initialize();

private:
    u8 m_driveType;          // ATA drive type (MASTER/SLAVE)
    bool m_initialized;       // Device initialization status
    u32 m_blockCount;        // Total number of blocks
    bool m_readOnly;         // Read-only status

    static constexpr u32 BLOCK_SIZE = 512;  // Standard sector size
};

/**
 * @brief Block Device Manager
 * 
 * Manages registration and access to block devices.
 */
class BlockDeviceManager {
public:
    static BlockDeviceManager& get_instance();

    /**
     * @brief Register a block device
     * @param device Block device to register
     * @param deviceName Name for the device (e.g., "hda", "hdb")
     * @return Device ID on success, -1 on failure
     */
    i32 register_device(BlockDevice* device, const char* deviceName);

    /**
     * @brief Get device by ID
     * @param deviceId Device ID returned by register_device
     * @return Pointer to device, nullptr if not found
     */
    BlockDevice* get_device(i32 deviceId);

    /**
     * @brief Get device by name
     * @param deviceName Device name
     * @return Pointer to device, nullptr if not found
     */
    BlockDevice* get_device(const char* deviceName);

    /**
     * @brief Initialize all registered devices
     * @return Number of successfully initialized devices
     */
    u32 initialize_devices();

private:
    BlockDeviceManager() = default;
    ~BlockDeviceManager() = default;
    BlockDeviceManager(const BlockDeviceManager&) = delete;
    BlockDeviceManager& operator=(const BlockDeviceManager&) = delete;

    struct DeviceEntry {
        BlockDevice* device;
        char name[16];
        bool active;
    };

    static BlockDeviceManager* s_instance;
    static constexpr u32 MAX_DEVICES = 8;

    u32 m_deviceCount;
    DeviceEntry m_devices[MAX_DEVICES];
};

} // namespace kira::fs 