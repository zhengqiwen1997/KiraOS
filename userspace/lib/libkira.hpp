#pragma once

#include "core/types.hpp"

namespace kira::usermode {

using namespace kira::system;  // Import types

// Color constants for user programs (matching VGA colors)
namespace Colors {
    constexpr u16 WHITE_ON_BLUE = 0x1F00;
    constexpr u16 YELLOW_ON_BLUE = 0x1E00;
    constexpr u16 GREEN_ON_BLUE = 0x1A00;
    constexpr u16 RED_ON_BLUE = 0x1C00;
    constexpr u16 CYAN_ON_BLUE = 0x1B00;
    constexpr u16 MAGENTA_ON_BLUE = 0x1D00;
    constexpr u16 LIGHT_GRAY_ON_BLUE = 0x1700;
    constexpr u16 BLACK_ON_CYAN = 0x3000;
    constexpr u16 DEFAULT = WHITE_ON_BLUE;
}

// File system constants and structures
namespace FileSystem {
    // File types
    enum class FileType : u8 {
        REGULAR = 0,
        DIRECTORY = 1,
        DEVICE = 2,
        SYMLINK = 3
    };
    
    // Open flags
    enum class OpenFlags : u32 {
        READ_ONLY = 0x00,
        WRITE_ONLY = 0x01,
        READ_WRITE = 0x02,
        CREATE = 0x40,
        TRUNCATE = 0x200,
        APPEND = 0x400
    };
    
    // Directory entry structure (matches kernel VFS)
    struct DirectoryEntry {
        char name[256];     // File name
        u32 inode;         // Inode number
        FileType type;     // File type
    } __attribute__((packed));
}

/**
 * @brief User mode system call interface
 * This is the userspace library (like libc) that provides system call wrappers
 */
class UserAPI {
public:
    /**
     * @brief Write text to console (simple version)
     * @param text Text to display
     * @return 0 on success, negative on error
     */
    static i32 print(const char* text);
    
    /**
     * @brief Write text to console with newline
     * @param text Text to display
     * @return 0 on success, negative on error
     */
    static i32 println(const char* text);
    
    /**
     * @brief Write text to console with color
     * @param text Text to display
     * @param color Color constant from Colors namespace
     * @return 0 on success, negative on error
     */
    static i32 print_colored(const char* text, u16 color);
    
    /**
     * @brief Formatted print to console (printf-style)
     * @param format Format string with %d, %u, %x, %X, %s, %c specifiers
     * @param ... Variable arguments for format specifiers
     * @return 0 on success, negative on error
     */
    static i32 printf(const char* format, ...);
    
    /**
     * @brief Write text to screen at specified position (legacy)
     * @param line Line number (0-24) - currently ignored, uses console
     * @param column Column number (0-79) - currently ignored, uses console
     * @param text Text to display
     * @return 0 on success, negative on error
     */
    static i32 write(u32 line, u32 column, const char* text);
    
    /**
     * @brief Write text to console with color and formatting
     * @param text Text to display
     * @param color Color constant from Colors namespace
     * @return 0 on success, negative on error
     */
    static i32 write_colored(const char* text, u16 color);
    
    /**
     * @brief Get current process ID
     * @return Process ID
     */
    static u32 get_pid();
    
    /**
     * @brief Yield CPU to other processes
     * @return 0 on success
     */
    static i32 yield();
    
    /**
     * @brief Sleep for specified number of ticks
     * @param ticks Number of timer ticks to sleep
     * @return 0 on success
     */
    static i32 sleep(u32 ticks);
    
    /**
     * @brief Exit current process
     * @return Does not return
     */
    static void exit();
    
    // File system operations
    /**
     * @brief Open file or directory
     * @param path Path to file or directory
     * @param flags Open flags (read/write/create etc.)
     * @return File descriptor on success, negative error code on failure
     */
    static i32 open(const char* path, u32 flags);
    
    /**
     * @brief Close file descriptor
     * @param fd File descriptor to close
     * @return 0 on success, negative error code on failure
     */
    static i32 close(i32 fd);
    
    /**
     * @brief Read from file
     * @param fd File descriptor
     * @param buffer Buffer to read into
     * @param size Number of bytes to read
     * @return Number of bytes read on success, negative error code on failure
     */
    static i32 read_file(i32 fd, void* buffer, u32 size);
    
    /**
     * @brief Write to file
     * @param fd File descriptor
     * @param buffer Buffer to write from
     * @param size Number of bytes to write
     * @return Number of bytes written on success, negative error code on failure
     */
    static i32 write_file(i32 fd, const void* buffer, u32 size);
    
    /**
     * @brief Read directory entry
     * @param path Directory path
     * @param index Entry index (0-based)
     * @param entry Buffer to store directory entry
     * @return 0 on success, negative error code on failure
     */
    static i32 readdir(const char* path, u32 index, void* entry);
    
    /**
     * @brief Create directory
     * @param path Path to new directory
     * @return 0 on success, negative error code on failure
     */
    static i32 mkdir(const char* path);
    
    // Process management operations
    /**
     * @brief List processes (get current process info)
     * @return Current process ID
     */
    static i32 ps();
    
    /**
     * @brief Terminate process by PID
     * @param pid Process ID to terminate
     * @return 0 on success, negative error code on failure
     */
    static i32 kill(u32 pid);

private:
    /**
     * @brief Low-level system call interface
     * @param syscallNum System call number
     * @param arg1 First argument
     * @param arg2 Second argument
     * @param arg3 Third argument
     * @return System call result
     */
    static i32 syscall(u32 syscallNum, u32 arg1 = 0, u32 arg2 = 0, u32 arg3 = 0);
};

} // namespace kira::usermode 