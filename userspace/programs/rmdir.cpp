#include "libkira.hpp"
#include "user_utils.hpp"

namespace kira::usermode {

using namespace kira::system;
using namespace kira::usermode::util;

void user_rmdir(const char* path, const char* currentDirectory) {
    if (!path) { UserAPI::print_colored("Usage: rmdir <directory>\n", Colors::YELLOW_ON_BLUE); return; }
    char fullPath[256]; build_absolute_path(currentDirectory, path, fullPath, sizeof(fullPath));
    i32 result = UserAPI::rmdir(fullPath);
    if (result == 0) UserAPI::println("Directory removed successfully");
    else UserAPI::printf("rmdir: failed to remove %s (error %d)\n", fullPath, result);
}

} // namespace kira::usermode


