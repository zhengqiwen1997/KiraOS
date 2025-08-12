#include "../lib/libkira.hpp"
#include "../include/user_programs.hpp"
#include "core/types.hpp"

using namespace kira::usermode;
using namespace kira::system;

namespace kira::usermode {

/**
 * @brief KiraOS Interactive Shell
 * 
 * A full-featured command interpreter that provides:
 * - File system navigation and manipulation
 * - Process management 
 * - System information display
 * - Interactive user experience
 */
class KiraShell {
private:
    static constexpr u32 MAX_COMMAND_LENGTH = 256;
    static constexpr u32 MAX_ARGS = 16;
    u32 commandIndex = 0;
    bool finished = false;
    char currentDirectory[MAX_COMMAND_LENGTH];
    char commandBuffer[MAX_COMMAND_LENGTH];
    char* args[MAX_ARGS];
    u32 argCount;
    
    bool running;
    
public:
    KiraShell() : running(false), argCount(0) {
        // Initialize current directory to root
        for (u32 i = 0; i < MAX_COMMAND_LENGTH; i++) {
            currentDirectory[i] = '\0';
        }
        currentDirectory[0] = '/';
        
        // Clear command buffer completely
        for (u32 i = 0; i < MAX_COMMAND_LENGTH; i++) {
            commandBuffer[i] = '\0';
        }
        
        // Clear args array
        for (u32 i = 0; i < MAX_ARGS; i++) {
            args[i] = nullptr;
        }
    }
    
    /**
     * @brief Main shell loop - displays prompt and processes commands
     */
    void run() {
        running = true;
        
        // Display welcome message
        display_welcome();
        
        while (running) {
            display_prompt();
            
            if (read_command()) {
                parse_command();
                execute_command();
            } else {
                // No more commands, exit the loop
                break;
            }
        }
    }
    
private:
    /**
     * @brief Display welcome banner
     */
    void display_welcome() {
        UserAPI::print_colored("=====================================\n", Colors::CYAN_ON_BLUE);
        UserAPI::print_colored("       KiraOS Interactive Shell v1.0\n", Colors::YELLOW_ON_BLUE);
        UserAPI::print_colored("=====================================\n", Colors::CYAN_ON_BLUE);
        UserAPI::print_colored("Type 'help' for available commands\n", Colors::WHITE_ON_BLUE);
    }
    
    /**
     * @brief Display command prompt
     */
    void display_prompt() {
        // Build complete prompt as single string since console stores one color per line
        // Current limitation: console system supports ONE color per line only
        char promptBuffer[64];
        u32 pos = 0;
        
        // Clear buffer
        for (u32 i = 0; i < 64; i++) {
            promptBuffer[i] = '\0';
        }
        
        // Build "KiraOS:/$ "
        const char* parts[] = {"KiraOS:", currentDirectory, "$ "};
        for (u32 part = 0; part < 3; part++) {
            const char* str = parts[part];
            for (u32 i = 0; str[i] != '\0' && pos < 63; i++) {
                promptBuffer[pos++] = str[i];
            }
        }
        // Print complete prompt in green (single color for entire line)
        UserAPI::print(promptBuffer);
    }
    
    /**
     * @brief Read command from user input (simplified for now)
     * @return true if command was read successfully
     */
    bool read_command() {
        // Simple demo sequence - run each command once then exit        
        
        if (finished) {
            return false;
        }
        const char* demoCommands[] = {
            // "help",
            // "about", 
            "pwd",
            "ls",
            "mkdir /test_dir",
            "ls",
            "cd /test_dir",
            "pwd",
            "mkdir subdir",
            "ls",
            "cd ..",
            "rmdir /test_dir/subdir",
            "rmdir /test_dir",
            "ls",
            // "cd /APPS",
            // "pwd",
            // "ls",
            // "cat README.TXT",
            // "cd ..",
            // "cd ./BOOT",
            // "pwd",
            // "ls",
            // "cat README.TXT",
            // "cat BOOTINFO.TXT",
            // "mem",
            // "ps",
            // "printf_test",
            // "dmesg",
            "exit"
        };
        const u32 numCommands = sizeof(demoCommands) / sizeof(demoCommands[0]);
        
        
        if (commandIndex < numCommands) {
            // Copy demo command to buffer
            const char* cmd = demoCommands[commandIndex++];
            u32 i = 0;
            while (cmd[i] != '\0' && i < MAX_COMMAND_LENGTH - 1) {
                commandBuffer[i] = cmd[i];
                i++;
            }
            commandBuffer[i] = '\0';
            
            // Echo the command (simulate user typing)
            UserAPI::print_colored(commandBuffer, Colors::WHITE_ON_BLUE);
            UserAPI::println(""); // Add newline after command
            return true;
        } else {
            finished = true;
            return false;
        }
    }
    
