#include "libkira.hpp"
using namespace kira::usermode;

// Child program prints a message to fd (should be closed if CLOEXEC set)
extern "C" int main(int argc, char** argv, char** envp) {
    // argv[0] is program name; if argv[1] present, interpret as fd number to write
    int fd = 1; // default stdout
    if (argc > 1) {
        // crude parse
        const char* s = argv[1]; int v = 0; while (*s) { v = v*10 + (*s - '0'); s++; }
        fd = v;
    }
    const char* msg = "fdexec child: writing on fd\n";
    int w = UserAPI::write_file(fd, msg, 26);
    UserAPI::printf("fdexec child: write ret=%d (expect -2 or 26)\n", w);
    return 0;
}
