#include "libkira.hpp"

extern "C" void user_entry() {
    using namespace kira::usermode;
    using namespace kira::system;
    char args[256];
    if (UserAPI::getspawnarg(args, sizeof(args)) != 0 || args[0] == '\0') {
        UserAPI::print_colored("rmdir: missing operand\n", Colors::RED_ON_BLUE);
        return;
    }
    // Parse space-separated list
    u32 i = 0;
    while (args[i] != '\0') {
        while (args[i] == ' ') i++;
        if (args[i] == '\0') break;
        char token[128]; u32 tp = 0;
        while (args[i] != ' ' && args[i] != '\0' && tp < sizeof(token)-1) token[tp++] = args[i++];
        token[tp] = '\0';
        if (tp > 0) {
            i32 rc = UserAPI::rmdir(token);
            if (rc != 0) { UserAPI::print_colored("rmdir: cannot remove ", Colors::RED_ON_BLUE); UserAPI::println(token); }
        }
    }
}


