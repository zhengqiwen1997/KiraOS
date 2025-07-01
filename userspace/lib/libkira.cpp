#include "libkira.hpp"
#include "core/syscalls.hpp"

namespace kira::usermode {

using namespace kira::system;

i32 UserAPI::syscall(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3) {
    i32 result;
    
    // Make real system call using INT 0x80 from Ring 3 to Ring 0
    // This will trigger our system call interrupt handler
    asm volatile(
        "int $0x80"
        : "=a" (result)                    // Output: result in EAX
        : "a" (syscall_num),               // Input: syscall number in EAX
          "b" (arg1),                      // Input: arg1 in EBX
          "c" (arg2),                      // Input: arg2 in ECX
          "d" (arg3)                       // Input: arg3 in EDX
        : "memory"                         // Clobber: memory
    );
    
    return result;
}

i32 UserAPI::write(u32 line, u32 column, const char* text) {
    return syscall(static_cast<u32>(SystemCall::WRITE), line, column, (u32)text);
}

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