#include "libkira.hpp"

namespace kira::usermode {

using namespace kira::system;

// Simple standalone 'ls' utility
void user_ls(const char* currentDirectory) {
    const char* cwd = currentDirectory ? currentDirectory : "/";
   UserAPI::printf("Directory listing for currentDirectory: %s\n", currentDirectory);

    FileSystem::DirectoryEntry entry;
    u32 index = 0;
    while (true) {
        i32 result = UserAPI::readdir(cwd, index, &entry);
        if (result != 0) break;
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
    // Ensure final newline after output block
    // UserAPI::println("");
}

void user_ls_main() {
    char cwd[256];
    UserAPI::getcwd(cwd, sizeof(cwd));
    user_ls(cwd);
    UserAPI::exit();
}
} // namespace kira::usermode


