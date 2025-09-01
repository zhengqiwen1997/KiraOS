#include "core/types.hpp"
#include "libkira.hpp"
using namespace kira::usermode;

int main() {
    i32 pid = UserAPI::fork();
    if (pid < 0) {
        UserAPI::printf("fork failed %d (%s)\n", pid, UserAPI::strerror(pid));
        return 1;
    }
    if (pid == 0) {
        // Child
        UserAPI::printf("child: pid=%u parent continues\n", UserAPI::get_pid());
        return 3;
    } else {
        // Parent
        UserAPI::printf("parent: forked child pid=%d\n", pid);
        // Wait for the child to avoid interleaving output with the shell prompt
        i32 st = -1;
        i32 cpid = UserAPI::waitid((u32)pid, &st);
        UserAPI::printf("parent: waitid ret pid=%d status=%d\n", cpid, st);
        UserAPI::printf("forktest: done\n");
        return 0;
    }
    
}


