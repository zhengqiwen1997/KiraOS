#include "libkira.hpp"

namespace kira::usermode {

using namespace kira::system;

// Simple standalone 'ls' utility
void user_ls(const char* currentDirectory) {
    UserAPI::printf("Directory listing for: %s\n", currentDirectory);

    FileSystem::DirectoryEntry entry;
    u32 index = 0;
    while (true) {
        i32 result = UserAPI::readdir(currentDirectory, index, &entry);
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
    // Return to caller (shell or standalone harness)
    return;
}

} // namespace kira::usermode