    /**
     * @brief Parse command line into command and arguments
     */
    void parse_command() {
        argCount = 0;
        
        // Reset args array
        for (u32 i = 0; i < MAX_ARGS; i++) {
            args[i] = nullptr;
        }
        
        // Ensure command buffer is null terminated
        commandBuffer[MAX_COMMAND_LENGTH - 1] = '\0';
        
        // Simple parsing - split by spaces
        char* ptr = commandBuffer;
        
        // Skip leading spaces
        while (*ptr == ' ' && *ptr != '\0' && ptr < commandBuffer + MAX_COMMAND_LENGTH) {
            ptr++;
        }
        
        // Parse arguments
        while (*ptr != '\0' && argCount < MAX_ARGS - 1 && ptr < commandBuffer + MAX_COMMAND_LENGTH) {
            args[argCount++] = ptr;
            
            // Find next space or end of string
            while (*ptr != ' ' && *ptr != '\0' && ptr < commandBuffer + MAX_COMMAND_LENGTH) {
                ptr++;
            }
            
            // Null terminate this argument
            if (*ptr == ' ' && ptr < commandBuffer + MAX_COMMAND_LENGTH) {
                *ptr = '\0';
                ptr++;
                
                // Skip multiple spaces
                while (*ptr == ' ' && *ptr != '\0' && ptr < commandBuffer + MAX_COMMAND_LENGTH) {
                    ptr++;
                }
            }
        }
        
        args[argCount] = nullptr; // Null terminate args array
    }
    
    /**
     * @brief Execute the parsed command
     */
    void execute_command() {
        if (argCount == 0) {
            return; // Empty command
        }
        
        if (args[0] == nullptr) {
            return; // Empty command
        }
        
        const char* cmd = args[0];
        
        // All commands using safe inline character comparison
        // if (cmd[0] == 'h') {
        //     cmd_help();
        // }
        if (string_equals(cmd, "help")) {
            cmd_help();
        } else if (string_equals(cmd, "about")) {
            cmd_about();
    
        } else if (string_equals(cmd, "clear")) {
            cmd_clear();
        } else if (string_equals(cmd, "pwd")) {
            cmd_pwd();
        } else if (string_equals(cmd, "ls")) {
            cmd_ls();
        } else if (string_equals(cmd, "cat")) {
            cmd_cat();
        } else if (string_equals(cmd, "cd")) {
            cmd_cd();
        } else if (string_equals(cmd, "mkdir")) {
            cmd_mkdir();
        } else if (string_equals(cmd, "rmdir")) {
            cmd_rmdir();
        // } else if (string_equals(cmd, "mem")) {
        //     cmd_mem();
        // } else if (string_equals(cmd, "ps")) {
        //     cmd_ps();
        // } else if (string_equals(cmd, "printf_test")) {
        //     cmd_printf_test();
        // } else if (string_equals(cmd, "dmesg")) {
        //     cmd_dmesg();
        } else if (string_equals(cmd, "exit")) {
            cmd_exit();
        } else {
        //     // Unknown command
        //     UserAPI::print_colored("Unknown command: ", Colors::RED_ON_BLUE);
        //     UserAPI::println(cmd);
        //     UserAPI::print_colored("Type 'help' for available commands\n", Colors::YELLOW_ON_BLUE);
        }
    }
    
