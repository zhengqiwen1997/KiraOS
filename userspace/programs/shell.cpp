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
    u32 argCount; u32 cmdLen;
    
public:
    KiraShell() : argCount(0), cmdLen(0) { currentDirectory[0] = '/'; currentDirectory[1] = '\0'; commandBuffer[0] = '\0'; }
    void run() {
        display_welcome();
        // Sync with kernel cwd at start
        UserAPI::getcwd(currentDirectory, sizeof(currentDirectory));
        while (!finished) {
            UserAPI::sleep(1);
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
        // char promptBuffer[64]; u32 pos = 0; for (u32 i = 0; i < 64; i++) promptBuffer[i] = '\0';
        // const char* parts[] = {"KiraOS:", currentDirectory, "$ "};
        // for (u32 part = 0; part < 3; part++) { const char* s = parts[part]; for (u32 i = 0; s[i] != '\0' && pos < 63; i++) promptBuffer[pos++] = s[i]; }
        // UserAPI::print_colored(promptBuffer, Colors::GREEN_ON_BLUE);
        UserAPI::printf("KiraOS:%s$ ", currentDirectory);
    }
    bool read_command() {
        cmdLen = 0; commandBuffer[0] = '\0';
        while (true) {
            i32 ch = UserAPI::getch();
            if (ch == 27) { finished = true; UserAPI::println(""); return false; }
            if (ch == '\n' || ch == '\r') { UserAPI::println(""); commandBuffer[cmdLen] = '\0'; return true; }
            if ((ch == '\b') && cmdLen > 0) { cmdLen--; commandBuffer[cmdLen] = '\0'; UserAPI::print("\b"); continue; }
            if (cmdLen < MAX_COMMAND_LENGTH - 1 && ch >= 32 && ch < 127) { commandBuffer[cmdLen++] = (char)ch; commandBuffer[cmdLen] = '\0'; char echo[2]; echo[0] = (char)ch; echo[1] = '\0'; UserAPI::print(echo); }
        }
    }
    void parse_command() {
        argCount = 0; for (u32 i = 0; i < MAX_ARGS; i++) args[i] = nullptr; commandBuffer[MAX_COMMAND_LENGTH - 1] = '\0'; char* p = commandBuffer;
        while (*p == ' ' && *p != '\0' && p < commandBuffer + MAX_COMMAND_LENGTH) p++;
        while (*p != '\0' && argCount < MAX_ARGS - 1 && p < commandBuffer + MAX_COMMAND_LENGTH) {
            args[argCount++] = p; while (*p != ' ' && *p != '\0' && p < commandBuffer + MAX_COMMAND_LENGTH) p++;
            if (*p == ' ' && p < commandBuffer + MAX_COMMAND_LENGTH) { *p = '\0'; p++; while (*p == ' ' && *p != '\0' && p < commandBuffer + MAX_COMMAND_LENGTH) p++; }
        }
        args[argCount] = nullptr;
    }
    void execute_command() {
        if (argCount == 0 || args[0] == nullptr) return; const char* cmd = args[0];
        if (string_equals(cmd, "help")) { cmd_help(); }
        else if (string_equals(cmd, "about")) { cmd_about(); }
        else if (string_equals(cmd, "clear")) { cmd_clear(); }
        else if (string_equals(cmd, "pwd")) { cmd_pwd(); }
        else if (string_equals(cmd, "ls")) { UserAPI::spawn(0, reinterpret_cast<u32>(currentDirectory)); }
        else if (string_equals(cmd, "cat")) { if (argCount >= 2) user_cat(args[1], currentDirectory); else UserAPI::println("Usage: cat <filename>"); }
        else if (string_equals(cmd, "cd")) { cmd_cd(); }
        else if (string_equals(cmd, "mkdir")) { if (argCount >= 2) user_mkdir(args[1], currentDirectory); else UserAPI::println("Usage: mkdir <directory>"); }
        else if (string_equals(cmd, "rmdir")) { if (argCount >= 2) user_rmdir(args[1], currentDirectory); else UserAPI::println("Usage: rmdir <directory>"); }
        else if (string_equals(cmd, "exit")) { cmd_exit(); }
        else { UserAPI::print_colored("Unknown command: ", Colors::RED_ON_BLUE); UserAPI::println(cmd); }
        // Sync cached cwd from kernel after commands that may modify it
        UserAPI::getcwd(currentDirectory, sizeof(currentDirectory));
    }
    void cmd_help() { UserAPI::print_colored("KiraOS Shell - Available Commands:\n", Colors::YELLOW_ON_BLUE); UserAPI::println("  ls, cat, cd, pwd, mkdir, rmdir, exit"); }
    void cmd_about() { UserAPI::print_colored("KiraOS Operating System\n", Colors::YELLOW_ON_BLUE); UserAPI::println("Monolithic kernel; VFS; FAT32"); }
    void cmd_clear() { for (u32 i = 0; i < 20; i++) UserAPI::println(""); }
    void cmd_pwd() { char buf[256]; UserAPI::getcwd(buf, sizeof(buf)); UserAPI::print_colored("Current directory: ", Colors::WHITE_ON_BLUE); UserAPI::println(buf); }
    void cmd_cd() {
        if (argCount < 2) { UserAPI::print_colored("Usage: cd <directory>", Colors::YELLOW_ON_BLUE); UserAPI::println(""); return; }
        // Build absolute path from currentDirectory and args[1] for now (until kernel supports relative chdir)
        char normalized[256]; build_absolute_path(currentDirectory, args[1], normalized, sizeof(normalized));
        if (UserAPI::chdir(normalized) != 0) { UserAPI::print_colored("cd: failed\n", Colors::RED_ON_BLUE); }
    }
    void cmd_exit() { UserAPI::print_colored("Exiting shell...\n", Colors::YELLOW_ON_BLUE); UserAPI::exit(); }
    static bool string_equals(const char* s1, const char* s2) { if (!s1 || !s2) return false; u32 i = 0; while (i < 32) { if (s1[i] != s2[i]) return false; if (s1[i] == '\0') return true; i++; } return false; }
};

void user_shell() { KiraShell shell; shell.run(); }

} // namespace kira::usermode