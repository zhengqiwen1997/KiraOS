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
    bool finished = false;
    char currentDirectory[MAX_COMMAND_LENGTH];
    char commandBuffer[MAX_COMMAND_LENGTH];
    char* args[MAX_ARGS];
    u32 argCount;
    u32 cmdLen;

public:
    KiraShell() : argCount(0), cmdLen(0) {
        // Initialize current directory to root
        for (u32 i = 0; i < MAX_COMMAND_LENGTH; i++) {
            currentDirectory[i] = '\0';
            commandBuffer[i] = '\0';
        }
        currentDirectory[0] = '/';
    }
    
    /**
     * @brief Main shell loop - displays prompt and processes commands
     */
    void run() {
        display_welcome();
        while (!finished) {
            display_prompt();
            if (!read_command()) break;
            parse_command();
            execute_command();
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
        char promptBuffer[64];
        u32 pos = 0;
        for (u32 i = 0; i < 64; i++) promptBuffer[i] = '\0';
        const char* parts[] = {"KiraOS:", currentDirectory, "$ "};
        for (u32 part = 0; part < 3; part++) {
            const char* str = parts[part];
            for (u32 i = 0; str[i] != '\0' && pos < 63; i++) {
                promptBuffer[pos++] = str[i];
            }
        }
        UserAPI::print(promptBuffer);
    }
    
    /**
     * @brief Read command from user input interactively
     * @return true if a line was read, false to exit
     */
    bool read_command() {
        // Reset buffer
        cmdLen = 0;
        commandBuffer[0] = '\0';
        
        while (true) {
            i32 ch = UserAPI::getch();
            if (ch == 27) { // ESC to exit shell
                finished = true;
                UserAPI::println("");
                return false;
            }
            if (ch == '\n' || ch == '\r') {
                UserAPI::println("");
                commandBuffer[cmdLen] = '\0';
                return true;
            }
            if ((ch == '\b') && cmdLen > 0) {
                // Backspace: remove last char visually and from buffer
                cmdLen--;
                commandBuffer[cmdLen] = '\0';
                UserAPI::print("\b");
                continue;
            }
            if (cmdLen < MAX_COMMAND_LENGTH - 1 && ch >= 32 && ch < 127) {
                // Printable character
                commandBuffer[cmdLen++] = (char)ch;
                commandBuffer[cmdLen] = '\0';
                char echo[2]; echo[0] = (char)ch; echo[1] = '\0';
                UserAPI::print(echo);
            }
        }
    }
    
    /**
     * @brief Parse command line into command and arguments
     */
    void parse_command() {
        argCount = 0;
        for (u32 i = 0; i < MAX_ARGS; i++) args[i] = nullptr;
        commandBuffer[MAX_COMMAND_LENGTH - 1] = '\0';
        char* ptr = commandBuffer;
        while (*ptr == ' ' && *ptr != '\0' && ptr < commandBuffer + MAX_COMMAND_LENGTH) ptr++;
        while (*ptr != '\0' && argCount < MAX_ARGS - 1 && ptr < commandBuffer + MAX_COMMAND_LENGTH) {
            args[argCount++] = ptr;
            while (*ptr != ' ' && *ptr != '\0' && ptr < commandBuffer + MAX_COMMAND_LENGTH) ptr++;
            if (*ptr == ' ' && ptr < commandBuffer + MAX_COMMAND_LENGTH) {
                *ptr = '\0'; ptr++;
                while (*ptr == ' ' && *ptr != '\0' && ptr < commandBuffer + MAX_COMMAND_LENGTH) ptr++;
            }
        }
        args[argCount] = nullptr;
    }
    
    /**
     * @brief Execute the parsed command
     */
    void execute_command() {
        if (argCount == 0 || args[0] == nullptr) return;
        const char* cmd = args[0];
        if (string_equals(cmd, "help")) { cmd_help(); }
        else if (string_equals(cmd, "about")) { cmd_about(); }
        else if (string_equals(cmd, "clear")) { cmd_clear(); }
        else if (string_equals(cmd, "pwd")) { cmd_pwd(); }
        else if (string_equals(cmd, "ls")) { cmd_ls(); }
        else if (string_equals(cmd, "cat")) { cmd_cat(); }
        else if (string_equals(cmd, "cd")) { cmd_cd(); }
        else if (string_equals(cmd, "mkdir")) { cmd_mkdir(); }
        else if (string_equals(cmd, "rmdir")) { cmd_rmdir(); }
        else if (string_equals(cmd, "exit")) { cmd_exit(); }
        else {
            UserAPI::print_colored("Unknown command: ", Colors::RED_ON_BLUE);
            UserAPI::println(cmd);
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
        UserAPI::println("  * Process management and scheduler");
        UserAPI::println("  * Virtual memory with paging");
        UserAPI::println("  * ATA/IDE disk driver, FAT32");
        UserAPI::println("");
    }
    
    void cmd_clear() {
        for (u32 i = 0; i < 20; i++) UserAPI::println("");
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
            if (result != 0) break;
            if (entry.type == FileSystem::FileType::DIRECTORY) UserAPI::printf("%s    <DIR>\n", entry.name);
            else UserAPI::printf("%s    <FILE>\n", entry.name);
            index++;
        }
        if (index == 0) UserAPI::print("Directory is empty\n");
    }
    
    void cmd_cat() {
        if (argCount < 2) { UserAPI::print_colored("Usage: cat <filename>", Colors::YELLOW_ON_BLUE); UserAPI::println(""); return; }
        const char* filename = args[1];
        char fullPath[256];
        build_absolute_path(filename, fullPath, sizeof(fullPath));
        i32 fd = UserAPI::open(fullPath, static_cast<u32>(FileSystem::OpenFlags::READ_ONLY));
        if (fd < 0) { UserAPI::printf("Error: Could not open file '%s' (err %d)\n", fullPath, fd); return; }
        char buffer[512];
        i32 bytesRead = UserAPI::read_file(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) { buffer[bytesRead] = '\0'; UserAPI::print(buffer); }
        UserAPI::close(fd);
        UserAPI::println("");
    }
    
    void cmd_cd() {
        if (argCount < 2) { UserAPI::print_colored("Usage: cd <directory>", Colors::YELLOW_ON_BLUE); UserAPI::println(""); return; }
        const char* inputPath = args[1];
        char normalized[256];
        build_absolute_path(inputPath, normalized, sizeof(normalized));
        if (normalized[0] == '/' && normalized[1] == '\0') { currentDirectory[0] = '/'; currentDirectory[1] = '\0'; UserAPI::println("Changed directory"); return; }
        if (!directory_exists(normalized)) { UserAPI::print_colored("cd: no such directory: ", Colors::RED_ON_BLUE); UserAPI::println(normalized); return; }
        u32 i = 0; for (; i < MAX_COMMAND_LENGTH - 1 && normalized[i] != '\0'; i++) currentDirectory[i] = normalized[i]; currentDirectory[i] = '\0';
        UserAPI::println("Changed directory");
    }
    
    void cmd_mkdir() {
        if (argCount < 2) { UserAPI::print_colored("Usage: mkdir <directory>", Colors::YELLOW_ON_BLUE); UserAPI::println(""); return; }
        const char* inputPath = args[1];
        char normalized[256]; build_absolute_path(inputPath, normalized, sizeof(normalized));
        i32 result = UserAPI::mkdir(normalized);
        if (result == 0) UserAPI::println("Directory created successfully");
        else UserAPI::printf("mkdir: failed to create %s (error %d)\n", normalized, result);
    }
    
    void cmd_rmdir() {
        if (argCount < 2) { UserAPI::print_colored("Usage: rmdir <directory>", Colors::YELLOW_ON_BLUE); UserAPI::println(""); return; }
        const char* inputPath = args[1];
        char normalized[256]; build_absolute_path(inputPath, normalized, sizeof(normalized));
        i32 result = UserAPI::rmdir(normalized);
        if (result == 0) UserAPI::println("Directory removed successfully");
        else UserAPI::printf("rmdir: failed to remove %s (error %d)\n", normalized, result);
    }
    
    void cmd_exit() {
        UserAPI::print_colored("Exiting shell...\n", Colors::YELLOW_ON_BLUE);
        UserAPI::print_colored("Shell terminated. Goodbye!\n", Colors::GREEN_ON_BLUE);
        UserAPI::exit();
    }

    // ============ UTILITY FUNCTIONS ============
    static bool string_equals(const char* s1, const char* s2) {
        if (!s1 || !s2) return false;
        u32 i = 0; while (i < 32) { if (s1[i] != s2[i]) return false; if (s1[i] == '\0') return true; i++; }
        return false;
    }

    void build_absolute_path(const char* inputPath, char* outPath, u32 outSize);
    bool directory_exists(const char* absPath);
};

