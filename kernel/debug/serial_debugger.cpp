#include "debug/serial_debugger.hpp"
#include "core/io.hpp"

namespace kira::debug {

void SerialDebugger::send_char(char c) {
    // Wait for transmitter to be ready
    while ((inb(COM1_LSR) & 0x20) == 0) {
        // Wait
    }
    outb(COM1_PORT, c);
}

void SerialDebugger::init() {
    // Basic serial port initialization
    outb(COM1_PORT + 1, 0x00);    // Disable all interrupts
    outb(COM1_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1_PORT + 1, 0x00);    // (hi byte)
    outb(COM1_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void SerialDebugger::print(const char* str) {
    while (*str) {
        send_char(*str++);
    }
}

void SerialDebugger::println(const char* str) {
    print(str);
    send_char('\r');
    send_char('\n');
}

void SerialDebugger::print_hex(u32 value) {
    print("0x");
    for (int i = 7; i >= 0; i--) {
        u32 digit = (value >> (i * 4)) & 0xF;
        char hexChar = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        send_char(hexChar);
    }
}

void SerialDebugger::print_dec(u32 value) {
    if (value == 0) {
        send_char('0');
        return;
    }
    
    char buffer[12]; // Enough for 32-bit number
    int pos = 0;
    
    // Convert to string (reversed)
    while (value > 0) {
        buffer[pos++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Print in correct order
    for (int i = pos - 1; i >= 0; i--) {
        send_char(buffer[i]);
    }
}

} // namespace kira::debug 