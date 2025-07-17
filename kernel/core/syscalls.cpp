#include "core/syscalls.hpp"
#include "arch/x86/idt.hpp"
#include "display/vga.hpp"
#include "display/console.hpp"
#include "core/process.hpp"

namespace kira::system {

using namespace kira::display;

} // namespace kira::system

// External console reference from kernel namespace  
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::system {

// Forward declaration for assembly stub (defined in syscall_stub.s)
extern "C" void syscall_stub();

/**
 * @brief C system call handler called from assembly stub
 */
extern "C" i32 syscall_handler(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3) {
    return handle_syscall(syscall_num, arg1, arg2, arg3);
}

i32 handle_syscall(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3) {
    VGADisplay vga;
    auto& pm = ProcessManager::get_instance();
    
    switch (static_cast<SystemCall>(syscall_num)) {
        case SystemCall::EXIT:
            // Terminate current process
            pm.terminate_current_process();
            
            // EXIT syscall should never return - either we switch to another process
            // or the system terminates. If we reach here, something went wrong.
            kira::kernel::console.add_message("ERROR: EXIT syscall returned - this should not happen!", VGA_RED_ON_BLUE);
            for(;;) { asm volatile("hlt"); } // Halt the system
            
        case SystemCall::WRITE: {
            // Legacy write system call (for backward compatibility)
            // arg1 = line, arg2 = column, arg3 = string pointer
            const char* str = reinterpret_cast<const char*>(arg3);
            if (!str) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            // Display the string via console system (ignore line/column for now)
            kira::kernel::console.add_message(str, VGA_WHITE_ON_BLUE);
            return static_cast<i32>(SyscallResult::SUCCESS);
        }
        
        case SystemCall::WRITE_COLORED: {
            // Enhanced write system call with color support
            // arg1 = string pointer, arg2 = color, arg3 = unused
            const char* str = reinterpret_cast<const char*>(arg1);
            if (!str) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            u16 color = static_cast<u16>(arg2);
            
            // Validate color (basic validation - ensure it's not 0)
            if (color == 0) {
                color = VGA_WHITE_ON_BLUE; // Default color
            }
            
            // Display the string via console system with specified color
            kira::kernel::console.add_message(str, color);
            return static_cast<i32>(SyscallResult::SUCCESS);
        }
        
        case SystemCall::YIELD:
            // Yield CPU to scheduler
            pm.yield();
            return static_cast<i32>(SyscallResult::SUCCESS);
            
        case SystemCall::GET_PID:
            // Return current process ID
            return pm.get_current_pid();
            
        case SystemCall::SLEEP:
            // Sleep for specified ticks
            pm.sleep_current_process(arg1);
            return static_cast<i32>(SyscallResult::SUCCESS);
            
        default:
            return static_cast<i32>(SyscallResult::INVALID_SYSCALL);
    }
}

void initialize_syscalls() {
    // Set up system call interrupt (INT 0x80) - accessible from Ring 3
    IDT::set_user_interrupt_gate(0x80, (void*)syscall_stub);
}

} // namespace kira::system 