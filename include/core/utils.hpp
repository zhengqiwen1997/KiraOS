#pragma once

#include "core/types.hpp"

/**
 * @brief Utility functions for kernel use
 * 
 * Common utility functions that don't depend on external libraries
 * and can be used throughout the kernel.
 */
namespace kira::utils {

// Bring types from kira::system into scope
using kira::system::u8;
using kira::system::u16;
using kira::system::u32;
using kira::system::u64;
using kira::system::i8;
using kira::system::i16;
using kira::system::i32;
using kira::system::i64;

/**
 * @brief Calculate the length of a null-terminated string
 * @param str Pointer to null-terminated string
 * @return Length of the string (excluding null terminator)
 */
inline u32 strlen(const char* str) {
    if (!str) return 0;
    
    u32 len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// Simple string class for kernel use
class String {
private:
    static constexpr u32 SMALL_STRING_SIZE = 32;
    char buffer[SMALL_STRING_SIZE];
    
public:
    String() {
        buffer[0] = '\0';
    }
    
    String(const char* str) {
        if (!str) {
            buffer[0] = '\0';
            return;
        }
        // Use our kernel string copy with bounds checking
        u32 i = 0;
        while (i < SMALL_STRING_SIZE - 1 && str[i]) {
            buffer[i] = str[i];
            i++;
        }
        buffer[i] = '\0';
    }
    
    operator const char*() const {
        return buffer;
    }
    
    const char* c_str() const {
        return buffer;
    }
    
    String& operator+=(const String& other) {
        // Use our kernel string functions
        u32 len = kira::utils::strlen(buffer);  // Explicitly use our strlen
        u32 i = 0;
        while (len + i < SMALL_STRING_SIZE - 1 && other.buffer[i]) {
            buffer[len + i] = other.buffer[i];
            i++;
        }
        buffer[len + i] = '\0';
        return *this;
    }
    
    String& operator+=(const char* str) {
        if (!str) return *this;
        // Use our kernel string functions
        u32 len = kira::utils::strlen(buffer);  // Explicitly use our strlen
        u32 i = 0;
        while (len + i < SMALL_STRING_SIZE - 1 && str[i]) {
            buffer[len + i] = str[i];
            i++;
        }
        buffer[len + i] = '\0';
        return *this;
    }
};

inline String operator+(const String& a, const String& b) {
    String result = a;
    result += b;
    return result;
}

inline String operator+(const String& a, const char* b) {
    String result = a;
    result += b;
    return result;
}

inline String operator+(const char* a, const String& b) {
    String result = a;
    result += b;
    return result;
}

/**
 * @brief Convert number to hexadecimal string (8 characters, lowercase)
 * @param buffer Output buffer (must be at least 9 characters)
 * @param number Number to convert
 */
inline void number_to_hex(char* buffer, u32 number) {
    if (!buffer) return;
    
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        buffer[i] = hex[(number >> (28 - i * 4)) & 0xF];
    }
    buffer[8] = '\0';
}

// Convert number to hex string
inline String to_hex_string(u32 value) {
    char buffer[16];  // More than enough for 32-bit hex
    buffer[0] = '0';
    buffer[1] = 'x';
    number_to_hex(buffer + 2, value);  // Use our number_to_hex function
    return String(buffer);
}

/**
 * @brief Compare two null-terminated strings
 * @param str1 First string
 * @param str2 Second string
 * @return 0 if equal, negative if str1 < str2, positive if str1 > str2
 */
inline i32 strcmp(const char* str1, const char* str2) {
    if (!str1 || !str2) {
        if (str1 == str2) return 0;
        return str1 ? 1 : -1;
    }
    
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    
    return static_cast<i32>(*str1) - static_cast<i32>(*str2);
}

/**
 * @brief Simple string copy (no bounds checking)
 * @param dest Destination buffer
 * @param src Source string
 * @note Assumes dest has enough space for src
 */
inline void strcpy(char* dest, const char* src) {
    if (!dest || !src) return;
    
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

/**
 * @brief Simple string concatenation (no bounds checking)
 * @param dest Destination buffer (must already contain a string)
 * @param src Source string to append
 * @note Assumes dest has enough space for concatenated result
 */
inline void strcat(char* dest, const char* src) {
    if (!dest || !src) return;
    
    // Find end of dest string
    while (*dest) {
        dest++;
    }
    
    // Copy src to end of dest
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

/**
 * @brief Convert number to decimal string
 * @param buffer Output buffer (must be at least 11 characters for u32)
 * @param number Number to convert
 */
inline void number_to_decimal(char* buffer, u32 number) {
    if (!buffer) return;
    
    if (number == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    char temp[16];
    int i = 0;
    while (number > 0) {
        temp[i++] = '0' + (number % 10);
        number /= 10;
    }
    
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

/**
 * @brief Safe string copy with bounds checking and overlap detection
 * @param dest Destination buffer
 * @param src Source string
 * @param maxLen Maximum number of characters to copy (including null terminator)
 * @note Refuses to copy if source and destination overlap (sets dest to empty string)
 */
inline void strcpy_s(char* dest, const char* src, u32 maxLen) {
    if (!dest || !src || maxLen == 0) return;
    if (dest == src) return;  // Same pointer, nothing to do
    
    // Calculate source length for overlap detection
    u32 srcLen = strlen(src);
    
    // Check for dangerous overlaps
    bool overlaps = (dest > src && dest < src + srcLen) ||
                   (src > dest && src < dest + maxLen);
    
    if (overlaps) {
        // For kernel safety, refuse overlapping copies
        dest[0] = '\0';  // Set to empty string
        return;
    }
    
    // Safe non-overlapping copy
    u32 i = 0;
    while (i < maxLen - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * @brief String move - handles overlapping memory regions safely
 * @param dest Destination buffer
 * @param src Source string
 * @param maxLen Maximum number of characters to copy (including null terminator)
 * @note Uses byte-by-byte copying in correct direction for overlapping regions
 */
inline void strmove_s(char* dest, const char* src, u32 maxLen) {
    if (!dest || !src || maxLen == 0) return;
    if (dest == src) return;  // Same pointer, nothing to do
    
    u32 srcLen = strlen(src);
    u32 copyLen = (srcLen < maxLen - 1) ? srcLen : maxLen - 1;
    
    if (dest > src && dest < src + srcLen) {
        // Forward overlap: copy backwards to avoid corruption
        for (u32 i = copyLen; i > 0; i--) {
            dest[i - 1] = src[i - 1];
        }
    } else {
        // No overlap or backward overlap: copy forwards
        for (u32 i = 0; i < copyLen; i++) {
            dest[i] = src[i];
        }
    }
    
    dest[copyLen] = '\0';
}

/**
 * @brief Copy memory from source to destination
 * @param dest Destination buffer
 * @param src Source buffer
 * @param count Number of bytes to copy
 * @return Pointer to destination
 */
inline void* memcpy(void* dest, const void* src, u32 count) {
    if (!dest || !src || count == 0) return dest;
    
    u8* d = static_cast<u8*>(dest);
    const u8* s = static_cast<const u8*>(src);
    
    for (u32 i = 0; i < count; i++) {
        d[i] = s[i];
    }
    
    return dest;
}

/**
 * @brief Set memory to a specific value
 * @param dest Destination buffer
 * @param value Value to set (will be cast to u8)
 * @param count Number of bytes to set
 * @return Pointer to destination
 */
inline void* memset(void* dest, i32 value, u32 count) {
    if (!dest || count == 0) return dest;
    
    u8* d = static_cast<u8*>(dest);
    u8 val = static_cast<u8>(value);
    
    for (u32 i = 0; i < count; i++) {
        d[i] = val;
    }
    
    return dest;
}

// Safe division functions for kernel code
template<typename T>
bool safe_divide(T dividend, T divisor, T& result) {
    if (divisor == 0) {
        return false;
    }
    result = dividend / divisor;
    return true;
}

// Safe division with default value
template<typename T>
T safe_divide_or(T dividend, T divisor, T default_value) {
    if (divisor == 0) {
        return default_value;
    }
    return dividend / divisor;
}

// Safe modulo operation
template<typename T>
bool safe_modulo(T dividend, T divisor, T& result) {
    if (divisor == 0) {
        return false;
    }
    result = dividend % divisor;
    return true;
}

/**
 * @brief Convert character to uppercase
 * @param c Character to convert
 * @return Uppercase version of character if it's lowercase, otherwise unchanged
 */
inline char toupper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

/**
 * @brief Compare two memory regions byte by byte
 * @param ptr1 First memory region
 * @param ptr2 Second memory region
 * @param num Number of bytes to compare
 * @return 0 if equal, negative if ptr1 < ptr2, positive if ptr1 > ptr2
 */
inline int memcmp(const void* ptr1, const void* ptr2, u32 num) {
    const u8* p1 = static_cast<const u8*>(ptr1);
    const u8* p2 = static_cast<const u8*>(ptr2);
    
    for (u32 i = 0; i < num; i++) {
        if (p1[i] != p2[i]) {
            return (p1[i] < p2[i]) ? -1 : 1;
        }
    }
    return 0;
}

/**
 * @brief Kernel printf function - formatted output to console
 * @param format Format string with %d, %x, %s, %c specifiers
 * @param ... Variable arguments
 * @return Number of characters written
 */
int k_printf(const char* format, ...);

/**
 * @brief Kernel printf function with color support
 * @param color VGA color for the output
 * @param format Format string with %d, %x, %s, %c specifiers  
 * @param ... Variable arguments
 * @return Number of characters written
 */
int k_printf_colored(u16 color, const char* format, ...);

} // namespace kira::utils 