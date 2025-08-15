#pragma once

#include "core/types.hpp"

namespace kira::system {

// ========== KEYBOARD SCAN CODES ==========
// Function keys
constexpr u8 KEY_ESC = 0x01;
constexpr u8 KEY_F1 = 0x3B;
constexpr u8 KEY_F2 = 0x3C;
constexpr u8 KEY_F3 = 0x3D;
constexpr u8 KEY_F4 = 0x3E;
constexpr u8 KEY_F5 = 0x3F;
constexpr u8 KEY_F6 = 0x40;
constexpr u8 KEY_F7 = 0x41;
constexpr u8 KEY_F8 = 0x42;
constexpr u8 KEY_F9 = 0x43;
constexpr u8 KEY_F10 = 0x44;
constexpr u8 KEY_F11 = 0x57;
constexpr u8 KEY_F12 = 0x58;

// Number row
constexpr u8 KEY_1 = 0x02;
constexpr u8 KEY_2 = 0x03;
constexpr u8 KEY_3 = 0x04;
constexpr u8 KEY_4 = 0x05;
constexpr u8 KEY_5 = 0x06;
constexpr u8 KEY_6 = 0x07;
constexpr u8 KEY_7 = 0x08;
constexpr u8 KEY_8 = 0x09;
constexpr u8 KEY_9 = 0x0A;
constexpr u8 KEY_0 = 0x0B;

// Letters
constexpr u8 KEY_Q = 0x10;
constexpr u8 KEY_W = 0x11;
constexpr u8 KEY_E = 0x12;
constexpr u8 KEY_R = 0x13;
constexpr u8 KEY_T = 0x14;
constexpr u8 KEY_Y = 0x15;
constexpr u8 KEY_U = 0x16;
constexpr u8 KEY_I = 0x17;
constexpr u8 KEY_O = 0x18;
constexpr u8 KEY_P = 0x19;
constexpr u8 KEY_A = 0x1E;
constexpr u8 KEY_S = 0x1F;
constexpr u8 KEY_D = 0x20;
constexpr u8 KEY_F = 0x21;
constexpr u8 KEY_G = 0x22;
constexpr u8 KEY_H = 0x23;
constexpr u8 KEY_J = 0x24;
constexpr u8 KEY_K = 0x25;
constexpr u8 KEY_L = 0x26;
constexpr u8 KEY_Z = 0x2C;
constexpr u8 KEY_X = 0x2D;
constexpr u8 KEY_C = 0x2E;
constexpr u8 KEY_V = 0x2F;
constexpr u8 KEY_B = 0x30;
constexpr u8 KEY_N = 0x31;
constexpr u8 KEY_M = 0x32;

// Special keys
constexpr u8 KEY_SPACE = 0x39;
constexpr u8 KEY_ENTER = 0x1C;
constexpr u8 KEY_BACKSPACE = 0x0E;
constexpr u8 KEY_TAB = 0x0F;
constexpr u8 KEY_LSHIFT = 0x2A;
constexpr u8 KEY_RSHIFT = 0x36;
constexpr u8 KEY_LCTRL = 0x1D;
constexpr u8 KEY_LALT = 0x38;
constexpr u8 KEY_CAPS_LOCK = 0x3A;

// Arrow keys
constexpr u8 KEY_UP = 0x48;
constexpr u8 KEY_DOWN = 0x50;
constexpr u8 KEY_LEFT = 0x4B;
constexpr u8 KEY_RIGHT = 0x4D;

// Navigation keys
constexpr u8 KEY_PAGE_UP = 0x49;
constexpr u8 KEY_PAGE_DOWN = 0x51;
constexpr u8 KEY_HOME = 0x47;
constexpr u8 KEY_END = 0x4F;
constexpr u8 KEY_INSERT = 0x52;
constexpr u8 KEY_DELETE = 0x53;

// Key release flag (bit 7 set)
constexpr u8 KEY_RELEASED = 0x80;

// Keyboard ports
constexpr u16 KEYBOARD_DATA_PORT = 0x60;
constexpr u16 KEYBOARD_STATUS_PORT = 0x64;

/**
 * @brief Enhanced keyboard input handler with interrupt integration
 */
class Keyboard {
private:
    static bool shiftPressed;
    static bool ctrlPressed;
    static bool altPressed;
    static bool capsLockOn;