    // ============ COMMAND IMPLEMENTATIONS ============
    
    void cmd_help() {
        UserAPI::print_colored("KiraOS Shell - Available Commands:\n", Colors::YELLOW_ON_BLUE);
        UserAPI::print_colored("File System:\n", Colors::CYAN_ON_BLUE);
        UserAPI::println("  ls        - List directory contents");
        UserAPI::println("  cat <file> - Display file contents");
        UserAPI::println("  cd <dir>  - Change directory");
        UserAPI::println("  pwd       - Print working directory");
        UserAPI::println("  mkdir <dir> - Create directory");
        UserAPI::println("  rmdir <dir> - Remove empty directory");
        UserAPI::print_colored("Process Management:\n", Colors::CYAN_ON_BLUE);
        UserAPI::println("  ps        - List running processes");
        UserAPI::print_colored("System:\n", Colors::CYAN_ON_BLUE);
        UserAPI::println("  about     - System information");
        UserAPI::println("  mem       - Memory usage information");
        UserAPI::println("  clear     - Clear screen");
        UserAPI::println("  help      - Show this help message");
        UserAPI::print_colored("Development:\n", Colors::CYAN_ON_BLUE);
        UserAPI::println("  printf_test - Test UserAPI::printf functionality");
        UserAPI::println("  dmesg     - Display kernel messages");
        UserAPI::print_colored("Control:\n", Colors::CYAN_ON_BLUE);
        UserAPI::println("  exit      - Exit shell");
    }
    
    void cmd_about() {
        UserAPI::print_colored("KiraOS Operating System\n", Colors::YELLOW_ON_BLUE);
        UserAPI::println("Architecture: x86 (32-bit)");
        UserAPI::println("Kernel: Monolithic with modular drivers");
        UserAPI::println("Memory: Virtual memory with paging");
        UserAPI::println("File System: FAT32 with VFS layer");
        UserAPI::print_colored("Features:\n", Colors::CYAN_ON_BLUE);
        UserAPI::println("  * Process management and scheduling");
        UserAPI::println("  * Virtual memory with paging");
        UserAPI::println("  * Hardware interrupt handling");
        UserAPI::println("  * ATA/IDE disk driver");
        UserAPI::println("  * FAT32 file system");
        UserAPI::println("  * System call interface");
        UserAPI::println("  * Interactive shell (you're using it!)");
        UserAPI::println("");
    }
    
    void cmd_clear() {
        // Send multiple newlines to simulate clearing
        for (u32 i = 0; i < 20; i++) {
            UserAPI::println("");
        }
        UserAPI::print_colored("Screen cleared\n", Colors::GREEN_ON_BLUE);
    }
    
    void cmd_pwd() {
        UserAPI::print_colored("Current directory: ", Colors::WHITE_ON_BLUE);
        UserAPI::print_colored(currentDirectory, Colors::CYAN_ON_BLUE);
        UserAPI::println("");
    }
    
    void cmd_ls() {
        UserAPI::printf("Directory listing for: %s\n", currentDirectory);
        
        static FileSystem::DirectoryEntry entry;
        u32 index = 0;
        
        while (true) {
            i32 result = UserAPI::readdir(currentDirectory, index, &entry);
            
            if (result != 0) {
                break; // No more entries or error
            }
            
            // Display the entry
            if (entry.type == FileSystem::FileType::DIRECTORY) {
                UserAPI::printf("%s    <DIR>\n", entry.name);
            } else {
                UserAPI::printf("%s    <FILE>\n", entry.name);
            }
            
            index++;
        }
        
        if (index == 0) {
            UserAPI::print("Directory is empty\n");
        }
    }
    
