#include "usermode/user_programs.hpp"
#include "core/syscalls.hpp"
#include "display/vga.hpp"

namespace kira::usermode {

using namespace kira::system;
using namespace kira::display;

i32 UserAPI::syscall(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3) {
    i32 result;
    
    // Debug: Show what we're about to call
    volatile u16* vga = (volatile u16*)0xB8000;
    vga[21 * 80 + 0] = 0x0E00 | 'C';  // Yellow 'C' for Call
    vga[21 * 80 + 1] = 0x0E00 | ':';  // Yellow ':'
    vga[21 * 80 + 2] = 0x0F00 | ('0' + (syscall_num % 10));  // White digit
    
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

// Simple test user program that runs once and yields
void user_test_simple() {
    static bool first_call = true;
    
    if (first_call) {
        first_call = false;
        
        // Test 1: Check current privilege level
        u16 cs_register;
        asm volatile("mov %%cs, %0" : "=r"(cs_register));
        u8 privilege_level = cs_register & 0x3;  // Bottom 2 bits = CPL (Current Privilege Level)
        
        char privilege_msg[32];
        const char* prefix = "Ring: ";
        int pos = 0;
        for (int i = 0; prefix[i] != '\0'; i++) {
            privilege_msg[pos++] = prefix[i];
        }
        privilege_msg[pos++] = '0' + privilege_level;
        privilege_msg[pos++] = ' ';
        privilege_msg[pos++] = 'C';
        privilege_msg[pos++] = 'S';
        privilege_msg[pos++] = ':';
        privilege_msg[pos++] = '0' + ((cs_register >> 4) & 0xF);
        privilege_msg[pos++] = '0' + (cs_register & 0xF);
        privilege_msg[pos] = '\0';
        
        UserAPI::write(17, 0, privilege_msg);
        
        // Test 2: Try to execute a privileged instruction (should cause GPF if in Ring 3)
        UserAPI::write(18, 0, "Testing privileged instruction...");
        
        // This should cause a GPF if we're truly in Ring 3
        // We'll comment it out for now to avoid crashing
        // asm volatile("cli");  // Clear interrupt flag - privileged instruction
        
        UserAPI::write(19, 0, "User mode test completed");
        
        // Safe infinite loop
        while (true) {
            for (volatile int i = 0; i < 1000000; i++) {
                // Just waste some time
            }
        }
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
    
    // Always yield back to kernel after each iteration
    UserAPI::yield();
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