    // Simple ring buffer for ASCII keys captured from IRQ
    static constexpr u32 KEY_BUFFER_SIZE = 128;
    static volatile u32 keyHead;
    static volatile u32 keyTail;
    static char keyBuffer[KEY_BUFFER_SIZE];

public:
    /**
     * @brief Initialize keyboard system
     */
    static void initialize();
    
    /**
     * @brief Check if a key is available (polling mode)
     * @return true if key is available
     */
    static bool key_available() {
        u8 status = inb(KEYBOARD_STATUS_PORT);
        return (status & 0x01) != 0;
    }
    
    /**
     * @brief Read a key scan code (non-blocking)
     * @return scan code or 0 if no key available
     */
    static u8 read_key() {
        if (!key_available()) {
            return 0;
        }
        return inb(KEYBOARD_DATA_PORT);
    }
    
    /**
     * @brief Wait for and read a key scan code (blocking)
     * @return scan code
     */
    static u8 wait_for_key() {
        while (!key_available()) {
            asm volatile("hlt");  // Wait for interrupt
        }
        return inb(KEYBOARD_DATA_PORT);
    }
    
    /**
     * @brief Convert scan code to ASCII character
     * @param scanCode Raw scan code from keyboard
     * @return ASCII character or 0 if not printable
     */
    static char scan_code_to_ascii(u8 scanCode);
    
    /**
     * @brief Handle keyboard interrupt (called from IRQ handler)
     * @param scanCode Raw scan code from keyboard
     */
    static void handle_key_press(u8 scanCode);
    
    /**
     * @brief Get human-readable key name
     * @param scanCode Raw scan code
     * @return Key name string
     */
    static const char* get_key_name(u8 scanCode);
    
    /**
     * @brief Check if a scan code represents a key press (not release)
     * @param scanCode Raw scan code
     * @return true if key press, false if key release
     */
    static bool is_key_press(u8 scanCode) {
        return (scanCode & KEY_RELEASED) == 0;
    }
    
    /**
     * @brief Get the base scan code (without release flag)
     * @param scanCode Raw scan code
     * @return Base scan code
     */
    static u8 get_base_scan_code(u8 scanCode) {
        return scanCode & ~KEY_RELEASED;
    }

    /**
     * @brief Enqueue ASCII character from IRQ context
     */
    static void enqueue_char_from_irq(char ch) {
        if (!ch) return;
        u32 next = (keyHead + 1) % KEY_BUFFER_SIZE;
        if (next != keyTail) {
            keyBuffer[keyHead] = ch;
            keyHead = next;
        }
    }

    /**
     * @brief Dequeue ASCII character if available (non-blocking)
     * @param outCh output char
     * @return true if a character was dequeued
     */
    static bool try_dequeue_char(char& outCh) {
        if (keyHead == keyTail) return false;
        outCh = keyBuffer[keyTail];
        keyTail = (keyTail + 1) % KEY_BUFFER_SIZE;
        return true;
    }

    /**
     * @brief Wait for and dequeue a character (blocking with interrupts enabled)
     */
    static char wait_dequeue_char() {
        char ch;
        while (!try_dequeue_char(ch)) {
            asm volatile("sti");
            asm volatile("hlt");
        }
        return ch;
    }

private:
    /**
     * @brief Read byte from I/O port
     */
    static u8 inb(u16 port) {
        u8 result;
        asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
        return result;
    }
};

} // namespace kira::system 