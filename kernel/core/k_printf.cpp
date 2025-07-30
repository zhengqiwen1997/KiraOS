#include "core/utils.hpp"
#include "display/console.hpp"
#include "display/vga.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::utils {

using namespace kira::system;
using namespace kira::display;

// Manual implementation of variadic arguments for kernel
typedef char* va_list;

#define va_start(ap, last) \
    (ap = (va_list)&last + ((sizeof(last) + sizeof(void*) - 1) & ~(sizeof(void*) - 1)))

#define va_arg(ap, type) \
    (ap += ((sizeof(type) + sizeof(void*) - 1) & ~(sizeof(void*) - 1)), \
     *(type*)(ap - ((sizeof(type) + sizeof(void*) - 1) & ~(sizeof(void*) - 1))))

#define va_end(ap) \
    (ap = (va_list)0)

/**
 * @brief Internal function to format a single argument and append to buffer
 * @param buffer Output buffer to append to
 * @param bufferSize Total size of output buffer
 * @param currentLen Current length of buffer content
 * @param specifier Format specifier character ('d', 'x', 's', 'c')
 * @param args Variable arguments list
 * @return New length of buffer content
 */
static u32 format_argument(char* buffer, u32 bufferSize, u32 currentLen, char specifier, va_list& args) {
    if (currentLen >= bufferSize - 1) return currentLen;
    
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
            
            // Add negative sign if needed
            if (negative && currentLen < bufferSize - 1) {
                buffer[currentLen++] = '-';
            }
            
            // Convert number to string
            char numBuffer[16];
            number_to_decimal(numBuffer, absValue);
            
            // Append to output buffer
            u32 i = 0;
            while (numBuffer[i] && currentLen < bufferSize - 1) {
                buffer[currentLen++] = numBuffer[i++];
            }
            break;
        }
        
        case 'u': {
            // Unsigned decimal integer
            u32 value = va_arg(args, unsigned int);
            char numBuffer[16];
            number_to_decimal(numBuffer, value);
            
            // Append to output buffer
            u32 i = 0;
            while (numBuffer[i] && currentLen < bufferSize - 1) {
                buffer[currentLen++] = numBuffer[i++];
            }
            break;
        }
        
        case 'x': {
            // Hexadecimal (lowercase)
            u32 value = va_arg(args, unsigned int);
            
            // Add "0x" prefix
            if (currentLen < bufferSize - 2) {
                buffer[currentLen++] = '0';
                buffer[currentLen++] = 'x';
            }
            
            char hexBuffer[16];
            number_to_hex(hexBuffer, value);
            
            // Append to output buffer
            u32 i = 0;
            while (hexBuffer[i] && currentLen < bufferSize - 1) {
                buffer[currentLen++] = hexBuffer[i++];
            }
            break;
        }
        
        case 'X': {
            // Hexadecimal (uppercase)
            u32 value = va_arg(args, unsigned int);
            
            // Add "0x" prefix
            if (currentLen < bufferSize - 2) {
                buffer[currentLen++] = '0';
                buffer[currentLen++] = 'X';
            }
            
            char hexBuffer[16];
            number_to_hex(hexBuffer, value);
            
            // Convert to uppercase and append
            u32 i = 0;
            while (hexBuffer[i] && currentLen < bufferSize - 1) {
                buffer[currentLen++] = toupper(hexBuffer[i++]);
            }
            break;
        }
        
        case 's': {
            // String
            const char* str = va_arg(args, const char*);
            if (!str) str = "(null)";
            
            // Append string to output buffer
            while (*str && currentLen < bufferSize - 1) {
                buffer[currentLen++] = *str++;
            }
            break;
        }
        
        case 'c': {
            // Character
            char ch = static_cast<char>(va_arg(args, int));
            if (currentLen < bufferSize - 1) {
                buffer[currentLen++] = ch;
            }
            break;
        }
        
        case 'p': {
            // Pointer (as hex)
            void* ptr = va_arg(args, void*);
            u32 value = reinterpret_cast<u32>(ptr);
            
            // Add "0x" prefix
            if (currentLen < bufferSize - 2) {
                buffer[currentLen++] = '0';
                buffer[currentLen++] = 'x';
            }
            
            char hexBuffer[16];
            number_to_hex(hexBuffer, value);
            
            // Append to output buffer
            u32 i = 0;
            while (hexBuffer[i] && currentLen < bufferSize - 1) {
                buffer[currentLen++] = hexBuffer[i++];
            }
            break;
        }
        
        case '%': {
            // Literal %
            if (currentLen < bufferSize - 1) {
                buffer[currentLen++] = '%';
            }
            break;
        }
        
        default: {
            // Unknown specifier - just add the % and the character
            if (currentLen < bufferSize - 2) {
                buffer[currentLen++] = '%';
                buffer[currentLen++] = specifier;
            }
            break;
        }
    }
    
    return currentLen;
}

/**
 * @brief Internal k_printf implementation
 * @param color VGA color for output (0 = use default)
 * @param format Format string
 * @param args Variable arguments list
 * @return Number of characters written
 */
static int k_printf_internal(u16 color, const char* format, va_list args) {
    if (!format) return 0;
    
    // Use a reasonably sized buffer for kernel messages
    constexpr u32 BUFFER_SIZE = 512;
    char buffer[BUFFER_SIZE];
    u32 bufferLen = 0;
    
    const char* ptr = format;
    while (*ptr && bufferLen < BUFFER_SIZE - 1) {
        if (*ptr == '%' && *(ptr + 1) != '\0') {
            ptr++; // Skip %
            char specifier = *ptr++;
            bufferLen = format_argument(buffer, BUFFER_SIZE, bufferLen, specifier, args);
        } else {
            buffer[bufferLen++] = *ptr++;
        }
    }
    
    // Null terminate
    buffer[bufferLen] = '\0';
    
    // Output to console using printf-style behavior (no auto-newline)
    if (color == 0) {
        kira::kernel::console.add_printf_output(buffer, VGA_WHITE_ON_BLUE);
    } else {
        kira::kernel::console.add_printf_output(buffer, color);
    }
    
    return static_cast<int>(bufferLen);
}

// Public k_printf function
int k_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = k_printf_internal(0, format, args);
    va_end(args);
    return result;
}

// Public k_printf_colored function
int k_printf_colored(u16 color, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = k_printf_internal(color, format, args);
    va_end(args);
    return result;
}

} // namespace kira::utils