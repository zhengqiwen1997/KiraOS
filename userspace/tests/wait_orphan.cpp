#include "core/types.hpp"
#include "libkira.hpp"
using namespace kira::usermode;

int main() {
    // Parent forks; child sleeps and exits, parent exits immediately
    i32 pid = UserAPI::fork();
    if (pid < 0) {
        UserAPI::printf("fork failed %d (%s)\n", pid, UserAPI::strerror(pid));
        return 1;
    }
    if (pid == 0) {
        // Child prints immediately, then sleeps and exits quietly.
        UserAPI::printf("orphan child starting: pid=%u\n", UserAPI::get_pid());
        UserAPI::sleep(5);
        return 77;
    }
    // Parent: exit immediately without waiting -> creates orphan
    UserAPI::printf("parent exiting to orphan child pid=%d\n", pid);
    return 0;
}


