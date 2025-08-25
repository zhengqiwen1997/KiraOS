#include "libkira.hpp"
using namespace kira::usermode;

int main() {
    i32 pid = UserAPI::exec("/bin/exitcode", nullptr);
    UserAPI::printf("waittest: pid=%d\n", pid);
    if (pid < 0) {
        UserAPI::printf("waittest: exec failed %d (%s)\n", pid, UserAPI::strerror(pid));
        return 1;
    }
    i32 st = UserAPI::wait((u32)pid);
    UserAPI::printf("waittest: child pid=%d status=%d\n", pid, st);
    return st;
}

