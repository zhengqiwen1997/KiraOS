#pragma once

#include "core/types.hpp"

namespace kira::drivers {

using namespace kira::system;

/**
 * @brief ATA/IDE Hard Disk Driver
 * 
 * Provides basic read/write operations for ATA/IDE hard disks.
 * Supports Primary and Secondary IDE channels with Master/Slave drives.
 */
class ATADriver {
public:
    // ATA drive identification
    enum class DriveType : u8 {
        MASTER = 0xA0,
        SLAVE = 0xB0
    };
    
    // ATA command codes
    enum class Command : u8 {
        READ_SECTORS = 0x20,
        WRITE_SECTORS = 0x30,
        IDENTIFY = 0xEC,
        CACHE_FLUSH = 0xE7
    };
    
    // ATA status register bits
    enum class Status : u8 {
        ERR = 0x01,    // Error
        DRQ = 0x08,    // Data Request
        SRV = 0x10,    // Service Request
        DF = 0x20,     // Drive Fault
        RDY = 0x40,    // Ready
        BSY = 0x80     // Busy
    };
    
    /**
     * @brief Initialize the ATA driver
     * @return true if at least one drive was detected
     */
    static bool initialize();
    
    /**
     * @brief Read sectors from disk
     * @param drive Drive to read from (MASTER/SLAVE)
     * @param lba Logical Block Address (28-bit LBA)
     * @param sectorCount Number of sectors to read (1-256)
     * @param buffer Buffer to store read data (must be sectorCount * 512 bytes)
     * @return true on success, false on error
     */
    static bool read_sectors(DriveType drive, u32 lba, u8 sectorCount, void* buffer);
    
    /**
     * @brief Write sectors to disk
     * @param drive Drive to write to (MASTER/SLAVE)
     * @param lba Logical Block Address (28-bit LBA)
     * @param sectorCount Number of sectors to write (1-256)
     * @param buffer Buffer containing data to write
     * @return true on success, false on error
     */
    static bool write_sectors(DriveType drive, u32 lba, u8 sectorCount, const void* buffer);
    
    /**
     * @brief Get drive information
     * @param drive Drive to query
     * @param info Buffer to store drive information (512 bytes)
     * @return true on success, false if drive not present
     */
    static bool get_drive_info(DriveType drive, void* info);
    
    // Drive presence getters
    static bool is_master_present() { return master_present; }
    static bool is_slave_present() { return slave_present; }
    
private:
    // Primary ATA channel I/O ports
    static constexpr u16 PRIMARY_DATA = 0x1F0;
    static constexpr u16 PRIMARY_ERROR = 0x1F1;
    static constexpr u16 PRIMARY_SECTOR_COUNT = 0x1F2;
    static constexpr u16 PRIMARY_LBA_LOW = 0x1F3;
    static constexpr u16 PRIMARY_LBA_MID = 0x1F4;
    static constexpr u16 PRIMARY_LBA_HIGH = 0x1F5;
    static constexpr u16 PRIMARY_DRIVE_SELECT = 0x1F6;
    static constexpr u16 PRIMARY_COMMAND = 0x1F7;
    static constexpr u16 PRIMARY_STATUS = 0x1F7;
    static constexpr u16 PRIMARY_CONTROL = 0x3F6;
    
    // Secondary ATA channel I/O ports (for future expansion)
    static constexpr u16 SECONDARY_DATA = 0x170;
    static constexpr u16 SECONDARY_COMMAND = 0x177;
    static constexpr u16 SECONDARY_STATUS = 0x177;
    static constexpr u16 SECONDARY_CONTROL = 0x376;
    
    // Timing constants
    static constexpr u32 TIMEOUT_MS = 1000;
    static constexpr u32 SECTOR_SIZE = 512;
    
    // Drive detection status
    static bool master_present;
    static bool slave_present;
    
    /**
     * @brief Wait for drive to be ready
     * @param timeoutMs Timeout in milliseconds
     * @return true if ready, false on timeout
     */
    static bool wait_ready(u32 timeoutMs = TIMEOUT_MS);
    
    /**
     * @brief Wait for data request
     * @param timeoutMs Timeout in milliseconds
     * @return true if data ready, false on timeout
     */
    static bool wait_data_request(u32 timeoutMs = TIMEOUT_MS);
    
    /**
     * @brief Check for errors in status register
     * @return true if error detected
     */
    static bool check_error();
    
    /**
     * @brief Select drive and wait for ready
     * @param drive Drive to select
     * @return true on success
     */
    static bool select_drive(DriveType drive);
    
    /**
     * @brief Read data from drive (512 bytes)
     * @param buffer Buffer to store data
     */
    static void read_data(void* buffer);
    
    /**
     * @brief Write data to drive (512 bytes)
     * @param buffer Buffer containing data
     */
    static void write_data(const void* buffer);
    
    /**
     * @brief Simple delay for ATA timing
     */
    static void delay_400ns();
};

} // namespace kira::drivers 