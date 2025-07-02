#pragma once

#include "core/types.hpp"

namespace kira {
namespace system {

using namespace kira::system;

// ========== KEYBOARD SCAN CODES ==========
// Function keys
const u8 KEY_ESC = 0x01;
const u8 KEY_F1 = 0x3B;
const u8 KEY_F2 = 0x3C;
const u8 KEY_F3 = 0x3D;
const u8 KEY_F4 = 0x3E;
const u8 KEY_F5 = 0x3F;
const u8 KEY_F6 = 0x40;
const u8 KEY_F7 = 0x41;
const u8 KEY_F8 = 0x42;
const u8 KEY_F9 = 0x43;
const u8 KEY_F10 = 0x44;
const u8 KEY_F11 = 0x57;
const u8 KEY_F12 = 0x58;

// Number row
const u8 KEY_1 = 0x02;
const u8 KEY_2 = 0x03;
const u8 KEY_3 = 0x04;
const u8 KEY_4 = 0x05;
const u8 KEY_5 = 0x06;
const u8 KEY_6 = 0x07;
const u8 KEY_7 = 0x08;
const u8 KEY_8 = 0x09;
const u8 KEY_9 = 0x0A;
const u8 KEY_0 = 0x0B;

// Letters
const u8 KEY_Q = 0x10;
const u8 KEY_W = 0x11;
const u8 KEY_E = 0x12;
const u8 KEY_R = 0x13;
const u8 KEY_T = 0x14;
const u8 KEY_Y = 0x15;
const u8 KEY_U = 0x16;
const u8 KEY_I = 0x17;
const u8 KEY_O = 0x18;
const u8 KEY_P = 0x19;
const u8 KEY_A = 0x1E;
const u8 KEY_S = 0x1F;
const u8 KEY_D = 0x20;
const u8 KEY_F = 0x21;
const u8 KEY_G = 0x22;
const u8 KEY_H = 0x23;
const u8 KEY_J = 0x24;
const u8 KEY_K = 0x25;
const u8 KEY_L = 0x26;
const u8 KEY_Z = 0x2C;
const u8 KEY_X = 0x2D;
const u8 KEY_C = 0x2E;
const u8 KEY_V = 0x2F;
const u8 KEY_B = 0x30;
const u8 KEY_N = 0x31;
const u8 KEY_M = 0x32;

// Special keys
const u8 KEY_SPACE = 0x39;
const u8 KEY_ENTER = 0x1C;
const u8 KEY_BACKSPACE = 0x0E;
const u8 KEY_TAB = 0x0F;
const u8 KEY_LSHIFT = 0x2A;
const u8 KEY_RSHIFT = 0x36;
const u8 KEY_LCTRL = 0x1D;
const u8 KEY_LALT = 0x38;
const u8 KEY_CAPS_LOCK = 0x3A;

// Arrow keys
const u8 KEY_UP = 0x48;
const u8 KEY_DOWN = 0x50;
const u8 KEY_LEFT = 0x4B;
const u8 KEY_RIGHT = 0x4D;

// Navigation keys
const u8 KEY_PAGE_UP = 0x49;
const u8 KEY_PAGE_DOWN = 0x51;
const u8 KEY_HOME = 0x47;
const u8 KEY_END = 0x4F;
const u8 KEY_INSERT = 0x52;
const u8 KEY_DELETE = 0x53;

// Key release flag (bit 7 set)
const u8 KEY_RELEASED = 0x80;

// Keyboard ports
const u16 KEYBOARD_DATA_PORT = 0x60;
const u16 KEYBOARD_STATUS_PORT = 0x64;

/**
 * @brief Enhanced keyboard input handler with interrupt integration
 */
class Keyboard {
private:
    static bool shiftPressed;
    static bool ctrlPressed;
    static bool altPressed;
    static bool capsLockOn;

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

} // namespace system
} // namespace kira 