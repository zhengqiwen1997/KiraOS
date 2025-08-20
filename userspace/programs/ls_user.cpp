#include "libkira.hpp"

extern "C" void user_entry() {
    using namespace kira::usermode;
    using namespace kira::system;

    char cwd[256];
    UserAPI::getcwd(cwd, sizeof(cwd));

    FileSystem::DirectoryEntry entry;
    UserAPI::printf("Directory listing for: %s\n", cwd);
    for (u32 idx = 0;; idx++) {
        i32 r = UserAPI::readdir(cwd, idx, &entry);
        if (r != 0) break;
        if (entry.type == FileSystem::FileType::DIRECTORY) {
            UserAPI::printf("%s    <DIR>\n", entry.name);
        } else {
            UserAPI::printf("%s    <FILE>\n", entry.name);
        }
    }
}


