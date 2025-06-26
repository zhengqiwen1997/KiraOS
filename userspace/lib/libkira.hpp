#pragma once

#include "core/types.hpp"

namespace kira::usermode {

using namespace kira::system;  // Import types

/**
 * @brief User mode system call interface
 * This is the userspace library (like libc) that provides system call wrappers
 */
class UserAPI {
public:
    /**
     * @brief Write text to screen at specified position
     * @param line Line number (0-24)
     * @param column Column number (0-79)
     * @param text Text to display
     * @return 0 on success, negative on error
     */
    static i32 write(u32 line, u32 column, const char* text);
    
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

private:
    /**
     * @brief Make system call
     * @param syscall_num System call number
     * @param arg1 First argument
     * @param arg2 Second argument
     * @param arg3 Third argument
     * @return System call result
     */
    static i32 syscall(u32 syscall_num, u32 arg1 = 0, u32 arg2 = 0, u32 arg3 = 0);
};

} // namespace kira::usermode 