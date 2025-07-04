#include "core/syscalls.hpp"
#include "arch/x86/idt.hpp"
#include "display/vga.hpp"
#include "core/process.hpp"

namespace kira::system {

using namespace kira::display;

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
            return static_cast<i32>(SyscallResult::SUCCESS);
            
        case SystemCall::WRITE: {
            // Write string to display at specified position
            // arg1 = line, arg2 = column, arg3 = string pointer
            if (arg1 >= 25 || arg2 >= 80) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            const char* str = reinterpret_cast<const char*>(arg3);
            if (!str) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            // vga.print_string(arg1, arg2, str, VGA_WHITE_ON_BLUE);  // Disabled to avoid overwriting console
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