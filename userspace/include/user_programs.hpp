#pragma once

namespace kira::usermode {

// Individual user programs
void user_test_simple();
void user_test_syscall();
void user_test_sleep();
void user_test_input();

// Interactive shell
void user_shell();

} // namespace kira::usermode 