    void cmd_cat() {
        UserAPI::printf("cmd_cat");
        if (argCount < 2) {
            UserAPI::print_colored("Usage: cat <filename>", Colors::YELLOW_ON_BLUE);
            UserAPI::println("");
            return;
        }
        
        const char* filename = args[1];
        char fullPath[256];
        build_absolute_path(filename, fullPath, sizeof(fullPath));
        UserAPI::printf("Reading file: %s\n", fullPath);
        UserAPI::println("");
        
        // Open file for reading
        i32 fd = UserAPI::open(fullPath, static_cast<u32>(FileSystem::OpenFlags::READ_ONLY));
        
        if (fd < 0) {
            UserAPI::printf("Error: Could not open file '%s' (error code: %d)\n", fullPath, fd);
            UserAPI::print_colored("Make sure the file exists and is readable.\n", Colors::YELLOW_ON_BLUE);
            return;
        }
        
        // Read and display file contents
        char buffer[512];
        i32 bytesRead = UserAPI::read_file(fd, buffer, sizeof(buffer) - 1);
        
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; // Null terminate
            UserAPI::printf("File contents (%d bytes):\n", bytesRead);
            UserAPI::println("=====================================");
            UserAPI::print(buffer);
            UserAPI::println("=====================================");
        } else if (bytesRead == 0) {
            UserAPI::println("File is empty.");
        } else {
            UserAPI::printf("Error reading file (error code: %d)\n", bytesRead);
        }
        
        // Close file
        UserAPI::close(fd);
        UserAPI::println("");
    }
    
    void cmd_cd() {
        if (argCount < 2) {
            UserAPI::print_colored("Usage: cd <directory>", Colors::YELLOW_ON_BLUE);
            UserAPI::println("");
            return;
        }

        const char* inputPath = args[1];

        // Build normalized absolute path from currentDirectory and inputPath
        char normalized[256];
        build_absolute_path(inputPath, normalized, sizeof(normalized));

        // Special-case root
        if (normalized[0] == '/' && normalized[1] == '\0') {
            currentDirectory[0] = '/';
            currentDirectory[1] = '\0';
            UserAPI::print_colored("Changed directory", Colors::GREEN_ON_BLUE);
            UserAPI::println("");
            return;
        }

        // Verify the target is a directory that exists
        if (!directory_exists(normalized)) {
            UserAPI::print_colored("cd: no such directory: ", Colors::RED_ON_BLUE);
            UserAPI::println(normalized);
            return;
        }

        // Update currentDirectory
        u32 i = 0;
        for (; i < MAX_COMMAND_LENGTH - 1 && normalized[i] != '\0'; i++) {
            currentDirectory[i] = normalized[i];
        }
        currentDirectory[i] = '\0';

        UserAPI::print_colored("Changed directory", Colors::GREEN_ON_BLUE);
        UserAPI::println("");
    }
    
    void cmd_mkdir() {
        if (argCount < 2) {
            UserAPI::print_colored("Usage: mkdir <directory>", Colors::YELLOW_ON_BLUE);
            UserAPI::println("");
            return;
        }

        const char* inputPath = args[1];

        // Build normalized absolute path from currentDirectory and inputPath
        char normalized[256];
        build_absolute_path(inputPath, normalized, sizeof(normalized));

        // Try to create the directory
        i32 result = UserAPI::mkdir(normalized);
        
        if (result == 0) {
            UserAPI::print_colored("Directory created successfully", Colors::GREEN_ON_BLUE);
            UserAPI::println("");
        } else if (result == -6) { // FILE_EXISTS
            UserAPI::print_colored("mkdir: directory already exists: ", Colors::RED_ON_BLUE);
            UserAPI::println(normalized);
        } else if (result == -5) { // FILE_NOT_FOUND (parent doesn't exist)
            UserAPI::print_colored("mkdir: parent directory does not exist: ", Colors::RED_ON_BLUE);
            UserAPI::println(normalized);
        } else if (result == -10) { // NOT_DIRECTORY (parent is not a directory)
            UserAPI::print_colored("mkdir: parent is not a directory: ", Colors::RED_ON_BLUE);
            UserAPI::println(normalized);
        } else {
            UserAPI::printf("mkdir: failed to create directory: %s (error %d)\n", normalized, result);
        }
    }
    
