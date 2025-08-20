#include "core/syscalls.hpp"
#include "arch/x86/idt.hpp"
#include "core/utils.hpp"
#include "display/vga.hpp"
#include "display/console.hpp"
#include "core/process.hpp"
#include "fs/vfs.hpp"
#include "drivers/keyboard.hpp"
#include "user_programs.hpp"
#include "memory/virtual_memory.hpp"
#include "loaders/elf_loader.hpp"

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
extern "C" {
    // Implemented in assembly to resume from a saved kernel stack
    void resume_from_syscall_stack(u32 newEsp);
}

// Forward declaration removed: dispatcher will be handled by program-specific entrypoints

extern "C" i32 syscall_handler(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3, u32 kernel_frame_esp) {
    // Save the kernel frame ESP into current process so we can resume at iret
    auto& pm_for_save = ProcessManager::get_instance();
    if (auto* p = pm_for_save.get_current_process()) {
        p->savedSyscallEsp = kernel_frame_esp;
    }
    i32 ret = handle_syscall(syscall_num, arg1, arg2, arg3);
    // Store return value for resume path
    if (auto* p = pm_for_save.get_current_process()) {
        p->pendingSyscallReturn = static_cast<u32>(ret);
    }
    return ret;
}

i32 handle_syscall(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3) {
    VGADisplay vga;
    auto& pm = ProcessManager::get_instance();
    
    switch (static_cast<SystemCall>(syscall_num)) {
        case SystemCall::EXIT:
            // Terminate current process and switch to another if available.
            pm.terminate_current_process();
            // If we reach here, there was no immediate process to run.
            // Enter idle loop; timer IRQ may start another process later.
            while (true) { asm volatile("sti"); asm volatile("hlt"); }
            
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

        case SystemCall::GETCH: {
            // If a char is available now, return it immediately
            char ch;
            if (Keyboard::try_dequeue_char(ch)) {
                return static_cast<i32>(static_cast<u8>(ch));
            }
            // Otherwise block this process on the input wait queue; it will be
            // woken by the keyboard IRQ with its return value set
            pm.block_current_process_for_input();
            // Do not return to the syscall stub for this blocked process. Park.
            for (;;) { asm volatile("sti"); asm volatile("hlt"); }
        }

        case SystemCall::TRYGETCH: {
            // Non-blocking get char
            char ch;
            if (Keyboard::try_dequeue_char(ch)) {
                return static_cast<i32>(static_cast<u8>(ch));
            }
            return 0;
        }

        case SystemCall::SPAWN: {
            // prog: 0=ls (currently supported). arg2=pointer to cwd
            u32 prog = arg1; u32 arg = arg2;
            if (prog != 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER); // only ls for now
            // Create child process for ls
            u32 childPid = pm.create_user_process(reinterpret_cast<ProcessFunction>(kira::usermode::user_ls_main), "ls", 5);
            if (childPid == 0) return static_cast<i32>(SyscallResult::IO_ERROR);
            Process* child = pm.get_process(childPid);
            if (!child) return static_cast<i32>(SyscallResult::IO_ERROR);
            // Copy caller's cwd into child's spawnArg for future use if needed
            const char* src = reinterpret_cast<const char*>(arg); u32 i = 0;
            // utils::k_printf("the arg2: %s\n", src);
            if (src) { while (src[i] != '\0' && i < sizeof(child->spawnArg) - 1) { child->spawnArg[i] = src[i]; i++; } }
            child->spawnArg[i] = '\0';
            utils::k_printf("the child->spawnArg: %s\n", child->spawnArg);
            // Also set child's PCB current working directory to the same path
            for (u32 j = 0; j <= i && j < sizeof(child->currentWorkingDirectory); j++) {
                child->currentWorkingDirectory[j] = child->spawnArg[j];
            }

            // No initial stack argument setup; child queries cwd via GETCWD
            // Block parent until child terminates (cooperative wait)
            Process* parent = pm.get_current_process();
            if (parent) {
                parent->waitingOnPid = childPid;
                parent->state = ProcessState::BLOCKED;
            }
            // Switch away; when child exits, scheduler will pick parent again
            pm.schedule();
            return static_cast<i32>(SyscallResult::SUCCESS);
        }
        case SystemCall::CHDIR: {
            const char* path = reinterpret_cast<const char*>(arg1);
            if (!path) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            Process* cur = pm.get_current_process();
            if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            // Require absolute path for now
            if (path[0] != '\0' && path[0] != '/') return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            // Validate path exists and is a directory via VFS
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FileStat st;
            kira::fs::FSResult res = vfs.stat(path, st);
            if (res != kira::fs::FSResult::SUCCESS) {
                return static_cast<i32>(SyscallResult::FILE_NOT_FOUND);
            }
            if (st.type != kira::fs::FileType::DIRECTORY) {
                return static_cast<i32>(SyscallResult::NOT_DIRECTORY);
            }
            // Copy string into PCB cwd
            u32 i = 0;
            while (path[i] != '\0' && i < sizeof(cur->currentWorkingDirectory) - 1) {
                cur->currentWorkingDirectory[i] = path[i];
                i++;
            }
            cur->currentWorkingDirectory[i] = '\0';
            return static_cast<i32>(SyscallResult::SUCCESS);
        }
        case SystemCall::GETCWD: {
            char* buf = reinterpret_cast<char*>(arg1);
            u32 size = arg2;
            if (!buf || size == 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            Process* cur = pm.get_current_process();
            if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            u32 i = 0;
            while (cur->currentWorkingDirectory[i] != '\0' && i + 1 < size) {
                buf[i] = cur->currentWorkingDirectory[i];
                i++;
            }
            buf[i] = '\0';
            return static_cast<i32>(SyscallResult::SUCCESS);
        }
        
        case SystemCall::GETCWD_PTR: {
            Process* p = pm.get_current_process();
            if (!p) return 0;
            return reinterpret_cast<i32>(p->currentWorkingDirectory);
        }

        case SystemCall::GETSPAWNARG: {
            // arg1 = user buffer, arg2 = size
            Process* p = pm.get_current_process();
            if (!p) return static_cast<i32>(SyscallResult::IO_ERROR);
            char* out = reinterpret_cast<char*>(arg1);
            u32 size = arg2;
            if (!out || size == 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            u32 i = 0;
            while (p->spawnArg[i] != '\0' && i + 1 < size) {
                out[i] = p->spawnArg[i];
                i++;
            }
            out[i] = '\0';
            return static_cast<i32>(SyscallResult::SUCCESS);
        }
        
        case SystemCall::EXEC: {
            const char* path = reinterpret_cast<const char*>(arg1);
            const char* arg0 = reinterpret_cast<const char*>(arg2); // optional spawn arg string
            if (!path) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            // Load file via VFS
            auto& vfs = kira::fs::VFS::get_instance();
            i32 fd;
            if (vfs.open(path, kira::fs::OpenFlags::READ_ONLY, fd) != kira::fs::FSResult::SUCCESS) {
                return static_cast<i32>(SyscallResult::FILE_NOT_FOUND);
            }
            // Get file size
            kira::fs::FileStat st;
            if (vfs.stat(path, st) != kira::fs::FSResult::SUCCESS || st.type != kira::fs::FileType::REGULAR || st.size == 0) {
                vfs.close(fd);
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }

            // Temporary kernel-mapped buffer for ELF file (limit size for now)
            static u8 elfBuffer[65536]; // 64KB cap for early exec implementation
            if (st.size > sizeof(elfBuffer)) {
                vfs.close(fd);
                return static_cast<i32>(SyscallResult::NO_SPACE);
            }
            if (vfs.read(fd, st.size, elfBuffer) != kira::fs::FSResult::SUCCESS) {
                vfs.close(fd);
                return static_cast<i32>(SyscallResult::IO_ERROR);
            }
            vfs.close(fd);
            // // Create address space and load ELF
            auto& vm = kira::system::VirtualMemoryManager::get_instance();
            kira::system::AddressSpace* as = vm.create_user_address_space();
            if (!as) { return static_cast<i32>(SyscallResult::NO_SPACE); }
            auto elfResult = kira::loaders::ElfLoader::load_executable(elfBuffer, st.size, as);
            if (!elfResult.success) { as->~AddressSpace(); return static_cast<i32>(SyscallResult::INVALID_PARAMETER); }
            u32 stackTop = kira::loaders::ElfLoader::setup_user_stack(as);
            if (stackTop == 0) { as->~AddressSpace(); return static_cast<i32>(SyscallResult::NO_SPACE); }
            

            // Create process from ELF
            u32 pid = pm.create_user_process_from_elf(as, elfResult.entryPoint, stackTop, "elf", 5);
            if (pid == 0) {
                as->~AddressSpace();
                return static_cast<i32>(SyscallResult::NO_SPACE);
            }

            // Inherit cwd and propagate single spawn argument (if provided)
            if (Process* child = pm.get_process(pid)) {
                if (Process* parent = pm.get_current_process()) {
                    for (u32 i = 0; i < sizeof(child->currentWorkingDirectory) - 1; i++) {
                        child->currentWorkingDirectory[i] = parent->currentWorkingDirectory[i];
                        if (parent->currentWorkingDirectory[i] == '\0') break;
                    }
                    // Copy optional argument into child's spawnArg for user programs
                    if (arg0) {
                        u32 i = 0; while (arg0[i] && i < sizeof(child->spawnArg) - 1) { child->spawnArg[i] = arg0[i]; i++; }
                        child->spawnArg[i] = '\0';
                    } else {
                        child->spawnArg[0] = '\0';
                    }
                }
            }

            // Block parent until child exits
            if (Process* parent = pm.get_current_process()) {
                parent->waitingOnPid = pid;
                parent->state = ProcessState::BLOCKED;
            }
            pm.schedule();
            return static_cast<i32>(SyscallResult::SUCCESS);
        }
        
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
            
            if (!path || !entry) {
                kira::kernel::console.add_message("[SYSCALL] null params", kira::display::VGA_RED_ON_BLUE);
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.readdir(path, index, *entry);
            
            if (result == kira::fs::FSResult::SUCCESS) {
                return static_cast<i32>(SyscallResult::SUCCESS);
            } else if (result == kira::fs::FSResult::NOT_FOUND) {
                return static_cast<i32>(SyscallResult::FILE_NOT_FOUND); // No more entries
            } else {
                return static_cast<i32>(SyscallResult::IO_ERROR);
            }
        }
        
        case SystemCall::MKDIR: {
            const char* path = reinterpret_cast<const char*>(arg1);
            if (!path) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.mkdir(path);
            if (result == kira::fs::FSResult::SUCCESS) return static_cast<i32>(SyscallResult::SUCCESS);
            if (result == kira::fs::FSResult::EXISTS) return static_cast<i32>(SyscallResult::FILE_EXISTS);
            return static_cast<i32>(SyscallResult::IO_ERROR);
        }
        
        case SystemCall::RMDIR: {
            const char* path = reinterpret_cast<const char*>(arg1);
            if (!path) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.rmdir(path);
            if (result == kira::fs::FSResult::SUCCESS) return static_cast<i32>(SyscallResult::SUCCESS);
            if (result == kira::fs::FSResult::NOT_FOUND) return static_cast<i32>(SyscallResult::FILE_NOT_FOUND);
            if (result == kira::fs::FSResult::NOT_DIRECTORY) return static_cast<i32>(SyscallResult::NOT_DIRECTORY);
            return static_cast<i32>(SyscallResult::IO_ERROR);
        }
        
        case SystemCall::PS: {
            return pm.get_current_pid();
        }
        
        case SystemCall::KILL: {
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