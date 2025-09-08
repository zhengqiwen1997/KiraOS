#include "core/types.hpp"
#include "libkira.hpp"
using namespace kira::usermode;

static constexpr u32 PAGE = 4096;
static constexpr u32 DATA_SIZE = 128 * 1024;  // 128KB -> 32 pages
static constexpr u32 STACK_TOUCH = 16 * 1024; // 16KB -> 4 pages (stack is 4KB, but touch in steps)

// Large global buffer to exercise .data/.bss CoW
static volatile u8 gData[DATA_SIZE];

int main() {
    // Initialize data so parent has known contents
    for (u32 i = 0; i < DATA_SIZE; i += PAGE) {
        gData[i] = (u8)(i / PAGE);
    }

    UserAPI::printf("cowstress: pre-fork\n");
    i32 pid = UserAPI::fork();
    if (pid < 0) {
        UserAPI::printf("cowstress: fork failed %d (%s)\n", pid, UserAPI::strerror(pid));
        return 1;
    }
    if (pid == 0) {
        // Child: write to many distinct pages in data and stack to trigger CoW
        for (u32 i = 0; i < DATA_SIZE; i += PAGE) {
            gData[i] ^= 0x5A;
        }
        // Touch stack in page steps
        volatile u8 stackBuf[STACK_TOUCH];
        for (u32 i = 0; i < STACK_TOUCH; i += PAGE) {
            stackBuf[i] = (u8)(i ^ 0xA5);
        }
        UserAPI::printf("cowstress: child done\n");
        return 0;
    }

    // Parent: wait to ensure foreground behavior
    i32 st = -1;
    (void)UserAPI::waitid((u32)pid, &st);
    UserAPI::printf("cowstress: parent done (child st=%d)\n", st);
    return 0;
}


