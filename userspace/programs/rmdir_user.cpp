#include "libkira.hpp"
#include "user_utils.hpp"

int main() {
    using namespace kira::usermode;
    using namespace kira::usermode::util;
    char args[256];
    if (UserAPI::getspawnarg(args, sizeof(args)) != 0 || args[0] == '\0') {
        UserAPI::print_colored("rmdir: missing operand\n", Colors::RED_ON_BLUE);
        return 0;
    }
    char cwd[256];
    UserAPI::getcwd(cwd, sizeof(cwd));
    // Parse space-separated list
    u32 i = 0;
    while (args[i] != '\0') {
        while (args[i] == ' ') i++;
        if (args[i] == '\0') break;
        char token[128]; u32 tp = 0;
        while (args[i] != ' ' && args[i] != '\0' && tp < sizeof(token)-1) token[tp++] = args[i++];
        token[tp] = '\0';
        if (tp > 0) {
            char full[256];
            build_absolute_path(cwd, token, full, sizeof(full));
            i32 rc = UserAPI::rmdir(full);
            if (rc != 0) { UserAPI::print_colored("rmdir: cannot remove ", Colors::RED_ON_BLUE); UserAPI::println(token); }
        }
    }
    return 0;
}
