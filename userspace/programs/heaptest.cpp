#include "libkira.hpp"
#include "core/types.hpp"
using namespace kira::usermode;

int main() {
    void* brk0 = UserAPI::sbrk(0);
    UserAPI::printf("heap: brk0=%x\n", (u32)brk0);
    // Grow by 16KB
    void* old = UserAPI::sbrk(16 * 1024);
    UserAPI::printf("heap: grew from %x to %x\n", (u32)old, (u32)UserAPI::sbrk(0));
    // Touch pages
    volatile u8* p = (volatile u8*)old;
    for (u32 i = 0; i < 16 * 1024; i += 4096) p[i] = (u8)i;
    // Shrink back
    void* old2 = UserAPI::sbrk(-8 * 1024);
    UserAPI::printf("heap: shrank from %x to %x\n", (u32)old2, (u32)UserAPI::sbrk(0));
    // Malloc/calloc/realloc/free smoke test
    char* a = (char*)UserAPI::malloc(1000);
    for (int i = 0; i < 1000; i++) a[i] = 'A';
    char* b = (char*)UserAPI::calloc(200, 2);
    int* c = (int*)UserAPI::realloc(a, sizeof(int) * 2000);
    c[0] = 200;
    UserAPI::printf("heap: malloc/calloc/realloc OK: c[0]=%d b[10]=%d\n", c[0], (int)b[10]);
    UserAPI::free(b);
    UserAPI::free(c);
    return 0;
}


