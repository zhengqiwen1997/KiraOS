#include "libkira.hpp"
#include "core/syscalls.hpp"

namespace kira::usermode {

using namespace kira::system;

i32 UserAPI::syscall(u32 syscallNum, u32 arg1, u32 arg2, u32 arg3) {
    i32 result;
    
    // Make real system call using INT 0x80 from Ring 3 to Ring 0
    // This will trigger our system call interrupt handler
    asm volatile(
        "int $0x80"
        : "=a" (result)                    // Output: result in EAX
        : "a" (syscallNum),               // Input: syscall number in EAX
          "b" (arg1),                      // Input: arg1 in EBX
          "c" (arg2),                      // Input: arg2 in ECX
          "d" (arg3)                       // Input: arg3 in EDX
        : "memory"                         // Clobber: memory
    );
    
    return result;
}

// Enhanced print functions
i32 UserAPI::print(const char* text) {
    if (!text) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    return syscall(static_cast<u32>(SystemCall::WRITE_COLORED), (u32)text, Colors::DEFAULT);
}

i32 UserAPI::println(const char* text) {
    if (!text) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    
    // Calculate length of text
    u32 textLen = 0;
    while (text[textLen] != '\0') {
        textLen++;
    }
    
    // Use a reasonable stack buffer size
    constexpr u32 MAX_MESSAGE_LEN = 256;
    char buffer[MAX_MESSAGE_LEN];
    
    // Check if message fits in buffer
    if (textLen + 1 >= MAX_MESSAGE_LEN) {
        // Message too long, fall back to separate calls
        i32 result = print(text);
        if (result != 0) return result;
        return print("\n");
    }
    
    // Copy text
    for (u32 i = 0; i < textLen; i++) {
        buffer[i] = text[i];
    }
    
    // Add newline and null terminator
    buffer[textLen] = '\n';
    buffer[textLen + 1] = '\0';
    
    // Send as single message
    return syscall(static_cast<u32>(SystemCall::WRITE_COLORED), (u32)buffer, Colors::DEFAULT);
}

i32 UserAPI::print_colored(const char* text, u16 color) {
    if (!text) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    return syscall(static_cast<u32>(SystemCall::WRITE_COLORED), (u32)text, color);
}

i32 UserAPI::write_colored(const char* text, u16 color) {
    return print_colored(text, color);
}

// Legacy write function (for backward compatibility)
i32 UserAPI::write(u32 line, u32 column, const char* text) {
    // For now, ignore line and column parameters and use console system
    // In the future, this could be enhanced to support positioned output
    return print(text);
}

// Existing functions
u32 UserAPI::get_pid() {
    return static_cast<u32>(syscall(static_cast<u32>(SystemCall::GET_PID)));
}

i32 UserAPI::yield() {
    return syscall(static_cast<u32>(SystemCall::YIELD));
}

i32 UserAPI::sleep(u32 ticks) {
    return syscall(static_cast<u32>(SystemCall::SLEEP), ticks);
}

void UserAPI::exit() {
    syscall(static_cast<u32>(SystemCall::EXIT));
    // Should not return, but just in case
    while (true) {
        asm volatile("hlt");
    }
}

} // namespace kira::usermode 