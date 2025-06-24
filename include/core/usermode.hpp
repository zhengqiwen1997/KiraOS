#pragma once

#include "core/types.hpp"

namespace kira::system {

/**
 * @brief User mode transition and management
 */
class UserMode {
public:
    /**
     * @brief Switch to user mode and execute function
     * @param function Function to execute in user mode
     * @param user_stack User mode stack pointer
     */
    static void switch_to_user_mode(void* function, u32 user_stack);
    
    /**
     * @brief Set up user mode stack for a process
     * @param stack_base Base address of stack
     * @param stack_size Size of stack
     * @return User mode stack pointer (top of stack)
     */
    static u32 setup_user_stack(u32 stack_base, u32 stack_size);
    
    /**
     * @brief Check if currently in user mode
     * @return true if in user mode (Ring 3)
     */
    static bool is_user_mode();
    
    /**
     * @brief Get current kernel stack pointer
     * @return Current ESP value
     */
    static u32 get_current_kernel_stack();
    
private:
    // User mode segment selectors
    static constexpr u16 USER_CODE_SELECTOR = 0x1B;  // GDT entry 3, Ring 3
    static constexpr u16 USER_DATA_SELECTOR = 0x23;  // GDT entry 4, Ring 3
};

} // namespace kira::system 