#include "libkira.hpp"
#include "core/types.hpp"
using namespace kira::usermode;

int main() {
    // Allocate a page-sized buffer and initialize
    u32 len = 4096;
    u8* buf = (u8*)UserAPI::malloc(len);
    if (!buf) { UserAPI::println("heap_cow: malloc failed"); return 1; }
    for (u32 i = 0; i < len; i += 256) buf[i] = 0x11;

    i32 pid = UserAPI::fork();
    if (pid < 0) { UserAPI::println("heap_cow: fork failed"); return 1; }
    if (pid == 0) {
        // Child: modify its copy; should trigger CoW
        for (u32 i = 0; i < len; i += 256) buf[i] = 0xAA;
        UserAPI::println("heap_cow: child wrote");
        return 0;
    }
    // Parent: wait and then verify unchanged
    i32 st = -1; (void)UserAPI::waitid((u32)pid, &st);
    u32 mismatches = 0;
    for (u32 i = 0; i < len; i += 256) if (buf[i] != 0x11) mismatches++;
    UserAPI::printf("heap_cow: parent verify mismatches=%u\n", mismatches);
    UserAPI::free(buf);
    return mismatches == 0 ? 0 : 2;
}


