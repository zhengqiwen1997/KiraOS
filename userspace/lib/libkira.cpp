#include "libkira.hpp"
#include "core/syscalls.hpp"

namespace kira::usermode {

using namespace kira::system;

// Manual implementation of variadic arguments for user space
typedef char* va_list;

#define va_start(ap, last) \
    (ap = (va_list)&last + ((sizeof(last) + sizeof(void*) - 1) & ~(sizeof(void*) - 1)))

#define va_arg(ap, type) \
    (ap += ((sizeof(type) + sizeof(void*) - 1) & ~(sizeof(void*) - 1)), \
     *(type*)(ap - ((sizeof(type) + sizeof(void*) - 1) & ~(sizeof(void*) - 1))))

#define va_end(ap) \
    (ap = (va_list)0)

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
    return syscall(static_cast<u32>(SystemCall::WRITE_PRINTF), (u32)text, Colors::DEFAULT);
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
    
    // Send as single message using printf-style output
    return syscall(static_cast<u32>(SystemCall::WRITE_PRINTF), (u32)buffer, Colors::DEFAULT);
}

i32 UserAPI::print_colored(const char* text, u16 color) {
    if (!text) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    return syscall(static_cast<u32>(SystemCall::WRITE_PRINTF), (u32)text, color);
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
    syscall(static_cast<u32>(SystemCall::EXIT), 0);
    // Should not return, but just in case
    while (true) {
        asm volatile("hlt");
    }
}

void UserAPI::exit_with(i32 status) {
    syscall(static_cast<u32>(SystemCall::EXIT), static_cast<u32>(status));
    while (true) { asm volatile("hlt"); }
}

/**
 * @brief Helper function to get string length
 */
