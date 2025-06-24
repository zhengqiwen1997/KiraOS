#pragma once

#include "core/types.hpp"

namespace kira::system {

/**
 * @brief System call numbers
 */
enum class SystemCall : u32 {
    EXIT = 0,           // Exit process
    WRITE = 1,          // Write to display
    READ = 2,           // Read from keyboard
    YIELD = 3,          // Yield CPU to scheduler
    GET_PID = 4,        // Get current process ID
    SLEEP = 5,          // Sleep for specified ticks
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
};

/**
 * @brief System call handler
 * Processes system calls from user mode programs
 * 
 * @param syscall_num System call number
 * @param arg1 First argument
 * @param arg2 Second argument  
 * @param arg3 Third argument
 * @return System call result
 */
i32 handle_syscall(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3);

/**
 * @brief Initialize system call handling
 * Sets up interrupt handler for system calls (INT 0x80)
 */
void initialize_syscalls();

} // namespace kira::system 