#include "libkira.hpp"
using namespace kira::usermode;

int main() {
    char cwd[256];
    UserAPI::getcwd(cwd, sizeof(cwd));

    FileSystem::DirectoryEntry entry;
    // Print header to console (not redirected); entries go to stdout
    UserAPI::printf("Directory listing for: %s\n", cwd);
    for (u32 idx = 0;; idx++) {
        i32 r = UserAPI::readdir(cwd, idx, &entry);
        if (r != 0) break;
        char line[512]; u32 lp = 0;
        // name
        for (u32 i = 0; entry.name[i] && lp < sizeof(line) - 1; i++) line[lp++] = entry.name[i];
        const char* suffix = (entry.type == FileSystem::FileType::DIRECTORY) ? "    <DIR>\n" : "    <FILE>\n";
        for (u32 i = 0; suffix[i] && lp < sizeof(line) - 1; i++) line[lp++] = suffix[i];
        UserAPI::write_file(1, line, lp);
    }
    return 0;
}
