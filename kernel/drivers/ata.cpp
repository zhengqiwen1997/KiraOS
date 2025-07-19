#include "drivers/ata.hpp"
#include "core/io.hpp"

namespace kira::drivers {

using namespace kira::system;

// Static member definitions
bool ATADriver::master_present = false;
bool ATADriver::slave_present = false;

bool ATADriver::initialize() {
    // Reset the primary ATA channel
    outb(PRIMARY_CONTROL, 0x04);  // Software reset
    delay_400ns();
    outb(PRIMARY_CONTROL, 0x00);  // Clear reset
    delay_400ns();
    
    // Check for master drive
    outb(PRIMARY_DRIVE_SELECT, static_cast<u8>(DriveType::MASTER));
    delay_400ns();
    
    // Send IDENTIFY command
    outb(PRIMARY_COMMAND, static_cast<u8>(Command::IDENTIFY));
    delay_400ns();
    
    // Check if drive exists
    u8 status = inb(PRIMARY_STATUS);
    if (status != 0) {
        // Drive exists, wait for it to be ready
        if (wait_ready()) {
            master_present = true;
        }
    }
    
    // Check for slave drive
    outb(PRIMARY_DRIVE_SELECT, static_cast<u8>(DriveType::SLAVE));
    delay_400ns();
    
    outb(PRIMARY_COMMAND, static_cast<u8>(Command::IDENTIFY));
    delay_400ns();
    
    status = inb(PRIMARY_STATUS);
    if (status != 0) {
        if (wait_ready()) {
            slave_present = true;
        }
    }
    
    return master_present || slave_present;
}

bool ATADriver::read_sectors(DriveType drive, u32 lba, u8 sectorCount, void* buffer) {
    if (!buffer || sectorCount == 0) {
        return false;
    }
    
    // Check if requested drive is present
    if (drive == DriveType::MASTER && !master_present) {
        return false;
    }
    if (drive == DriveType::SLAVE && !slave_present) {
        return false;
    }
    
    // Select drive and set up LBA
    if (!select_drive(drive)) {
        return false;
    }
    
    // Set up the read operation
    outb(PRIMARY_SECTOR_COUNT, sectorCount);
    outb(PRIMARY_LBA_LOW, lba & 0xFF);
    outb(PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(PRIMARY_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // Update drive select register with LBA bits 24-27
    u8 driveReg = static_cast<u8>(drive) | 0x40 | ((lba >> 24) & 0x0F);  // LBA mode
    outb(PRIMARY_DRIVE_SELECT, driveReg);
    delay_400ns();
    
    // Send read command
    outb(PRIMARY_COMMAND, static_cast<u8>(Command::READ_SECTORS));
    
    // Read each sector
    u8* byteBuffer = static_cast<u8*>(buffer);
    for (u8 i = 0; i < sectorCount; i++) {
        // Wait for data to be ready
        if (!wait_data_request()) {
            return false;
        }
        
        // Check for errors
        if (check_error()) {
            return false;
        }
        
        // Read 512 bytes
        read_data(byteBuffer + (i * SECTOR_SIZE));
    }
    
    return true;
}

bool ATADriver::write_sectors(DriveType drive, u32 lba, u8 sectorCount, const void* buffer) {
    if (!buffer || sectorCount == 0) {
        return false;
    }
    
    // Check if requested drive is present
    if (drive == DriveType::MASTER && !master_present) {
        return false;
    }
    if (drive == DriveType::SLAVE && !slave_present) {
        return false;
    }
    
    // Select drive and set up LBA
    if (!select_drive(drive)) {
        return false;
    }
    
    // Set up the write operation
    outb(PRIMARY_SECTOR_COUNT, sectorCount);
    outb(PRIMARY_LBA_LOW, lba & 0xFF);
    outb(PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(PRIMARY_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // Update drive select register with LBA bits 24-27
    u8 driveReg = static_cast<u8>(drive) | 0x40 | ((lba >> 24) & 0x0F);  // LBA mode
    outb(PRIMARY_DRIVE_SELECT, driveReg);
    delay_400ns();
    
    // Send write command
    outb(PRIMARY_COMMAND, static_cast<u8>(Command::WRITE_SECTORS));
    
    // Write each sector
    const u8* byteBuffer = static_cast<const u8*>(buffer);
    for (u8 i = 0; i < sectorCount; i++) {
        // Wait for drive to be ready for data
        if (!wait_data_request()) {
            return false;
        }
        
        // Check for errors
        if (check_error()) {
            return false;
        }
        
        // Write 512 bytes
        write_data(byteBuffer + (i * SECTOR_SIZE));
    }
    
    // Flush cache
    outb(PRIMARY_COMMAND, static_cast<u8>(Command::CACHE_FLUSH));
    if (!wait_ready()) {
        return false;
    }
    
    return true;
}

bool ATADriver::get_drive_info(DriveType drive, void* info) {
    if (!info) {
        return false;
    }
    
    // Check if requested drive is present
    if (drive == DriveType::MASTER && !master_present) {
        return false;
    }
    if (drive == DriveType::SLAVE && !slave_present) {
        return false;
    }
    
    if (!select_drive(drive)) {
        return false;
    }
    
    // Send IDENTIFY command
    outb(PRIMARY_COMMAND, static_cast<u8>(Command::IDENTIFY));
    
    // Wait for data
    if (!wait_data_request()) {
        return false;
    }
    
    if (check_error()) {
        return false;
    }
    
    // Read drive information
    read_data(info);
    
    return true;
}



// Private helper methods

bool ATADriver::wait_ready(u32 timeoutMs) {
    for (u32 i = 0; i < timeoutMs * 1000; i++) {  // Rough timing
        u8 status = inb(PRIMARY_STATUS);
        
        // Check if busy bit is clear and ready bit is set
        if (!(status & static_cast<u8>(Status::BSY)) && 
            (status & static_cast<u8>(Status::RDY))) {
            return true;
        }
        
        // Small delay
        delay_400ns();
    }
    
    return false;  // Timeout
}

bool ATADriver::wait_data_request(u32 timeoutMs) {
    for (u32 i = 0; i < timeoutMs * 1000; i++) {  // Rough timing
        u8 status = inb(PRIMARY_STATUS);
        
        // Check if busy bit is clear and data request bit is set
        if (!(status & static_cast<u8>(Status::BSY)) && 
            (status & static_cast<u8>(Status::DRQ))) {
            return true;
        }
        
        delay_400ns();
    }
    
    return false;  // Timeout
}

bool ATADriver::check_error() {
    u8 status = inb(PRIMARY_STATUS);
    return (status & static_cast<u8>(Status::ERR)) || 
           (status & static_cast<u8>(Status::DF));
}

bool ATADriver::select_drive(DriveType drive) {
    outb(PRIMARY_DRIVE_SELECT, static_cast<u8>(drive));
    delay_400ns();
    
    return wait_ready(100);  // Short timeout for drive selection
}

void ATADriver::read_data(void* buffer) {
    u16* wordBuffer = static_cast<u16*>(buffer);
    
    // Read 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        wordBuffer[i] = inw(PRIMARY_DATA);
    }
}

void ATADriver::write_data(const void* buffer) {
    const u16* wordBuffer = static_cast<const u16*>(buffer);
    
    // Write 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        outw(PRIMARY_DATA, wordBuffer[i]);
    }
}

void ATADriver::delay_400ns() {
    // Read status register 4 times for ~400ns delay
    for (int i = 0; i < 4; i++) {
        inb(PRIMARY_STATUS);
    }
}

} // namespace kira::drivers 