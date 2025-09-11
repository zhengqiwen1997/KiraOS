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
    static constexpr u32 HISTORY_SIZE = 16;
    bool finished = false;
    char currentDirectory[MAX_COMMAND_LENGTH];
    char commandBuffer[MAX_COMMAND_LENGTH];
    char* args[MAX_ARGS];
    u32 argCount; u32 cmdLen;
    // History
    char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
    u32 historyCount = 0; // number of stored commands
    i32 historyIndex = -1; // -1 means not navigating, else index into history
    
public:
    KiraShell() : argCount(0), cmdLen(0) { currentDirectory[0] = '/'; currentDirectory[1] = '\0'; commandBuffer[0] = '\0'; }
    void run() {
        display_welcome();
        // Sync with kernel cwd at start
        UserAPI::getcwd(currentDirectory, sizeof(currentDirectory));
        while (!finished) {
            // UserAPI::sleep(2);
            display_prompt();
            if (!read_command()) break; 
                parse_command();
                execute_command();
        }
    }
private:
    void push_history(const char* line) {
        if (!line || line[0] == '\0') return;
        // Skip duplicate of last entry
        if (historyCount > 0) {
            const char* last = history[(historyCount - 1) % HISTORY_SIZE];
            u32 i = 0; bool same = true; while (line[i] || last[i]) { if (line[i] != last[i]) { same = false; break; } i++; }
            if (same) return;
        }
        u32 slot = historyCount % HISTORY_SIZE;
        // Copy with cap
        u32 i = 0; while (line[i] && i < MAX_COMMAND_LENGTH - 1) { history[slot][i] = line[i]; i++; }
        history[slot][i] = '\0';
        historyCount++;
    }
    void replace_current_line_with(const char* line) {
        // Erase current typed chars visually
        while (cmdLen > 0) { UserAPI::print("\b \b"); cmdLen--; }
        // Copy new line and echo it
        u32 i = 0; while (line && line[i] && i < MAX_COMMAND_LENGTH - 1) { commandBuffer[i] = line[i]; i++; }
        commandBuffer[i] = '\0';
        cmdLen = i;
        UserAPI::print(commandBuffer);
    }
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
        // Ensure prompt starts on a new line
        UserAPI::printf("KiraOS:%s$ ", currentDirectory);
    }
    bool read_command() {
        cmdLen = 0; commandBuffer[0] = '\0';
        historyIndex = -1;
        bool esc = false, bracket = false;
        while (true) {
            i32 ch = UserAPI::getch();
            if (ch == '\n' || ch == '\r') { UserAPI::println(""); commandBuffer[cmdLen] = '\0'; push_history(commandBuffer); return true; }
            // ANSI escape sequence handling for arrows: ESC '[' 'A'/'B'
            if (!esc) {
                if (ch == 27) { esc = true; bracket = false; continue; }
            } else if (!bracket) {
                if (ch == '[') { bracket = true; continue; }
                esc = false; // unknown sequence
            } else {
                // Final byte
                if (ch == 'A' || ch == 'B') {
                    // Up/Down history
                    if (historyCount > 0) {
                        if (historyIndex < 0) historyIndex = static_cast<i32>(historyCount); // start at blank
                        if (ch == 'A') { // Up
                            if (historyIndex > 0) historyIndex--;
                        } else { // 'B' Down
                            if (historyIndex < static_cast<i32>(historyCount)) historyIndex++;
                        }
                        const char* line = (historyIndex >= 0 && historyIndex < static_cast<i32>(historyCount))
                            ? history[historyIndex % HISTORY_SIZE]
                            : ""; // at historyCount => blank
                        replace_current_line_with(line);
                    }
                    esc = bracket = false; continue;
                }
                esc = bracket = false; // unhandled sequence
                continue;
            }
            if ((ch == '\b') && cmdLen > 0) { cmdLen--; commandBuffer[cmdLen] = '\0'; UserAPI::print("\b "); UserAPI::print("\b"); continue; }
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
        // No special casing for 'cat', treat externally
        else if (string_equals(cmd, "cd")) { cmd_cd(); }
        // No special casing for 'mkdir'/'rmdir', treat externally
        else if (string_equals(cmd, "exit")) { cmd_exit(); }
        else {
            // Try exec of /bin/<cmd>; if not present, report unknown. Do not auto-wait.
            char path[64]; u32 p = 0;
            const char* prefix = "/bin/";
            for (; prefix[p] != '\0' && p < 63; p++) path[p] = prefix[p];
            for (u32 i = 0; cmd[i] != '\0' && p < 63; i++, p++) path[p] = cmd[i];
            path[p] = '\0';
            // Join remaining args into a single space-separated raw string
            const char* argStr = nullptr;
            char joined[256];
            if (argCount > 1) {
                u32 jp = 0; joined[0] = '\0';
                for (u32 ai = 1; ai < argCount; ai++) {
                    const char* s = args[ai];
                    for (u32 k = 0; s && s[k] && jp < sizeof(joined) - 1; k++) joined[jp++] = s[k];
                    if (ai + 1 < argCount && jp < sizeof(joined) - 1) joined[jp++] = ' ';
                }
                joined[jp] = '\0';
                argStr = joined;
            }
            i32 pid = UserAPI::exec(path, argStr);
            if (pid < 0) {
                UserAPI::print_colored("Unknown command: ", Colors::RED_ON_BLUE);
                UserAPI::println(cmd);
            } else {
                // Foreground execution: announce and then wait for completion
                (void)UserAPI::wait((u32)pid);                
            }
        }
        // Sync cached cwd from kernel after commands that may modify it
        UserAPI::getcwd(currentDirectory, sizeof(currentDirectory));
    }
    void cmd_help() { UserAPI::print_colored("KiraOS Shell - Available Commands:\n", Colors::YELLOW_ON_BLUE); UserAPI::println("  ls, cat, cd, pwd, mkdir, rmdir, exit"); }
    void cmd_about() { UserAPI::print_colored("KiraOS Operating System\n", Colors::YELLOW_ON_BLUE); UserAPI::println("Monolithic kernel; VFS; FAT32"); }
    void cmd_clear() { for (u32 i = 0; i < 20; i++) UserAPI::println(""); }
    void cmd_pwd() { char buf[256]; UserAPI::getcwd(buf, sizeof(buf)); UserAPI::print_colored("Current directory: ", Colors::WHITE_ON_BLUE); UserAPI::println(buf); }
    void cmd_cd() {
        if (argCount < 2) { UserAPI::print_colored("Usage: cd <directory>", Colors::YELLOW_ON_BLUE); UserAPI::println(""); return; }
        // Build absolute path from currentDirectory and args[1]
        char normalized[256]; 
        build_absolute_path(currentDirectory, args[1], normalized, sizeof(normalized));
        i32 rc = UserAPI::chdir(normalized);
        if (rc == 0) return;
        UserAPI::print_colored("cd: ", Colors::RED_ON_BLUE);
        UserAPI::print_colored(UserAPI::strerror(rc), Colors::RED_ON_BLUE);
        UserAPI::print_colored(": ", Colors::RED_ON_BLUE);
        UserAPI::println(normalized);
    }
    void cmd_exit() { UserAPI::print_colored("Exiting shell...\n", Colors::YELLOW_ON_BLUE); UserAPI::exit(); }
};

void user_shell() { KiraShell shell; shell.run(); }

} // namespace kira::usermode