    void cmd_rmdir() {
        if (argCount < 2) {
            UserAPI::print_colored("Usage: rmdir <directory>", Colors::YELLOW_ON_BLUE);
            UserAPI::println("");
            return;
        }

        const char* inputPath = args[1];

        // Build normalized absolute path from currentDirectory and inputPath
        char normalized[256];
        build_absolute_path(inputPath, normalized, sizeof(normalized));

        // Don't allow removing root directory
        if (normalized[0] == '/' && normalized[1] == '\0') {
            UserAPI::print_colored("rmdir: cannot remove root directory", Colors::RED_ON_BLUE);
            UserAPI::println("");
            return;
        }

        // Try to remove the directory
        i32 result = UserAPI::rmdir(normalized);
        
        if (result == 0) {
            UserAPI::print_colored("Directory removed successfully", Colors::GREEN_ON_BLUE);
            UserAPI::println("");
        } else if (result == -5) { // FILE_NOT_FOUND
            UserAPI::print_colored("rmdir: directory does not exist: ", Colors::RED_ON_BLUE);
            UserAPI::println(normalized);
        } else if (result == -10) { // NOT_DIRECTORY
            UserAPI::print_colored("rmdir: not a directory: ", Colors::RED_ON_BLUE);
            UserAPI::println(normalized);
        } else if (result == -5) { // Directory not empty (we're reusing NOT_FOUND for this)
            UserAPI::print_colored("rmdir: directory not empty: ", Colors::RED_ON_BLUE);
            UserAPI::println(normalized);
        } else {
            UserAPI::printf("rmdir: failed to remove directory: %s (error %d)\n", normalized, result);
        }
    }
    
    // void cmd_mem() {
    //     UserAPI::print_colored("Memory Information:", Colors::YELLOW_ON_BLUE);
    //     UserAPI::println("");
    //     UserAPI::println("");
    //     UserAPI::println("Total RAM: 64MB (simulated)");
    //     UserAPI::println("Kernel Memory: 8MB"); 
    //     UserAPI::println("User Memory: 56MB");
    //     UserAPI::println("Free Memory: 48MB");
    //     UserAPI::println("");
    //     UserAPI::print_colored("Note: ", Colors::YELLOW_ON_BLUE);
    //     UserAPI::println("Real memory statistics coming soon!");
    // }
    
    // void cmd_ps() {
    //     UserAPI::print_colored("Process List:", Colors::YELLOW_ON_BLUE);
    //     UserAPI::println("");
    //     UserAPI::println("");
        
    //     // Get current process info using real system call
    //     i32 current_pid = UserAPI::ps();
        
    //     if (current_pid > 0) {
    //         UserAPI::printf("Current Process Information:\n");
    //         UserAPI::printf("PID: %d\n", current_pid);
    //         UserAPI::printf("Name: shell\n");
    //         UserAPI::printf("State: Running\n");
    //         UserAPI::printf("Mode: User\n");
    //         UserAPI::println("");
    //         UserAPI::print_colored("Note: ", Colors::YELLOW_ON_BLUE);
    //         UserAPI::println("Extended process listing coming soon!");
    //     } else {
    //         UserAPI::print_colored("Error: Could not retrieve process information\n", Colors::RED_ON_BLUE);
    //     }
        
    //     UserAPI::println("");
    // }
    
    // void cmd_printf_test() {
    //     UserAPI::print_colored("UserAPI::printf Test Suite:", Colors::YELLOW_ON_BLUE);
    //     UserAPI::println("");
    //     UserAPI::println("");
        
    //     // Test basic string formatting
    //     UserAPI::printf("Simple string: %s\n", "Hello, KiraOS!");
        
    //     // Test signed integers
    //     UserAPI::printf("Signed integers: %d, %d, %d\n", 42, -123, 0);
        
    //     // Test unsigned integers
    //     UserAPI::printf("Unsigned integers: %u, %u\n", 3456, 0xDEADBEEF);
        
    //     // Test hexadecimal (lowercase)
    //     UserAPI::printf("Hex lowercase: 0x%x, 0x%x\n", 255, 4096);
        
    //     // Test hexadecimal (uppercase)
    //     UserAPI::printf("Hex uppercase: 0x%X, 0x%X\n", 255, 4096);
        