static u32 user_strlen(const char* str) {
    if (!str) return 0;
    u32 len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * @brief Helper function to copy string
 */
static void user_strcpy(char* dest, const char* src, u32 maxLen) {
    if (!dest || !src) return;
    u32 i = 0;
    while (src[i] != '\0' && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * @brief Helper function to append string
 */
static void user_strcat(char* dest, const char* src, u32 maxLen) {
    if (!dest || !src) return;
    u32 destLen = user_strlen(dest);
    u32 i = 0;
    while (src[i] != '\0' && destLen + i < maxLen - 1) {
        dest[destLen + i] = src[i];
        i++;
    }
    dest[destLen + i] = '\0';
}

/**
 * @brief Convert number to decimal string
 */
static void user_number_to_decimal(char* buffer, u32 bufferSize, u32 number) {
    if (!buffer || bufferSize == 0) return;
    
    if (number == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    // Convert number to string (reverse order)
    char temp[16];
    u32 i = 0;
    while (number > 0 && i < 15) {
        temp[i++] = '0' + (number % 10);
        number /= 10;
    }
    
    // Reverse and copy to buffer
    u32 len = (i < bufferSize - 1) ? i : bufferSize - 1;
    for (u32 j = 0; j < len; j++) {
        buffer[j] = temp[len - 1 - j];
    }
    buffer[len] = '\0';
}

/**
 * @brief Convert number to hex string
 */
static void user_number_to_hex(char* buffer, u32 bufferSize, u32 number, bool uppercase) {
    if (!buffer || bufferSize == 0) return;
    
    if (number == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    
    // Convert number to hex string (reverse order)
    char temp[16];
    u32 i = 0;
    while (number > 0 && i < 15) {
        temp[i++] = digits[number % 16];
        number /= 16;
    }
    
    // Reverse and copy to buffer
    u32 len = (i < bufferSize - 1) ? i : bufferSize - 1;
    for (u32 j = 0; j < len; j++) {
        buffer[j] = temp[len - 1 - j];
    }
    buffer[len] = '\0';
}

/**
 * @brief Format a single argument and append to buffer
 */
static u32 user_format_argument(char* buffer, u32 bufferSize, u32 currentLen, char specifier, va_list& args) {
    if (currentLen >= bufferSize - 1) return currentLen;
    
    char temp[32];
    
    switch (specifier) {
        case 'd': {
            // Signed decimal integer
            i32 value = va_arg(args, int);
            bool negative = false;
            u32 absValue;
            
            if (value < 0) {
                negative = true;
                absValue = static_cast<u32>(-value);
            } else {
                absValue = static_cast<u32>(value);
            }
            
            user_number_to_decimal(temp, sizeof(temp), absValue);
            
            if (negative && currentLen < bufferSize - 1) {
                buffer[currentLen++] = '-';
            }
            
            user_strcat(buffer, temp, bufferSize);
            return user_strlen(buffer);
        }
        
        case 'u': {
            // Unsigned decimal integer
            u32 value = va_arg(args, u32);
            user_number_to_decimal(temp, sizeof(temp), value);
            user_strcat(buffer, temp, bufferSize);
            return user_strlen(buffer);
        }
        
        case 'x': {
            // Lowercase hexadecimal
            u32 value = va_arg(args, u32);
            user_number_to_hex(temp, sizeof(temp), value, false);
            user_strcat(buffer, temp, bufferSize);
            return user_strlen(buffer);
        }
        
        case 'X': {
            // Uppercase hexadecimal
            u32 value = va_arg(args, u32);
            user_number_to_hex(temp, sizeof(temp), value, true);
            user_strcat(buffer, temp, bufferSize);
            return user_strlen(buffer);
        }
        
        case 's': {
            // String
            const char* str = va_arg(args, const char*);
            if (str) {
                user_strcat(buffer, str, bufferSize);
            } else {
                user_strcat(buffer, "(null)", bufferSize);
            }
            return user_strlen(buffer);
        }
        
        case 'c': {
            // Character
            char value = static_cast<char>(va_arg(args, int));
            if (currentLen < bufferSize - 1) {
                buffer[currentLen] = value;
                buffer[currentLen + 1] = '\0';
                return currentLen + 1;
            }
            return currentLen;
        }
        
        default:
            // Unknown specifier, just add the character
            if (currentLen < bufferSize - 1) {
                buffer[currentLen] = specifier;
                buffer[currentLen + 1] = '\0';
                return currentLen + 1;
            }
            return currentLen;
    }
}

i32 UserAPI::printf(const char* format, ...) {
    if (!format) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    
    constexpr u32 BUFFER_SIZE = 512;
    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';
    
    va_list args;
    va_start(args, format);
    
    u32 bufferLen = 0;
    
    for (u32 i = 0; format[i] != '\0' && bufferLen < BUFFER_SIZE - 1; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            // Format specifier
            char specifier = format[i + 1];
            if (specifier == '%') {
                // Escaped percent sign
                buffer[bufferLen++] = '%';
                buffer[bufferLen] = '\0';
                i++; // Skip the second %
            } else {
                // Process format specifier
                bufferLen = user_format_argument(buffer, BUFFER_SIZE, bufferLen, specifier, args);
                i++; // Skip the format character
            }
        } else {
            // Regular character
            buffer[bufferLen++] = format[i];
            buffer[bufferLen] = '\0';
        }
    }
    
    va_end(args);
    
    // Output the formatted string using existing print_colored function
    return print_colored(buffer, Colors::DEFAULT);
}

// File system operations
i32 UserAPI::open(const char* path, u32 flags) {
    if (!path) return -2; // INVALID_PARAMETER
    return syscall(static_cast<u32>(SystemCall::OPEN), (u32)path, flags);
}

i32 UserAPI::close(i32 fd) {
    return syscall(static_cast<u32>(SystemCall::CLOSE), fd);
}

i32 UserAPI::read_file(i32 fd, void* buffer, u32 size) {
    if (!buffer) return -2; // INVALID_PARAMETER
    return syscall(static_cast<u32>(SystemCall::READ_FILE), fd, (u32)buffer, size);
}

i32 UserAPI::write_file(i32 fd, const void* buffer, u32 size) {
    if (!buffer) return -2; // INVALID_PARAMETER
    return syscall(static_cast<u32>(SystemCall::WRITE_FILE), fd, (u32)buffer, size);
}

i32 UserAPI::readdir(const char* path, u32 index, void* entry) {
    if (!path || !entry) {
        return -2; // INVALID_PARAMETER
    }
    
    return syscall(static_cast<u32>(SystemCall::READDIR), (u32)path, index, (u32)entry);
}

i32 UserAPI::mkdir(const char* path) {
    if (!path) return -2; // INVALID_PARAMETER
    return syscall(static_cast<u32>(SystemCall::MKDIR), (u32)path);
}

i32 UserAPI::rmdir(const char* path) {
    if (!path) return -2; // INVALID_PARAMETER
    return syscall(static_cast<u32>(SystemCall::RMDIR), (u32)path);
}

// Process management operations
i32 UserAPI::ps() {
    return syscall(static_cast<u32>(SystemCall::PS));
}

i32 UserAPI::kill(u32 pid) {
    return syscall(static_cast<u32>(SystemCall::KILL), pid);
}

i32 UserAPI::wait(u32 pid) {
    return syscall(static_cast<u32>(SystemCall::WAIT), pid);
}

i32 UserAPI::waitid(u32 pid, i32* statusPtr) {
    return syscall(static_cast<u32>(SystemCall::WAITID), pid, reinterpret_cast<u32>(statusPtr));
}

i32 UserAPI::fork() {
    return syscall(static_cast<u32>(SystemCall::FORK));
}

// Keyboard input
i32 UserAPI::getch() {
    return syscall(static_cast<u32>(SystemCall::GETCH));
}

i32 UserAPI::trygetch() {
    return syscall(static_cast<u32>(SystemCall::TRYGETCH));
}

i32 UserAPI::spawn(u32 programId, u32 arg1) {
    return syscall(static_cast<u32>(SystemCall::SPAWN), programId, arg1);
}

const char* UserAPI::getcwd_ptr() {
    return reinterpret_cast<const char*>(syscall(static_cast<u32>(SystemCall::GETCWD_PTR)));
}

i32 UserAPI::chdir(const char* absPath) {
    if (!absPath) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    return syscall(static_cast<u32>(SystemCall::CHDIR), reinterpret_cast<u32>(absPath));
}

i32 UserAPI::getcwd(char* buffer, u32 size) {
    if (!buffer || size == 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    return syscall(static_cast<u32>(SystemCall::GETCWD), reinterpret_cast<u32>(buffer), size);
}

i32 UserAPI::exec(const char* absPath, const char* arg0) {
    if (!absPath) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    return syscall(static_cast<u32>(SystemCall::EXEC), reinterpret_cast<u32>(absPath), reinterpret_cast<u32>(arg0));
}

i32 UserAPI::getspawnarg(char* buffer, u32 size) {
    if (!buffer || size == 0) return static_cast<i32>(SyscallResult::INVALID_PARAMETER);
    return syscall(static_cast<u32>(SystemCall::GETSPAWNARG), reinterpret_cast<u32>(buffer), size);
}

const char* UserAPI::strerror(i32 code) {
    switch (code) {
        case 0: return "success";
        case static_cast<i32>(SyscallResult::INVALID_SYSCALL): return "invalid syscall";
        case static_cast<i32>(SyscallResult::INVALID_PARAMETER): return "invalid parameter";
        case static_cast<i32>(SyscallResult::PERMISSION_DENIED): return "permission denied";
        case static_cast<i32>(SyscallResult::RESOURCE_UNAVAILABLE): return "resource unavailable";
        case static_cast<i32>(SyscallResult::FILE_NOT_FOUND): return "file not found";
        case static_cast<i32>(SyscallResult::FILE_EXISTS): return "file exists";
        case static_cast<i32>(SyscallResult::IO_ERROR): return "I/O error";
        case static_cast<i32>(SyscallResult::NO_SPACE): return "no space";
        case static_cast<i32>(SyscallResult::TOO_MANY_FILES): return "too many files";
        case static_cast<i32>(SyscallResult::NOT_DIRECTORY): return "not a directory";
        case static_cast<i32>(SyscallResult::IS_DIRECTORY): return "is a directory";
        default: return "unknown error";
    }
}

} // namespace kira::usermode 