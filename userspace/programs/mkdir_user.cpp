#include "libkira.hpp"

int main() {
    using namespace kira::usermode;
    using namespace kira::system;
    char args[256];
    if (UserAPI::getspawnarg(args, sizeof(args)) != 0 || args[0] == '\0') {
        UserAPI::print_colored("mkdir: missing operand\n", Colors::RED_ON_BLUE);
        return 0;
    }
    // Parse space-separated list
    u32 i = 0;
    while (args[i] != '\0') {
        // Skip spaces
        while (args[i] == ' ') i++;
        if (args[i] == '\0') break;
        // Extract token
        char token[128]; u32 tp = 0;
        while (args[i] != ' ' && args[i] != '\0' && tp < sizeof(token)-1) token[tp++] = args[i++];
        token[tp] = '\0';
        if (tp > 0) {
            i32 rc = UserAPI::mkdir(token);
            if (rc != 0) { UserAPI::print_colored("mkdir: cannot create ", Colors::RED_ON_BLUE); UserAPI::println(token); }
        }
    }
    return 0;
}
