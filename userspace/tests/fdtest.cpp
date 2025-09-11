#include "libkira.hpp"
using namespace kira::usermode;

extern "C" int main(int argc, char** argv, char** envp) {
    UserAPI::printf("fdtest: start\n");
    // Open an existing file from /bin
    int fd = UserAPI::open("/bin/ls", 0); // read-only
    if (fd < 0) { UserAPI::printf("fdtest: open failed %d\n", fd); return 1; }
    int dupfd = UserAPI::dup(fd, -1);
    if (dupfd < 0) { UserAPI::printf("fdtest: dup failed %d\n", dupfd); return 2; }

    char buf[32];
    int r1 = UserAPI::read_file(fd, buf, sizeof(buf)-1);
    int r2 = UserAPI::read_file(dupfd, buf, sizeof(buf)-1);
    UserAPI::printf("fdtest: read fd=%d=%d dupfd=%d=%d\n", fd, r1, dupfd, r2);

    // Close original; dup should still be valid
    UserAPI::close(fd);
    int r3 = UserAPI::read_file(dupfd, buf, sizeof(buf)-1);
    UserAPI::printf("fdtest: after close original, dup read=%d\n", r3);

    UserAPI::close(dupfd);
    UserAPI::printf("fdtest: done\n");
    return 0;
}
