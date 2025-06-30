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
 * @brief Simple string copy (no bounds checking)
 * @param dest Destination buffer
 * @param src Source string
 * @note Assumes dest has enough space for src
 */
inline void string_copy(char* dest, const char* src) {
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
inline void string_concat(char* dest, const char* src) {
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
 * @param max_len Maximum number of characters to copy (including null terminator)
 * @note Refuses to copy if source and destination overlap (sets dest to empty string)
 */
inline void safe_strcpy(char* dest, const char* src, u32 max_len) {
    if (!dest || !src || max_len == 0) return;
    if (dest == src) return;  // Same pointer, nothing to do
    
    // Calculate source length for overlap detection
    u32 src_len = strlen(src);
    
    // Check for dangerous overlaps
    bool overlaps = (dest > src && dest < src + src_len) ||
                   (src > dest && src < dest + max_len);
    
    if (overlaps) {
        // For kernel safety, refuse overlapping copies
        dest[0] = '\0';  // Set to empty string
        return;
    }
    
    // Safe non-overlapping copy
    u32 i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * @brief String move - handles overlapping memory regions safely
 * @param dest Destination buffer
 * @param src Source string
 * @param max_len Maximum number of characters to copy (including null terminator)
 * @note Uses byte-by-byte copying in correct direction for overlapping regions
 */
inline void safe_strmove(char* dest, const char* src, u32 max_len) {
    if (!dest || !src || max_len == 0) return;
    if (dest == src) return;  // Same pointer, nothing to do
    
    u32 src_len = strlen(src);
    u32 copy_len = (src_len < max_len - 1) ? src_len : max_len - 1;
    
    if (dest > src && dest < src + src_len) {
        // Forward overlap: copy backwards to avoid corruption
        for (u32 i = copy_len; i > 0; i--) {
            dest[i - 1] = src[i - 1];
        }
    } else {
        // No overlap or backward overlap: copy forwards
        for (u32 i = 0; i < copy_len; i++) {
            dest[i] = src[i];
        }
    }
    
    dest[copy_len] = '\0';
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