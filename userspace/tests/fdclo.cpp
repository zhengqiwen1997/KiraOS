#include "libkira.hpp"
using namespace kira::usermode;

extern "C" int main(int argc, char** argv, char** envp) {
    UserAPI::printf("fdclo: start\n");
    int d = UserAPI::dup(1, -1);
    if (d < 0) { UserAPI::printf("fdclo: dup failed %d\n", d); return 1; }
    int r = UserAPI::set_fd_close_on_exec(d, true);
    UserAPI::printf("fdclo: set cloexec fd=%d ret=%d\n", d, r);
    // Build arg string: child expects fd number as argv[1]
    char arg[4]; int v = d; int i = 0; if (v == 0) { arg[i++] = '0'; }
    char tmp[4]; int t = 0; while (v > 0 && t < 4) { tmp[t++] = '0' + (v % 10); v /= 10; }
    while (t > 0) { arg[i++] = tmp[--t]; }
    arg[i] = '\0';
    int pid = UserAPI::exec("/bin/fdexec", arg);
    UserAPI::printf("fdclo: exec ret pid=%d\n", pid);
    int st = UserAPI::wait(static_cast<u32>(pid));
    UserAPI::printf("fdclo: child exit=%d (expect 0)\n", st);
    return 0;
}
