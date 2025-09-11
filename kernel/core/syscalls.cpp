#include "core/syscalls.hpp"
#include "arch/x86/idt.hpp"
#include "core/utils.hpp"
#include "display/vga.hpp"
#include "display/console.hpp"
#include "core/process.hpp"
#include "fs/vfs.hpp"
#include "drivers/keyboard.hpp"
#include "memory/virtual_memory.hpp"
#include "memory/memory_manager.hpp"
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
    // Capture the original caller BEFORE handle_syscall potentially yields/switches
    auto& pm_for_save = ProcessManager::get_instance();
    Process* caller = pm_for_save.get_current_process();
    if (caller) {
        caller->savedSyscallEsp = kernel_frame_esp;
    }
    i32 ret = handle_syscall(syscall_num, arg1, arg2, arg3);
    // Store return value for resume path on the ORIGINAL caller
    if (caller) {
        caller->pendingSyscallReturn = static_cast<u32>(ret);
    }
    return ret;
}

i32 handle_syscall(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3) {
    VGADisplay vga;
    auto& pm = ProcessManager::get_instance();
    
    switch (static_cast<SystemCall>(syscall_num)) {
        case SystemCall::EXIT:
            // Terminate current process and switch to another if available.
            // arg1 optionally carries exit status; default 0
            kira::utils::k_printf("[EXIT] exiting with status=%d\n", static_cast<i32>(arg1));
            pm.terminate_current_process_with_status(static_cast<i32>(arg1));
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
            // Deprecated: builtin spawn removed; use EXEC instead
            return static_cast<i32>(SyscallResult::INVALID_SYSCALL);
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
            if (!elfResult.success) { vm.destroy_user_address_space(as); return static_cast<i32>(SyscallResult::INVALID_PARAMETER); }
            // Build argv: argv[0] = basename(path), then tokenize arg string by spaces
            const char* argvArr[16];
            char argvBuf[16][128];
            u32 argc = 0;
            // argv[0]
            {
                // Derive basename from path
                const char* base = path;
                for (const char* p = path; *p; ++p) { if (*p == '/') base = p + 1; }
                // Copy basename
                u32 i = 0; while (base[i] && i < sizeof(argvBuf[0]) - 1) { argvBuf[0][i] = base[i]; i++; }
                argvBuf[0][i] = '\0'; argvArr[0] = argvBuf[0]; argc = 1;
            }
            // Parse arg string from caller AS
            if (arg0 && argc < 16) {
                if (Process* caller = pm.get_current_process()) {
                    auto& vmc = VirtualMemoryManager::get_instance();
                    AddressSpace* original = vmc.get_current_address_space();
                    if (caller->addressSpace) vmc.switch_address_space(caller->addressSpace);
                    char kArgs[256]; u32 i = 0; const char* p = arg0;
                    while (i + 1 < sizeof(kArgs)) { char c = p[i]; kArgs[i++] = c; if (c == '\0') break; }
                    if (i == 0 || kArgs[i - 1] != '\0') kArgs[i] = '\0';
                    if (original) vmc.switch_address_space(original);
                    // Tokenize by spaces
                    u32 pos = 0;
                    while (kArgs[pos] != '\0' && argc < 16) {
                        while (kArgs[pos] == ' ') pos++;
                        if (kArgs[pos] == '\0') break;
                        u32 tp = 0;
                        while (kArgs[pos] != ' ' && kArgs[pos] != '\0' && tp < sizeof(argvBuf[0]) - 1) {
                            argvBuf[argc][tp++] = kArgs[pos++];
                        }
                        argvBuf[argc][tp] = '\0';
                        argvArr[argc] = argvBuf[argc];
                        argc++;
                    }
                }
            }
            // Build a minimal envp: PWD, USER, PATH
            const char* envpArr[8]; char envBuf[8][128]; u32 envc = 0;
            if (Process* parent = pm.get_current_process()) {
                // PWD
                {
                    u32 j = 0; const char* key = "PWD="; while (key[j]) { envBuf[envc][j] = key[j]; j++; }
                    u32 k = 0; while (parent->currentWorkingDirectory[k] && j < sizeof(envBuf[0]) - 1) { envBuf[envc][j++] = parent->currentWorkingDirectory[k++]; }
                    envBuf[envc][j] = '\0'; envpArr[envc] = envBuf[envc]; envc++;
                }
                // USER
                { const char* s = "USER=kira"; u32 j = 0; while (s[j] && j < sizeof(envBuf[0]) - 1) { envBuf[envc][j] = s[j]; j++; } envBuf[envc][j] = '\0'; envpArr[envc] = envBuf[envc]; envc++; }
                // PATH
                { const char* s = "PATH=/bin"; u32 j = 0; while (s[j] && j < sizeof(envBuf[0]) - 1) { envBuf[envc][j] = s[j]; j++; } envBuf[envc][j] = '\0'; envpArr[envc] = envBuf[envc]; envc++; }
            }
            u32 stackTop = kira::loaders::ElfLoader::setup_user_stack_with_args(as,
                                             argc ? argvArr : nullptr, argc,
                                             envc ? envpArr : nullptr, envc);
            if (stackTop == 0) { vm.destroy_user_address_space(as); return static_cast<i32>(SyscallResult::NO_SPACE); }
            

            // Create process from ELF
            u32 pid = pm.create_user_process_from_elf(as, elfResult.entryPoint, stackTop, "elf", 5);
            if (pid == 0) { vm.destroy_user_address_space(as); return static_cast<i32>(SyscallResult::NO_SPACE); }

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
                    // Inherit parent's FD table, honoring CLOSE_ON_EXEC
                    for (u32 i = 0; i < MAX_FDS; i++) {
                        child->fdTable[i] = parent->fdTable[i];
                        child->fdFlags[i] = parent->fdFlags[i];
                        if (child->fdTable[i] >= 0 && (child->fdFlags[i] & static_cast<u32>(FD_FLAGS::CLOSE_ON_EXEC))) {
                            child->fdTable[i] = -1;
                            child->fdFlags[i] = 0;
                        }
                    }
                }
            }

            // Record parentPid in child and return child's PID immediately
            if (Process* parent = pm.get_current_process()) {
                if (Process* child = pm.get_process(pid)) {
                    child->parentPid = parent->pid;
                }
            }
            // Do not yield here; return to caller first so it can decide to wait
            return static_cast<i32>(pid);
        }
        
        case SystemCall::FORK: {
            // Duplicate current process; parent returns child's pid, child returns 0
            i32 childPid = static_cast<i32>(pm.fork_current_process());
            // Set the parent's pending return to childPid (child context gets eax=0)
            if (auto* p = pm.get_current_process()) {
                p->pendingSyscallReturn = static_cast<u32>(childPid);
            }
            return childPid;
        }

        case SystemCall::SBRK: {
            // arg1 = increment (signed), returns old break or negative on error
            Process* cur = pm.get_current_process();
            if (!cur || !cur->addressSpace) return static_cast<i32>(SyscallResult::IO_ERROR);
            i32 inc = static_cast<i32>(arg1);
            u32 oldBrk = cur->heapEnd;
            // Initialize heap on first use
            if (cur->heapStart == 0 && cur->heapEnd == 0) {
                cur->heapStart = USER_HEAP_START;
                cur->heapEnd = USER_HEAP_START;
                oldBrk = cur->heapEnd;
            }
            // Compute new break
            i32 newEndSigned = static_cast<i32>(cur->heapEnd) + inc;
            if (newEndSigned < static_cast<i32>(cur->heapStart)) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            u32 newEnd = static_cast<u32>(newEndSigned);
            // Page-align mapping operations
            auto& vm = VirtualMemoryManager::get_instance();
            if (newEnd > cur->heapEnd) {
                for (u32 va = (cur->heapEnd + PAGE_SIZE - 1) & PAGE_MASK; va < newEnd; va += PAGE_SIZE) {
                    void* phys = MemoryManager::get_instance().allocate_physical_page();
                    if (!phys) return static_cast<i32>(SyscallResult::NO_SPACE);
                    if (!cur->addressSpace->map_page(va, reinterpret_cast<u32>(phys), true, true)) {
                        return static_cast<i32>(SyscallResult::NO_SPACE);
                    }
                }
            } else if (newEnd < cur->heapEnd) {
                for (u32 va = (newEnd + PAGE_SIZE - 1) & PAGE_MASK; va < cur->heapEnd; va += PAGE_SIZE) {
                    u32 phys = cur->addressSpace->get_physical_address(va) & PAGE_MASK;
                    cur->addressSpace->unmap_page(va);
                    if (phys) MemoryManager::get_instance().free_physical_page(reinterpret_cast<void*>(phys));
                }
            }
            cur->heapEnd = newEnd;
            return static_cast<i32>(oldBrk);
        }

        case SystemCall::BRK: {
            // arg1 = absolute new end; returns new end on success, negative on error
            Process* cur = pm.get_current_process();
            if (!cur || !cur->addressSpace) return static_cast<i32>(SyscallResult::IO_ERROR);
            u32 requested = arg1;
            if (cur->heapStart == 0 && cur->heapEnd == 0) {
                cur->heapStart = USER_HEAP_START;
                cur->heapEnd = USER_HEAP_START;
            }
            if (requested < cur->heapStart || requested > USER_STACK_TOP) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            i32 inc = static_cast<i32>(requested - cur->heapEnd);
            // Reuse sbrk logic by tail-calling handler
            return handle_syscall(static_cast<u32>(SystemCall::SBRK), static_cast<u32>(inc), 0, 0);
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
            i32 vfsFd;
            kira::fs::FSResult result = vfs.open(path, static_cast<kira::fs::OpenFlags>(arg2), vfsFd);
            
            if (result != kira::fs::FSResult::SUCCESS) {
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
            // Map to a small per-process FD
            Process* cur = pm.get_current_process(); if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            for (u32 ufd = 0; ufd < MAX_FDS; ufd++) {
                if (cur->fdTable[ufd] == -1) { cur->fdTable[ufd] = vfsFd; cur->fdFlags[ufd] = 0; return static_cast<i32>(ufd); }
            }
            // No slot; close VFS fd and return error
            (void)vfs.close(vfsFd);
            return static_cast<i32>(SyscallResult::TOO_MANY_FILES);
        }
        
        case SystemCall::CLOSE: {
            // Close file descriptor
            // arg1 = file descriptor, arg2/arg3 = unused
            i32 ufd = static_cast<i32>(arg1);
            Process* cur = pm.get_current_process(); if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            if (ufd < 0 || ufd >= static_cast<i32>(MAX_FDS)) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            i32 vfsFd = cur->fdTable[static_cast<u32>(ufd)];
            // Handle STD sentinels specially
            if (vfsFd == -100 || vfsFd == -101 || vfsFd == -102) {
                cur->fdTable[static_cast<u32>(ufd)] = -1; cur->fdFlags[static_cast<u32>(ufd)] = 0;
                return static_cast<i32>(SyscallResult::SUCCESS);
            }
            if (vfsFd < 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            // Determine if any other ufd maps to same vfsFd; if none, close underlying
            bool another = false;
            for (u32 i = 0; i < MAX_FDS; i++) {
                if (i == static_cast<u32>(ufd)) continue;
                if (cur->fdTable[i] == vfsFd) { another = true; break; }
            }
            auto& vfs = kira::fs::VFS::get_instance();
            if (!another) {
                kira::fs::FSResult r = vfs.close(vfsFd);
                (void)r;
            }
            cur->fdTable[static_cast<u32>(ufd)] = -1;
            cur->fdFlags[static_cast<u32>(ufd)] = 0;
            return static_cast<i32>(SyscallResult::SUCCESS);
        }
        
        case SystemCall::READ_FILE: {
            // Read from file
            // arg1 = file descriptor, arg2 = buffer pointer, arg3 = size
            i32 ufd = static_cast<i32>(arg1);
            void* buffer = reinterpret_cast<void*>(arg2);
            u32 size = arg3;
            
            if (!buffer || size == 0) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            Process* cur = pm.get_current_process(); if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            if (ufd < 0 || ufd >= static_cast<i32>(MAX_FDS)) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            i32 vfsFd = cur->fdTable[static_cast<u32>(ufd)];
            // STDIN
            if (vfsFd == -100) {
                // Blocking read of one char for now
                char* out = reinterpret_cast<char*>(buffer);
                if (size == 0) return 0;
                // Use existing GETCH path
                char c;
                // Simulate GETCH via blocking delivery
                pm.block_current_process_for_input();
                for (;;) { asm volatile("sti"); asm volatile("hlt"); }
                // Unreachable here; return value delivered via pendingSyscallReturn
            }
            if (vfsFd < 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.read(vfsFd, size, buffer);
            if (result == kira::fs::FSResult::SUCCESS) return static_cast<i32>(size);
            return static_cast<i32>(SyscallResult::IO_ERROR);
        }
        
        case SystemCall::WRITE_FILE: {
            // Write to file
            // arg1 = file descriptor, arg2 = buffer pointer, arg3 = size
            i32 ufd = static_cast<i32>(arg1);
            const void* buffer = reinterpret_cast<const void*>(arg2);
            u32 size = arg3;
            
            if (!buffer || size == 0) {
                return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            }
            
            Process* cur = pm.get_current_process(); if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            if (ufd < 0 || ufd >= static_cast<i32>(MAX_FDS)) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            i32 vfsFd = cur->fdTable[static_cast<u32>(ufd)]; if (vfsFd < 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            // STDOUT/STDERR routed to console
            if (vfsFd == -101 || vfsFd == -102) {
                // Best-effort print without interpreting %
                const char* s = reinterpret_cast<const char*>(buffer);
                // Ensure null-terminated copy within size
                char tmp[256]; u32 n = (size < sizeof(tmp)-1) ? size : (sizeof(tmp)-1);
                for (u32 i = 0; i < n; i++) tmp[i] = s[i]; tmp[n] = '\0';
                kira::kernel::console.add_message(tmp, kira::display::VGA_WHITE_ON_BLUE);
                return static_cast<i32>(size);
            }
            auto& vfs = kira::fs::VFS::get_instance();
            kira::fs::FSResult result = vfs.write(vfsFd, size, buffer);
            if (result == kira::fs::FSResult::SUCCESS) return static_cast<i32>(size);
            return static_cast<i32>(SyscallResult::IO_ERROR);
        }

        case SystemCall::DUP: {
            // arg1 = oldfd, arg2 = newfd (optional; if 0xFFFFFFFF, use lowest), arg3 unused
            Process* cur = pm.get_current_process(); if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            i32 oldfd = static_cast<i32>(arg1);
            i32 newfd = static_cast<i32>(arg2);
            if (oldfd < 0 || oldfd >= static_cast<i32>(MAX_FDS)) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            i32 oldVfs = cur->fdTable[static_cast<u32>(oldfd)];
            // Allow dup of STD sentinels too
            bool isStdSentinel = (oldVfs == -100 || oldVfs == -101 || oldVfs == -102);
            if (oldVfs < 0 && !isStdSentinel) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            if (newfd == -1) {
                for (u32 i = 0; i < MAX_FDS; i++) { if (cur->fdTable[i] == -1) { newfd = static_cast<i32>(i); break; } }
                if (newfd == -1) return static_cast<i32>(SyscallResult::TOO_MANY_FILES);
            } else {
                if (newfd < 0 || newfd >= static_cast<i32>(MAX_FDS)) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
                if (newfd == oldfd) return newfd;
                // If occupied, close it first
                if (cur->fdTable[static_cast<u32>(newfd)] >= 0 ||
                    cur->fdTable[static_cast<u32>(newfd)] == -100 ||
                    cur->fdTable[static_cast<u32>(newfd)] == -101 ||
                    cur->fdTable[static_cast<u32>(newfd)] == -102) {
                    (void)handle_syscall(static_cast<u32>(SystemCall::CLOSE), static_cast<u32>(newfd), 0, 0);
                }
            }
            cur->fdTable[static_cast<u32>(newfd)] = oldVfs;
            cur->fdFlags[static_cast<u32>(newfd)] = cur->fdFlags[static_cast<u32>(oldfd)];
            return newfd;
        }

        case SystemCall::SET_FD_FLAGS: {
            // arg1 = fd, arg2 = flags mask, arg3 = set(1)/clear(0)
            Process* cur = pm.get_current_process(); if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            i32 fd = static_cast<i32>(arg1);
            u32 mask = arg2;
            u32 set = arg3;
            if (fd < 0 || fd >= static_cast<i32>(MAX_FDS)) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            if (cur->fdTable[static_cast<u32>(fd)] < 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            if (set) cur->fdFlags[static_cast<u32>(fd)] |= mask; else cur->fdFlags[static_cast<u32>(fd)] &= ~mask;
            return static_cast<i32>(SyscallResult::SUCCESS);
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
        
        case SystemCall::WAIT: {
            // arg1 = pid to wait on (0 => wait any child), arg2 = user buffer for status (ignored for now)
            Process* cur = pm.get_current_process();
            if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            u32 wantPid = arg1;
            // Simple: only support explicit pid for now
            if (wantPid == 0) {
                // Any child: if none exist, error; else block until one exits
                if (!pm.has_child(cur->pid)) return static_cast<i32>(SyscallResult::PERMISSION_DENIED);
                cur->waitingOnPid = WAIT_ANY_CHILD;
                pm.block_current_process();
                for (;;) { asm volatile("sti"); asm volatile("hlt"); }
            }
            Process* target = pm.get_process(wantPid);
            if (!target) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            // Must be a child
            if (target->parentPid != cur->pid) return static_cast<i32>(SyscallResult::PERMISSION_DENIED);
            if (target->state == ProcessState::ZOMBIE) {
                // If already terminated, return its exit status and reap
                i32 st = target->exitStatus;
                target->hasBeenWaited = true;
                pm.reap_child(target);
                return st;
            }
            // Block until it terminates; switch away immediately using helper
            cur->waitingOnPid = wantPid;
            pm.block_current_process();
            // Park: this thread will be resumed via resume_from_syscall_stack with EAX preset
            for (;;) { asm volatile("sti"); asm volatile("hlt"); }
        }
        
        case SystemCall::WAITID: {
            // arg1 = pid to wait on (0 => any child), arg2 = user ptr to write status
            Process* cur = pm.get_current_process();
            if (!cur) return static_cast<i32>(SyscallResult::IO_ERROR);
            u32 wantPid = arg1;
            i32* userStatus = reinterpret_cast<i32*>(arg2);
            if (!userStatus) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
            // If any child
            if (wantPid == 0) {
                // First, check if any child already terminated
                if (Process* p = pm.find_terminated_child(cur->pid)) {
                    if (p->hasBeenWaited) {
                        // Skip already reported child; fall through to stash/block logic
                    } else {
                        // Write in caller's AS
                        auto& vm = VirtualMemoryManager::get_instance();
                        AddressSpace* original = vm.get_current_address_space();
                        if (cur->addressSpace) { vm.switch_address_space(cur->addressSpace); }
                        const i32 st = p->exitStatus;
                        const u32 retPid = p->pid;
                        *userStatus = st;
                        if (original) { vm.switch_address_space(original); }
                        p->hasBeenWaited = true;
                        kira::utils::k_printf("[WAITID] fast parent=%u child=%u status=%d\n", cur->pid, p->pid, p->exitStatus);
                        pm.reap_child(p);
                        return static_cast<i32>(retPid);
                    }
                }
                // If parent has a stashed completed child (completed before block), consume it
                if (cur->pendingChildPid != 0) {
                    auto& vm = VirtualMemoryManager::get_instance();
                    AddressSpace* original = vm.get_current_address_space();
                    if (cur->addressSpace) { vm.switch_address_space(cur->addressSpace); }
                    *userStatus = cur->pendingChildStatus;
                    if (original) { vm.switch_address_space(original); }
                    i32 ret = static_cast<i32>(cur->pendingChildPid);
                    cur->pendingChildPid = 0; cur->pendingChildStatus = 0;
                    kira::utils::k_printf("[WAITID] fast-stash parent=%u child=%d status=%d\n", cur->pid, ret, *userStatus);
                    return ret;
                }
                // If no children at all, error
                if (!pm.has_child(cur->pid)) {
                    return static_cast<i32>(SyscallResult::PERMISSION_DENIED);
                }
                // None yet; block and wait on any
                kira::utils::k_printf("[WAITID] block parent=%u any, ptr=%x\n", cur->pid, (u32)userStatus);
                cur->waitingOnPid = WAIT_ANY_CHILD; // explicit any-child sentinel
                cur->waitStatusUserPtr = reinterpret_cast<u32>(userStatus);
                pm.block_current_process();
                for (;;) { asm volatile("sti"); asm volatile("hlt"); }
            } else {
                Process* target = pm.get_process(wantPid);
                if (!target) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
                if (target->parentPid != cur->pid) return static_cast<i32>(SyscallResult::PERMISSION_DENIED);
                if (target->state == ProcessState::ZOMBIE) {
                    if (target->hasBeenWaited) {
                        return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
                    }
                    auto& vm = VirtualMemoryManager::get_instance();
                    AddressSpace* original = vm.get_current_address_space();
                    if (cur->addressSpace) { vm.switch_address_space(cur->addressSpace); }
                    const i32 st = target->exitStatus;
                    const u32 retPid = target->pid;
                    *userStatus = st;
                    if (original) { vm.switch_address_space(original); }
                    target->hasBeenWaited = true;
                    kira::utils::k_printf("[WAITID] fast parent=%u child=%u status=%d\n", cur->pid, retPid, st);
                    pm.reap_child(target);
                    return static_cast<i32>(retPid);
                }
                kira::utils::k_printf("[WAITID] block parent=%u on pid=%u ptr=%x\n", cur->pid, wantPid, (u32)userStatus);
                cur->waitingOnPid = wantPid;
                cur->waitStatusUserPtr = reinterpret_cast<u32>(userStatus);
                pm.block_current_process();
                for (;;) { asm volatile("sti"); asm volatile("hlt"); }
            }
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