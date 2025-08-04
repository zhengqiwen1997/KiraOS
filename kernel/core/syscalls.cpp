#include "core/syscalls.hpp"
#include "arch/x86/idt.hpp"
#include "display/vga.hpp"
#include "display/console.hpp"
#include "core/process.hpp"
#include "fs/vfs.hpp"

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
            // Enhanced write system call with color support (auto-newline)
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
        
        case SystemCall::WRITE_PRINTF: {
            // Printf-style write system call (explicit newlines only)
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
            
            // Display the string via printf-style console output (explicit newlines only)
            kira::kernel::console.add_printf_output(str, color);
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
            
        // File system operations
        case SystemCall::OPEN: {
            // Open file or directory
            // arg1 = path pointer, arg2 = flags, arg3 = unused
            const char* path = reinterpret_cast<const char*>(arg1);
            if (!path) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            auto& vfs = kira::fs::VFS::get_instance();
            i32 fd;
            kira::fs::FSResult result = vfs.open(path, static_cast<kira::fs::OpenFlags>(arg2), fd);
            
            if (result == kira::fs::FSResult::SUCCESS) {
                return fd; // Return file descriptor
            } else {
                // Convert VFS result to syscall result
                switch (result) {
                    case kira::fs::FSResult::NOT_FOUND:
                        return static_cast<i32>(SyscallResult::FILE_NOT_FOUND);
                    case kira::fs::FSResult::PERMISSION_DENIED:
                        return static_cast<i32>(SyscallResult::PERMISSION_DENIED);
                    case kira::fs::FSResult::NO_SPACE:
                        return static_cast<i32>(SyscallResult::NO_SPACE);
                    default:
                        return static_cast<i32>(SyscallResult::IO_ERROR);
                }
            }
        }
        
        case SystemCall::CLOSE: {
            // Close file descriptor
            // arg1 = file descriptor, arg2/arg3 = unused
            i32 fd = static_cast<i32>(arg1);
            
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.close(fd);
            
            return (result == kira::fs::FSResult::SUCCESS) ? 
                static_cast<i32>(SyscallResult::SUCCESS) : 
                static_cast<i32>(SyscallResult::INVALID_PARAMETER);
        }
        
        case SystemCall::READ_FILE: {
            // Read from file
            // arg1 = file descriptor, arg2 = buffer pointer, arg3 = size
            i32 fd = static_cast<i32>(arg1);
            void* buffer = reinterpret_cast<void*>(arg2);
            u32 size = arg3;
            
            if (!buffer || size == 0) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.read(fd, size, buffer);
            
            return (result == kira::fs::FSResult::SUCCESS) ? 
                static_cast<i32>(size) : // Return bytes read
                static_cast<i32>(SyscallResult::IO_ERROR);
        }
        
        case SystemCall::WRITE_FILE: {
            // Write to file
            // arg1 = file descriptor, arg2 = buffer pointer, arg3 = size
            i32 fd = static_cast<i32>(arg1);
            const void* buffer = reinterpret_cast<const void*>(arg2);
            u32 size = arg3;
            
            if (!buffer || size == 0) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.write(fd, size, buffer);
            
            return (result == kira::fs::FSResult::SUCCESS) ? 
                static_cast<i32>(size) : // Return bytes written
                static_cast<i32>(SyscallResult::IO_ERROR);
        }
        
        case SystemCall::READDIR: {            
            // Read directory entry
            // arg1 = path pointer, arg2 = index, arg3 = entry buffer pointer
            const char* path = reinterpret_cast<const char*>(arg1);
            u32 index = arg2;
            kira::fs::DirectoryEntry* entry = reinterpret_cast<kira::fs::DirectoryEntry*>(arg3);
            
            kira::kernel::console.add_message("[SYSCALL] checking params", kira::display::VGA_MAGENTA_ON_BLUE);
            if (!path || !entry) {
                kira::kernel::console.add_message("[SYSCALL] null params", kira::display::VGA_RED_ON_BLUE);
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            // Debug: check path contents
            if (path[0] == '/' && path[1] == '\0') {
                kira::kernel::console.add_message("[SYSCALL] path is root /", kira::display::VGA_CYAN_ON_BLUE);
            } else {
                kira::kernel::console.add_message("[SYSCALL] path not root", kira::display::VGA_YELLOW_ON_BLUE);
            }
            
            kira::kernel::console.add_message("[SYSCALL] calling VFS", kira::display::VGA_MAGENTA_ON_BLUE);
            auto& vfs = kira::fs::VFS::get_instance();
            kira::kernel::console.add_message("[SYSCALL] about to call readdir", kira::display::VGA_MAGENTA_ON_BLUE);
            
            // Test if VFS instance is valid
            kira::kernel::console.add_message("[SYSCALL] VFS instance OK", kira::display::VGA_MAGENTA_ON_BLUE);
            
            // Check VFS root vnode status RIGHT NOW - before using it
            kira::kernel::console.add_message("[SYSCALL] Checking VFS root state", kira::display::VGA_YELLOW_ON_BLUE);
            if (vfs.is_root_mounted()) {
                kira::kernel::console.add_message("[SYSCALL] VFS root is mounted", kira::display::VGA_GREEN_ON_BLUE);
            } else {
                kira::kernel::console.add_message("[SYSCALL] ERROR: VFS root NOT mounted!", kira::display::VGA_RED_ON_BLUE);
                return static_cast<i32>(SyscallResult::IO_ERROR);
            }
            
            // Use kernel-space buffer to avoid user-space pointer dereferencing issue
            kira::fs::DirectoryEntry kernelEntry;
            

            kira::fs::FSResult result = vfs.readdir(path, index, kernelEntry);
            
            if (result == kira::fs::FSResult::SUCCESS) {
                kira::kernel::console.add_message("[SYSCALL] VFS success, copying to user", kira::display::VGA_GREEN_ON_BLUE);
                // Copy kernel entry to user space
                *entry = kernelEntry;
                kira::kernel::console.add_message("[SYSCALL] Copy to user completed", kira::display::VGA_GREEN_ON_BLUE);
            }
            kira::kernel::console.add_message("[SYSCALL] VFS call completed", kira::display::VGA_MAGENTA_ON_BLUE);
            
            if (result == kira::fs::FSResult::SUCCESS) {
                kira::kernel::console.add_message("[SYSCALL] SUCCESS", kira::display::VGA_GREEN_ON_BLUE);
                return static_cast<i32>(SyscallResult::SUCCESS);
            } else if (result == kira::fs::FSResult::NOT_FOUND) {
                // kira::kernel::console.add_message("[SYSCALL] NOT_FOUND", kira::display::VGA_YELLOW_ON_BLUE);
                return static_cast<i32>(SyscallResult::FILE_NOT_FOUND); // No more entries
            } else {
                return static_cast<i32>(SyscallResult::IO_ERROR);
            }
        }
        
        case SystemCall::MKDIR: {
            // Create directory
            // arg1 = path pointer, arg2/arg3 = unused
            const char* path = reinterpret_cast<const char*>(arg1);
            if (!path) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.mkdir(path);
            
            if (result == kira::fs::FSResult::SUCCESS) {
                return static_cast<i32>(SyscallResult::SUCCESS);
            } else if (result == kira::fs::FSResult::EXISTS) {
                return static_cast<i32>(SyscallResult::FILE_EXISTS);
            } else {
                return static_cast<i32>(SyscallResult::IO_ERROR);
            }
        }
        
        // Process management operations
        case SystemCall::PS: {
            // List processes - for now return process count
            // TODO: Implement proper process listing
            return pm.get_current_pid(); // Placeholder - return current PID
        }
        
        case SystemCall::KILL: {
            // Terminate process by PID
            // arg1 = target PID, arg2/arg3 = unused
            u32 target_pid = arg1;
            
            bool success = pm.terminate_process(target_pid);
            return success ? 
                static_cast<i32>(SyscallResult::SUCCESS) : 
                static_cast<i32>(SyscallResult::INVALID_PARAMETER);
        }
        
        default:
            return static_cast<i32>(SyscallResult::INVALID_SYSCALL);
    }
}

void initialize_syscalls() {
    // Set up system call interrupt (INT 0x80) - accessible from Ring 3
    IDT::set_user_interrupt_gate(0x80, (void*)syscall_stub);
}

} // namespace kira::system 