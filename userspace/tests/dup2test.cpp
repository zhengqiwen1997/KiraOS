#include "libkira.hpp"
using namespace kira::usermode;

extern "C" int main(int argc, char** argv, char** envp) {
    const char* path = "/dup2_out.txt";
    // WRITE_ONLY | CREATE | TRUNCATE
    u32 flags = 0x01u | 0x40u | 0x200u;
    int fd = UserAPI::open(path, flags);
    if (fd < 0) {
        UserAPI::printf("dup2test: open failed %d\n", fd);
        return 1;
    }
    int r = UserAPI::dup2(fd, 1);
    UserAPI::printf("dup2test: dup2 ret=%d to stdout\n", r);
    // Write a known ASCII-only payload; avoid % and control chars
    const char* line = "HELLO_FROM_DUP2TEST\n";
    (void)UserAPI::write_file(1, line, 20);
    UserAPI::close(fd);
    return 0;
}
