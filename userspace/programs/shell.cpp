#include "libkira.hpp"
#include "user_programs.hpp"
#include "core/types.hpp"
#include "user_utils.hpp"

using namespace kira::usermode;
using namespace kira::system;
using namespace kira::usermode::util;

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
    void display_welcome() {
        UserAPI::print_colored("=====================================\n", Colors::CYAN_ON_BLUE);
        UserAPI::print_colored("       KiraOS Interactive Shell v1.0\n", Colors::YELLOW_ON_BLUE);
        UserAPI::print_colored("=====================================\n", Colors::CYAN_ON_BLUE);
        UserAPI::print_colored("Type 'help' for available commands\n", Colors::WHITE_ON_BLUE);
    }
    
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
        UserAPI::print_colored(promptBuffer, Colors::GREEN_ON_BLUE);
    }
    
    bool read_command() {
        cmdLen = 0; commandBuffer[0] = '\0';
        while (true) {
            i32 ch = UserAPI::getch();
            if (ch == 27) { finished = true; UserAPI::println(""); return false; }
            if (ch == '\n' || ch == '\r') { UserAPI::println(""); commandBuffer[cmdLen] = '\0'; return true; }
            if ((ch == '\b') && cmdLen > 0) { cmdLen--; commandBuffer[cmdLen] = '\0'; UserAPI::print("\b"); continue; }
            if (cmdLen < MAX_COMMAND_LENGTH - 1 && ch >= 32 && ch < 127) {
                commandBuffer[cmdLen++] = (char)ch; commandBuffer[cmdLen] = '\0';
                char echo[2]; echo[0] = (char)ch; echo[1] = '\0'; UserAPI::print(echo);
            }
        }
    }
    
    void parse_command() {
        argCount = 0; for (u32 i = 0; i < MAX_ARGS; i++) args[i] = nullptr;
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
    
    void execute_command() {
        if (argCount == 0 || args[0] == nullptr) return;
        const char* cmd = args[0];
        if (string_equals(cmd, "help")) { cmd_help(); }
        else if (string_equals(cmd, "about")) { cmd_about(); }
        else if (string_equals(cmd, "clear")) { cmd_clear(); }
        else if (string_equals(cmd, "pwd")) { cmd_pwd(); }
        else if (string_equals(cmd, "ls")) { user_ls(currentDirectory); }
        else if (string_equals(cmd, "cat")) { if (argCount >= 2) user_cat(args[1], currentDirectory); else UserAPI::println("Usage: cat <filename>"); }
        else if (string_equals(cmd, "cd")) { cmd_cd(); }
        else if (string_equals(cmd, "mkdir")) { if (argCount >= 2) user_mkdir(args[1], currentDirectory); else UserAPI::println("Usage: mkdir <directory>"); }
        else if (string_equals(cmd, "rmdir")) { if (argCount >= 2) user_rmdir(args[1], currentDirectory); else UserAPI::println("Usage: rmdir <directory>"); }
        else if (string_equals(cmd, "exit")) { cmd_exit(); }
        else { UserAPI::print_colored("Unknown command: ", Colors::RED_ON_BLUE); UserAPI::println(cmd); }
    }
    
    void cmd_help() { UserAPI::print_colored("KiraOS Shell - Available Commands:\n", Colors::YELLOW_ON_BLUE);
        UserAPI::print_colored("File System:\n", Colors::CYAN_ON_BLUE);
        UserAPI::println("  ls, cat, cd, pwd, mkdir, rmdir");
        UserAPI::print_colored("Control:\n", Colors::CYAN_ON_BLUE); UserAPI::println("  exit"); }
    
    void cmd_about() { UserAPI::print_colored("KiraOS Operating System\n", Colors::YELLOW_ON_BLUE); UserAPI::println("x86 32-bit; monolithic kernel; FAT32; VFS"); }
    
    void cmd_clear() { for (u32 i = 0; i < 20; i++) UserAPI::println(""); UserAPI::print_colored("Screen cleared\n", Colors::GREEN_ON_BLUE); }
    
    void cmd_pwd() { UserAPI::print_colored("Current directory: ", Colors::WHITE_ON_BLUE); UserAPI::print_colored(currentDirectory, Colors::CYAN_ON_BLUE); UserAPI::println(""); }
    
    void cmd_cd() {
        if (argCount < 2) { UserAPI::print_colored("Usage: cd <directory>", Colors::YELLOW_ON_BLUE); UserAPI::println(""); return; }
        char normalized[256]; build_absolute_path(currentDirectory, args[1], normalized, sizeof(normalized));
        if (normalized[0] == '/' && normalized[1] == '\0') { currentDirectory[0] = '/'; currentDirectory[1] = '\0'; UserAPI::println("Changed directory"); return; }
        if (!directory_exists(normalized)) { UserAPI::print_colored("cd: no such directory: ", Colors::RED_ON_BLUE); UserAPI::println(normalized); return; }
        u32 i = 0; for (; i < MAX_COMMAND_LENGTH - 1 && normalized[i] != '\0'; i++) currentDirectory[i] = normalized[i]; currentDirectory[i] = '\0'; UserAPI::println("Changed directory"); }
    
    void cmd_exit() {
        UserAPI::print_colored("Exiting shell...\n", Colors::YELLOW_ON_BLUE);
        UserAPI::print_colored("Shell terminated. Goodbye!\n", Colors::GREEN_ON_BLUE);
        UserAPI::exit();
    }
};

void user_shell() { KiraShell shell; shell.run(); }

} // namespace kira::usermode