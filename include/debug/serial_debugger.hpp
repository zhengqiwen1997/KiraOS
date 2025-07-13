#pragma once

#include "core/types.hpp"

namespace kira::debug {

using namespace kira::system;

/**
 * SerialDebugger - A utility class for outputting debug information to the serial port
 * 
 * This class provides a simple interface for sending debug messages to COM1 serial port,
 * which is useful for debugging kernel code where normal console output might not be available
 * or when debugging early boot processes.
 */
class SerialDebugger {
private:
    static constexpr u16 COM1_PORT = 0x3F8;
    static constexpr u16 COM1_LSR = COM1_PORT + 5;
    
    static void send_char(char c);
    
public:
    /**
     * Initialize the serial port for debugging output
     * Sets up COM1 with 38400 baud, 8 bits, no parity, one stop bit
     */
    static void init();
    
    /**
     * Print a null-terminated string to the serial port
     * @param str The string to print
     */
    static void print(const char* str);
    
    /**
     * Print a null-terminated string followed by a newline
     * @param str The string to print
     */
    static void println(const char* str);
    
    /**
     * Print a 32-bit value in hexadecimal format (with 0x prefix)
     * @param value The value to print in hex
     */
    static void print_hex(u32 value);
    
    /**
     * Print a 32-bit value in decimal format
     * @param value The value to print in decimal
     */
    static void print_dec(u32 value);
};

} // namespace kira::debug 