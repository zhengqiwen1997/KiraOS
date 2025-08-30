#include "libkira.hpp"
using namespace kira::usermode;

int main() {
    // Test 1: waitid on a specific child pid
    i32 pid1 = UserAPI::exec("/bin/exitcode", nullptr);
    UserAPI::printf("waitidtest: spawned pid1=%d (exitcode)\n", pid1);
    if (pid1 < 0) {
        UserAPI::printf("waitidtest: exec failed %d (%s)\n", pid1, UserAPI::strerror(pid1));
        return 1;
    }
    i32 status1 = -999;
    i32 r1 = UserAPI::waitid((u32)pid1, &status1);
    UserAPI::printf("waitidtest: waitid(pid1) -> ret=%d status=%d\n", r1, status1);

    // Test 2: waitid on any child (pid=0), with two children
    i32 pid2 = UserAPI::exec("/bin/exitcode", nullptr);
    i32 pid3 = UserAPI::exec("/bin/ls", nullptr);
    UserAPI::printf("waitidtest: spawned pid2=%d pid3=%d (exitcode)\n", pid2, pid3);
    if (pid2 < 0 || pid3 < 0) {
        UserAPI::printf("waitidtest: exec failed pid2=%d pid3=%d\n", pid2, pid3);
        return 2;
    }
    i32 stA = -999, stB = -999;
    i32 cpidA = UserAPI::waitid(0, &stA);
    i32 cpidB = UserAPI::waitid(0, &stB);
    UserAPI::printf("waitidtest: waitid(ANY) -> cpidA=%d statusA=%d\n", cpidA, stA);
    UserAPI::printf("waitidtest: waitid(ANY) -> cpidB=%d statusB=%d\n", cpidB, stB);
    return 0;
}


