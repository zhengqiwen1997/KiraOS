#include "libkira.hpp"
#include "user_utils.hpp"

namespace kira::usermode {

using namespace kira::system;
using namespace kira::usermode::util;

void user_cat(const char* path, const char* currentDirectory) {
    if (!path) { UserAPI::print_colored("Usage: cat <filename>\n", Colors::YELLOW_ON_BLUE); return; }
    // For now assume cwd is root; shell can pass absolute paths if needed
    char fullPath[256]; build_absolute_path(currentDirectory, path, fullPath, sizeof(fullPath));
    i32 fd = UserAPI::open(fullPath, static_cast<u32>(FileSystem::OpenFlags::READ_ONLY));
    if (fd < 0) { UserAPI::printf("Error: Could not open file '%s' (err %d)\n", fullPath, fd); return; }
    char buffer[512];
    i32 bytesRead = UserAPI::read_file(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) { buffer[bytesRead] = '\0'; UserAPI::print(buffer); }
    UserAPI::close(fd);
}

} // namespace kira::usermode


