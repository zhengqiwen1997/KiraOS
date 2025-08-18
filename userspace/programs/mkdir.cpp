#include "libkira.hpp"
#include "user_utils.hpp"

namespace kira::usermode {

using namespace kira::system;
using namespace kira::usermode::util;

void user_mkdir(const char* path, const char* currentDirectory) {
    if (!path) { UserAPI::print_colored("Usage: mkdir <directory>\n", Colors::YELLOW_ON_BLUE); return; }
    char fullPath[256]; build_absolute_path(currentDirectory, path, fullPath, sizeof(fullPath));
    i32 result = UserAPI::mkdir(fullPath);
    if (result == 0) UserAPI::println("Directory created successfully");
    else UserAPI::printf("mkdir: failed to create %s (error %d)\n", fullPath, result);
}

} // namespace kira::usermode