// Build an absolute, normalized path from currentDirectory and an input path.
// Handles absolute and relative inputs, "." and ".." components, and collapses duplicate slashes.
void KiraShell::build_absolute_path(const char* inputPath, char* outPath, u32 outSize) {
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
        for (u32 i = 0; currentDirectory[i] != '\0' && pos < sizeof(combined) - 1; i++) {
            combined[pos++] = currentDirectory[i];
        }
        // Ensure single slash separator if not root
        if (pos == 0 || combined[0] != '/') {
            if (pos < sizeof(combined) - 1) combined[pos++] = '/';
        } else if (!(pos == 1 && combined[0] == '/')) {
            if (combined[pos - 1] != '/' && pos < sizeof(combined) - 1) combined[pos++] = '/';
        }
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
            if (compCount > 0) compCount--; // Go up one level
        } else {
            if (compCount < MAX_COMPONENTS) {
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
        outPath[outPos < outSize ? outPos : (outSize - 1)] = '\0';
        return;
    }
    for (u32 idx = 0; idx < compCount; idx++) {
        u32 c = 0;
        while (components[idx][c] != '\0' && outPos < outSize - 1) {
            outPath[outPos++] = components[idx][c++];
        }
        if (idx + 1 < compCount && outPos < outSize - 1) outPath[outPos++] = '/';
    }
    outPath[outPos < outSize ? outPos : (outSize - 1)] = '\0';
}

// Check whether an absolute path refers to an existing directory using READDIR
bool KiraShell::directory_exists(const char* absPath) {
    if (!absPath || absPath[0] != '/') return false;
    FileSystem::DirectoryEntry entry;
    i32 rd = UserAPI::readdir(absPath, 0, &entry);
    if (rd == 0) return true;            // Directory with entries
    if (rd == -7) return false;          // IO_ERROR => not a directory
    if (rd == -5) {                      // FILE_NOT_FOUND ambiguous: try open
        i32 fd = UserAPI::open(absPath, static_cast<u32>(FileSystem::OpenFlags::READ_ONLY));
        if (fd >= 0) { UserAPI::close(fd); return true; }
        return false;
    }
    return false;
}

void user_shell() {
    KiraShell shell;
    shell.run();
}

} // namespace kira::usermode