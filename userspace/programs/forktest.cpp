#include "core/types.hpp"
#include "libkira.hpp"
using namespace kira::usermode;

static volatile i32 gValue = 42; // Demonstrate CoW on data segment

int main() {
    i32 x = 7; // Demonstrate CoW on stack
    UserAPI::printf("pre-fork: pid=%u g=%d x=%d\n", UserAPI::get_pid(), (i32)gValue, x);

    i32 pid = UserAPI::fork();
    if (pid < 0) {
        UserAPI::printf("fork failed %d (%s)\n", pid, UserAPI::strerror(pid));
        return 1;
    }

    if (pid == 0) {
        // Child: modify both variables; should not affect parent
        x = 99;
        gValue = 1000;
        UserAPI::printf("child after write: pid=%u g=%d x=%d\n", UserAPI::get_pid(), (i32)gValue, x);
        return 0;
    }

    // Parent
    i32 st = -1;
    i32 cpid = UserAPI::waitid((u32)pid, &st);
    UserAPI::printf("parent after wait: pid=%u g=%d x=%d (child=%d st=%d)\n",
                    UserAPI::get_pid(), (i32)gValue, x, cpid, st);
    UserAPI::printf("forktest: done\n");
    return 0;
}


