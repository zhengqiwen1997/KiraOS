#include "libkira.hpp"

namespace kira::usermode {

// System call test program
void user_test_syscall() {
    UserAPI::print_colored("[sleep-test] started", Colors::YELLOW_ON_BLUE);
    for (u32 i = 0; i < 3; i++) {
        UserAPI::printf("[sleep-test] tick %u before sleep\n", i);
        // Sleep 5 ticks, expect to resume at exact next instruction
        UserAPI::sleep(5);
        UserAPI::printf("[sleep-test] resumed at tick %u\n", i);
    }
    UserAPI::print_colored("[sleep-test] done", Colors::GREEN_ON_BLUE);
    UserAPI::exit();
}

} // namespace kira::usermode 