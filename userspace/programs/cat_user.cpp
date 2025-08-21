#include "libkira.hpp"

int main() {
    using namespace kira::usermode;
    using namespace kira::system;
    char args[256];
    if (UserAPI::getspawnarg(args, sizeof(args)) != 0 || args[0] == '\0') {
        UserAPI::print_colored("cat: missing operand\n", Colors::RED_ON_BLUE);
        return 0;
    }
    // Iterate tokens and print sequentially
    u32 i = 0;
    while (args[i] != '\0') {
        while (args[i] == ' ') i++;
        if (args[i] == '\0') break;
        char token[128]; u32 tp = 0;
        while (args[i] != ' ' && args[i] != '\0' && tp < sizeof(token)-1) token[tp++] = args[i++];
        token[tp] = '\0';
        if (tp > 0) {
            i32 fd = UserAPI::open(token, static_cast<u32>(FileSystem::OpenFlags::READ_ONLY));
            if (fd < 0) { UserAPI::print_colored("cat: cannot open ", Colors::RED_ON_BLUE); UserAPI::println(token); }
            else {
                char buf[512]; i32 n = UserAPI::read_file(fd, buf, sizeof(buf) - 1);
                if (n > 0) { buf[n] = '\0'; UserAPI::print(buf); }
                UserAPI::close(fd);
            }
        }
    }
    return 0;
}
