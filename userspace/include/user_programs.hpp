#pragma once

namespace kira::usermode {

// Individual user programs
void user_test_simple();
void user_test_syscall();
void user_test_sleep();
void user_test_input();

// Interactive shell
void user_shell();

// Standalone utilities
void user_ls(const char* currentDirectory);
void user_cat(const char* path, const char* currentDirectory);
void user_mkdir(const char* path, const char* currentDirectory);
void user_rmdir(const char* path, const char* currentDirectory);

} // namespace kira::usermode 