    //     // Test character
    //     UserAPI::printf("Characters: '%c', '%c', '%c'\n", 'A', 'B', '!');
        
    //     // Test mixed formatting
    //     UserAPI::printf("Mixed: Process %s has PID %d (0x%X)\n", "shell", 42, 42);
        
    //     // Test percent escape
    //     UserAPI::printf("Percentage: 100%% complete\n");
        
    //     // Test null string handling
    //     UserAPI::printf("Null string: '%s'\n", (const char*)0);
        
    //     UserAPI::println("");
    //     UserAPI::print_colored("printf test completed!", Colors::GREEN_ON_BLUE);
    //     UserAPI::println("");
    // }
    
    // void cmd_dmesg() {
    //     UserAPI::print_colored("Kernel Messages (simulated):\n", Colors::YELLOW_ON_BLUE);
    //     UserAPI::println("");
        
    //     // Since we can't access kernel console directly from user mode,
    //     // provide useful diagnostic information
    //     UserAPI::println("[BOOT] KiraOS Kernel Started");
    //     UserAPI::println("[INIT] Memory Manager Initialized");
    //     UserAPI::println("[INIT] Process Manager Initialized");
    //     UserAPI::println("[VFS]  Mounting RamFS as root filesystem...");
    //     UserAPI::println("[VFS]  RamFS mounted successfully at /");
    //     UserAPI::println("[VFS]  Demo files creation completed");
    //     UserAPI::println("[USER] Shell process started");
        
    //     // Show current system state
    //     i32 current_pid = UserAPI::ps();
    //     UserAPI::printf("[INFO] Current PID: %d\n", current_pid);
    //     UserAPI::printf("[INFO] Shell running in user mode\n");
    //     UserAPI::printf("[INFO] VFS and RamFS operational\n");
        
    //     UserAPI::println("");
    //     UserAPI::print_colored("Note: This is simulated output. Real kernel messages", Colors::YELLOW_ON_BLUE);
    //     UserAPI::println(" are not accessible from user mode.");
    // }
    
    void cmd_exit() {
        UserAPI::print_colored("Exiting shell...\n", Colors::YELLOW_ON_BLUE);
        UserAPI::print_colored("Shell terminated. Goodbye!\n", Colors::GREEN_ON_BLUE);
        
        // Properly terminate the user process using system call
        UserAPI::exit(); // This will not return
    }
    
    // ============ UTILITY FUNCTIONS ============
    
    /**
     * @brief Simple string comparison (since we can't use strcmp)
     */
    static bool string_equals(const char* str1, const char* str2) {
        if (!str1 || !str2) {
            return false;
        }
        
        // Simple character-by-character comparison
        u32 i = 0;
        while (i < 32) { // Reasonable limit for command names
            if (str1[i] != str2[i]) {
                return false;
            }
            
            // If both strings end here, they're equal
            if (str1[i] == '\0') {
                return true;
            }
            
            i++;
        }
        
        // If we hit the limit, strings are too long or not equal
        return false;
    }

