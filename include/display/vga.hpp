#pragma once

#include "core/types.hpp"

namespace kira {
namespace display {

using namespace kira::system;

// VGA Color Constants
const u16 VGA_WHITE_ON_BLUE = 0x1F00;
const u16 VGA_YELLOW_ON_BLUE = 0x1E00;
const u16 VGA_GREEN_ON_BLUE = 0x1A00;
const u16 VGA_RED_ON_BLUE = 0x1C00;
const u16 VGA_CYAN_ON_BLUE = 0x1B00;
const u16 VGA_MAGENTA_ON_BLUE = 0x1D00;
const u16 VGA_LIGHT_GRAY_ON_BLUE = 0x1700;
const u16 VGA_BLACK_ON_CYAN = 0x3000;

// VGA Layout Constants
const u32 VGA_BUFFER = 0xB8000;
const u32 VGA_WIDTH = 80;
const u32 VGA_HEIGHT = 25;
const u32 VGA_SIZE = VGA_WIDTH * VGA_HEIGHT;



/**
 * @brief VGA Text Mode Display Manager
 * 
 * Provides a clean interface for VGA text mode operations,
 * replacing repetitive inline VGA manipulation code.
 */
class VGADisplay {
private:
    volatile u16* buffer;
    
public:
    /**
     * @brief Constructor - initializes VGA buffer pointer
     */
    VGADisplay() : buffer(reinterpret_cast<volatile u16*>(VGA_BUFFER)) {}
    
    /**
     * @brief Clear the entire screen with specified color
     * @param color Background and foreground color (default: white on blue)
     */
    void clear_screen(u16 color = VGA_WHITE_ON_BLUE) {
        for (u32 i = 0; i < VGA_SIZE; i++) {
            buffer[i] = color | ' ';
        }
    }
    
    /**
     * @brief Print a string at specified position
     * @param line Line number (0-24)
     * @param col Column number (0-79)
     * @param str String to print
     * @param color Text color (default: white on blue)
     */
    void print_string(u32 line, u32 col, const char* str, u16 color = VGA_WHITE_ON_BLUE) {
        if (line >= VGA_HEIGHT) return;
        
        volatile u16* pos = buffer + (line * VGA_WIDTH) + col;
        for (u32 i = 0; str[i] != '\0' && (col + i) < VGA_WIDTH; i++) {
            pos[i] = color | str[i];
        }
    }
    
    /**
     * @brief Print a 32-bit hexadecimal value
     * @param line Line number
     * @param col Column number
     * @param value Value to print as hex
     * @param color Text color
     */
    void print_hex(u32 line, u32 col, u32 value, u16 color = VGA_WHITE_ON_BLUE) {
        if (line >= VGA_HEIGHT || col + 10 > VGA_WIDTH) return;
        
        volatile u16* pos = buffer + (line * VGA_WIDTH) + col;
        pos[0] = color | '0';
        pos[1] = color | 'x';
        
        for (int i = 7; i >= 0; i--) {
            u32 digit = (value >> (i * 4)) & 0xF;
            char hexChar = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
            pos[2 + (7 - i)] = color | hexChar;
        }
    }
    
    /**
     * @brief Print a single byte as hexadecimal (2 characters)
     * @param line Line number
     * @param col Column number  
     * @param value Byte value to print as hex
     * @param color Text color
     */
    void print_hex_byte(u32 line, u32 col, u8 value, u16 color = VGA_WHITE_ON_BLUE) {
        if (line >= VGA_HEIGHT || col + 2 > VGA_WIDTH) return;
        
        volatile u16* pos = buffer + (line * VGA_WIDTH) + col;
        
        // High nibble
        u32 highDigit = (value >> 4) & 0xF;
        char highChar = (highDigit < 10) ? ('0' + highDigit) : ('A' + highDigit - 10);
        pos[0] = color | highChar;
        
        // Low nibble
        u32 lowDigit = value & 0xF;
        char lowChar = (lowDigit < 10) ? ('0' + lowDigit) : ('A' + lowDigit - 10);
        pos[1] = color | lowChar;
    }
    
    /**
     * @brief Print a 32-bit decimal value
     * @param line Line number
     * @param col Column number
     * @param value Value to print as decimal
     * @param color Text color
     */
    void print_decimal(u32 line, u32 col, u32 value, u16 color = VGA_WHITE_ON_BLUE) {
        if (line >= VGA_HEIGHT) return;
        
        volatile u16* pos = buffer + (line * VGA_WIDTH) + col;
        
        // Handle zero case
        if (value == 0) {
            if (col < VGA_WIDTH) {
                pos[0] = color | '0';
            }
            return;
        }
        
        // Convert to string (reverse order)
        char digits[12]; // Enough for 32-bit number
        int digitCount = 0;
        
        while (value > 0 && digitCount < 11) {
            digits[digitCount++] = '0' + (value % 10);
            value /= 10;
        }
        
        // Print digits in correct order
        for (int i = digitCount - 1; i >= 0 && col + (digitCount - 1 - i) < VGA_WIDTH; i--) {
            pos[digitCount - 1 - i] = color | digits[i];
        }
    }
    
    /**
     * @brief Print a character at specified position
     * @param line Line number
     * @param col Column number
     * @param ch Character to print
     * @param color Text color
     */
    void print_char(u32 line, u32 col, char ch, u16 color = VGA_WHITE_ON_BLUE) {
        if (line >= VGA_HEIGHT || col >= VGA_WIDTH) return;
        
        volatile u16* pos = buffer + (line * VGA_WIDTH) + col;
        pos[0] = color | ch;
    }
    
    /**
     * @brief Fill a line with spaces (clear line)
     * @param line Line number to clear
     * @param color Background color
     */
    void clear_line(u32 line, u16 color = VGA_WHITE_ON_BLUE) {
        if (line >= VGA_HEIGHT) return;
        
        volatile u16* pos = buffer + (line * VGA_WIDTH);
        for (u32 i = 0; i < VGA_WIDTH; i++) {
            pos[i] = color | ' ';
        }
    }
    
    /**
     * @brief Get the current character at specified position
     * @param line Line number
     * @param col Column number
     * @return Character at position (or 0 if out of bounds)
     */
    char get_char(u32 line, u32 col) const {
        if (line >= VGA_HEIGHT || col >= VGA_WIDTH) return 0;
        
        volatile u16* pos = buffer + (line * VGA_WIDTH) + col;
        return static_cast<char>(pos[0] & 0xFF);
    }
    


};

} // namespace display
} // namespace kira 