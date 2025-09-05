#pragma once

#include "core/types.hpp"

namespace kira::system {

/**
 * @brief System call numbers
 */
enum class SystemCall : u32 {
    EXIT = 0,           // Exit process
    WRITE = 1,          // Write to display (legacy)
    READ = 2,           // Read from keyboard
    YIELD = 3,          // Yield CPU to scheduler
    GET_PID = 4,        // Get current process ID
    SLEEP = 5,          // Sleep for specified ticks
    WRITE_COLORED = 6,  // Write to display with color (auto-newline)
    WRITE_PRINTF = 7,   // Write to display with printf-style behavior (explicit newlines only)
    
    // File system operations
    OPEN = 8,           // Open file or directory
    CLOSE = 9,          // Close file descriptor  
    READ_FILE = 10,     // Read from file
    WRITE_FILE = 11,    // Write to file
    STAT = 12,          // Get file/directory information
    READDIR = 13,       // Read directory entries
    MKDIR = 14,         // Create directory
    RMDIR = 15,         // Remove directory
    CHDIR = 16,         // Change working directory
    GETCWD = 17,        // Get current working directory
    
    // Process management operations
    EXEC = 18,          // Execute program
    KILL = 19,          // Terminate process
    PS = 20,            // List processes
    WAIT = 21,          // Wait for child process
    GETCH = 22,         // Get one character from keyboard (blocking)
    TRYGETCH = 23,      // Non-blocking get character (0 if none)
    SPAWN = 24,         // Spawn a builtin user program by id (temporary)
    GETCWD_PTR = 25,    // Get pointer to current process CWD (temporary)
    GETSPAWNARG = 26,   // Copy spawn argument string into user buffer
    WAITID = 27,        // Wait for child by pid (0 = any); write status to user ptr
    FORK = 28,          // Fork current process
    SBRK = 29,          // Adjust program break by increment (returns old break)
    BRK = 30,           // Set program break to absolute address (returns new break)
};

/**
 * @brief System call result codes
 */
enum class SyscallResult : i32 {
    SUCCESS = 0,
    INVALID_SYSCALL = -1,
    INVALID_PARAMETER = -2,
    PERMISSION_DENIED = -3,
    RESOURCE_UNAVAILABLE = -4,
    FILE_NOT_FOUND = -5,
    FILE_EXISTS = -6,
    IO_ERROR = -7,
    NO_SPACE = -8,
    TOO_MANY_FILES = -9,
    NOT_DIRECTORY = -10,
    IS_DIRECTORY = -11,
};

/**
 * @brief System call handler
 * Processes system calls from user mode programs
 * 
 * @param syscallNum System call number
 * @param arg1 First argument
 * @param arg2 Second argument  
 * @param arg3 Third argument
 * @return System call result
 */
i32 handle_syscall(u32 syscallNum, u32 arg1, u32 arg2, u32 arg3);

/**
 * @brief Initialize system call handling
 * Sets up interrupt handler for system calls (INT 0x80)
 */
void initialize_syscalls();

} // namespace kira::system 