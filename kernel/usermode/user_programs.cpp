#include "usermode/user_programs.hpp"
#include "core/syscalls.hpp"
#include "display/vga.hpp"

namespace kira::usermode {

using namespace kira::system;
using namespace kira::display;

i32 UserAPI::syscall(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3) {
    // Use direct kernel calls for now since we're in hybrid mode
    return kira::system::handle_syscall(syscall_num, arg1, arg2, arg3);
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

// User mode program implementations using system calls
void user_hello_world() {
    static int iteration = 0;
    
    if (iteration == 0) {
        u32 pid = UserAPI::get_pid();
        UserAPI::write(17, 0, "Hello from user mode! PID: ");
        
        // Simple PID display (assuming single digit for now)
        char pid_str[2] = {static_cast<char>('0' + (pid % 10)), '\0'};
        UserAPI::write(17, 28, pid_str);
    }
    
    iteration++;
    
    if (iteration > 50) {  // Run for 50 iterations
        UserAPI::write(17, 35, "Exiting...");
        // For now, just mark as done instead of exiting
        static bool done = false;
        if (!done) {
            done = true;
            UserAPI::exit();
        }
    }
}

void user_counter_program() {
    static u32 counter = 0;
    static char display_buffer[50];
    
    counter++;
    
    u32 pid = UserAPI::get_pid();
    
    // Build display string
    const char* prefix = "User Counter PID ";
    int pos = 0;
    
    // Copy prefix
    for (int i = 0; prefix[i] != '\0'; i++) {
        display_buffer[pos++] = prefix[i];
    }
    
    // Add PID
    display_buffer[pos++] = static_cast<char>('0' + (pid % 10));
    display_buffer[pos++] = ':';
    display_buffer[pos++] = ' ';
    
    // Add counter (2 digits)
    display_buffer[pos++] = static_cast<char>('0' + ((counter / 10) % 10));
    display_buffer[pos++] = static_cast<char>('0' + (counter % 10));
    display_buffer[pos] = '\0';
    
    UserAPI::write(18, 0, display_buffer);
    
    if (counter > 50) {
        UserAPI::write(18, 25, "DONE");
        UserAPI::exit();
    }
}

void user_interactive_program() {
    static int animation_counter = 0;
    static char display_buffer[50];
    
    const char* animation = "|/-\\";
    
    u32 pid = UserAPI::get_pid();
    
    // Build display string
    const char* prefix = "Interactive Program PID ";
    int pos = 0;
    
    // Copy prefix
    for (int i = 0; prefix[i] != '\0'; i++) {
        display_buffer[pos++] = prefix[i];
    }
    
    // Add PID
    display_buffer[pos++] = static_cast<char>('0' + (pid % 10));
    display_buffer[pos++] = ' ';
    
    // Add animation character
    display_buffer[pos++] = animation[animation_counter % 4];
    display_buffer[pos] = '\0';
    
    UserAPI::write(19, 0, display_buffer);
    
    animation_counter++;
    
    if (animation_counter > 40) {
        UserAPI::write(19, 30, "Finished!");
        UserAPI::exit();
    }
}

} // namespace kira::usermode 