    // Build an absolute, normalized path from currentDirectory and an input path.
    // Handles absolute and relative inputs, "." and ".." components, and collapses duplicate slashes.
    void build_absolute_path(const char* inputPath, char* outPath, u32 outSize) {
        if (!outPath || outSize == 0) return;

        // Step 1: Build a combined path string
        char combined[256];
        u32 pos = 0;
        combined[0] = '\0';

        if (inputPath && inputPath[0] == '/') {
            // Absolute input
            for (u32 i = 0; inputPath[i] != '\0' && pos < sizeof(combined) - 1; i++) {
                combined[pos++] = inputPath[i];
            }
        } else {
            // Relative input: start from currentDirectory
            // Copy currentDirectory
            for (u32 i = 0; currentDirectory[i] != '\0' && pos < sizeof(combined) - 1; i++) {
                combined[pos++] = currentDirectory[i];
            }
            // Ensure single slash separator if not root
            if (pos == 0 || combined[0] != '/') {
                if (pos < sizeof(combined) - 1) combined[pos++] = '/';
            } else if (!(pos == 1 && combined[0] == '/')) {
                if (combined[pos - 1] != '/' && pos < sizeof(combined) - 1) combined[pos++] = '/';
            }
            // Append inputPath
            if (inputPath) {
                for (u32 i = 0; inputPath[i] != '\0' && pos < sizeof(combined) - 1; i++) {
                    combined[pos++] = inputPath[i];
                }
            }
        }
        combined[pos < sizeof(combined) ? pos : (sizeof(combined) - 1)] = '\0';

        // Step 2: Normalize the combined path into components
        const u32 MAX_COMPONENTS = 32;
        const u32 MAX_COMP_LEN = 64;
        char components[MAX_COMPONENTS][MAX_COMP_LEN];
        u32 compCount = 0;

        u32 i = 0;
        while (combined[i] != '\0') {
            // Skip consecutive slashes
            while (combined[i] == '/') { i++; }
            if (combined[i] == '\0') break;

            // Extract one segment
            char segment[MAX_COMP_LEN];
            u32 segLen = 0;
            while (combined[i] != '\0' && combined[i] != '/' && segLen < MAX_COMP_LEN - 1) {
                segment[segLen++] = combined[i++];
            }
            segment[segLen] = '\0';

            // Process segment
            if (segment[0] == '\0' || (segment[0] == '.' && segment[1] == '\0')) {
                // Skip
            } else if (segment[0] == '.' && segment[1] == '.' && segment[2] == '\0') {
                if (compCount > 0) {
                    compCount--; // Go up one level
                }
            } else {
                if (compCount < MAX_COMPONENTS) {
                    // Copy segment into components
                    u32 c = 0;
                    while (segment[c] != '\0' && c < MAX_COMP_LEN - 1) {
                        components[compCount][c] = segment[c];
                        c++;
                    }
                    components[compCount][c] = '\0';
                    compCount++;
                }
            }
        }

        // Step 3: Rebuild normalized path
        u32 outPos = 0;
        if (outPos < outSize - 1) outPath[outPos++] = '/';
        if (compCount == 0) {
            // Root
            outPath[outPos < outSize ? outPos : (outSize - 1)] = '\0';
            return;
        }
        for (u32 idx = 0; idx < compCount; idx++) {
            u32 c = 0;
            while (components[idx][c] != '\0' && outPos < outSize - 1) {
                outPath[outPos++] = components[idx][c++];
            }
            if (idx + 1 < compCount && outPos < outSize - 1) {
                outPath[outPos++] = '/';
            }
        }
        outPath[outPos < outSize ? outPos : (outSize - 1)] = '\0';
    }

    // Check whether an absolute path refers to an existing directory.
    // Strategy:
    // - Try readdir(path, 0, &entry)
    //   * SUCCESS (0): directory exists and has at least one entry
    //   * IO_ERROR (-7): not a directory
    //   * FILE_NOT_FOUND (-5): ambiguous: path not found OR empty directory
    //       -> disambiguate by open(path): if open succeeds, treat as empty directory; else not found.
    bool directory_exists(const char* absPath) {
        if (!absPath || absPath[0] != '/') return false;

        FileSystem::DirectoryEntry entry;
        i32 rd = UserAPI::readdir(absPath, 0, &entry);
        if (rd == 0) {
            return true; // Directory with entries
        }
        // -7: IO_ERROR (from kernel SyscallResult)
        if (rd == -7) {
            return false; // Not a directory
        }
        // -5: FILE_NOT_FOUND (from kernel SyscallResult)
        if (rd == -5) {
            // Try to open to see if the path exists at all (dir may be empty)
            i32 fd = UserAPI::open(absPath, static_cast<u32>(FileSystem::OpenFlags::READ_ONLY));
            if (fd >= 0) {
                UserAPI::close(fd);
                return true; // Treat as existing (likely empty directory)
            }
            return false; // Does not exist
        }
        return false;
    }
};

/**
 * @brief Shell entry point for user mode
 */
void user_shell() {
    KiraShell shell;
    shell.run();
}

} // namespace kira::usermode