#pragma once

#include "core/types.hpp"

namespace kira::system {

/**
 * @brief Utility functions for kernel use
 * 
 * Common utility functions that don't depend on external libraries
 * and can be used throughout the kernel.
 */
namespace utils {

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

} // namespace utils
} // namespace kira::system 