#include "libkira.hpp"
using namespace kira::usermode;

int main(int argc, char** argv) {
    UserAPI::printf("argvtest: argc=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        UserAPI::printf("argv[%d]=%s\n", i, argv[i] ? argv[i] : "(null)");
    }
    // Also show legacy spawnArg for comparison
    char buf[128];
    if (UserAPI::getspawnarg(buf, sizeof(buf)) == 0 && buf[0] != '\0') {
        UserAPI::printf("spawnArg=%s\n", buf);
    }
    return 0;
}
