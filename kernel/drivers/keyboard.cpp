#include "drivers/keyboard.hpp"

namespace kira::system {

// Static member definitions
bool Keyboard::shiftPressed = false;
bool Keyboard::ctrlPressed = false;
bool Keyboard::altPressed = false;
bool Keyboard::capsLockOn = false;
volatile u32 Keyboard::keyHead = 0;
volatile u32 Keyboard::keyTail = 0;
char Keyboard::keyBuffer[Keyboard::KEY_BUFFER_SIZE];

// Scan code to ASCII conversion table (unshifted)
static const char scan_code_to_ascii_table[128] = {
    0,    0,   '1', '2', '3', '4', '5', '6',     // 0x00-0x07
    '7',  '8', '9', '0', '-', '=', '\b', '\t',   // 0x08-0x0F
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',    // 0x10-0x17
    'o',  'p', '[', ']', '\n', 0,  'a', 's',    // 0x18-0x1F
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',    // 0x20-0x27
    '\'', '`', 0,   '\\','z', 'x', 'c', 'v',    // 0x28-0x2F
    'b',  'n', 'm', ',', '.', '/', 0,   '*',    // 0x30-0x37
    0,    ' ', 0,   0,   0,   0,   0,   0,       // 0x38-0x3F
    0,    0,   0,   0,   0,   0,   0,   '7',     // 0x40-0x47
    '8',  '9', '-', '4', '5', '6', '+', '1',     // 0x48-0x4F
    '2',  '3', '0', '.', 0,   0,   0,   0,       // 0x50-0x57
    0,    0,   0,   0,   0,   0,   0,   0,       // 0x58-0x5F
    0,    0,   0,   0,   0,   0,   0,   0,       // 0x60-0x67
    0,    0,   0,   0,   0,   0,   0,   0,       // 0x68-0x6F
    0,    0,   0,   0,   0,   0,   0,   0,       // 0x70-0x77
    0,    0,   0,   0,   0,   0,   0,   0        // 0x78-0x7F
};

// Scan code to ASCII conversion table (shifted)
static const char scan_code_to_ascii_shifted_table[128] = {
    0,    0,   '!', '@', '#', '$', '%', '^',     // 0x00-0x07
    '&',  '*', '(', ')', '_', '+', '\b', '\t',   // 0x08-0x0F
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',    // 0x10-0x17
    'O',  'P', '{', '}', '\n', 0,  'A', 'S',    // 0x18-0x1F
    'D',  'F', 'G', 'H', 'J', 'K', 'L', ':',    // 0x20-0x27
    '"',  '~', 0,   '|', 'Z', 'X', 'C', 'V',    // 0x28-0x2F
    'B',  'N', 'M', '<', '>', '?', 0,   '*',    // 0x30-0x37
    0,    ' ', 0,   0,   0,   0,   0,   0,       // 0x38-0x3F
    0,    0,   0,   0,   0,   0,   0,   '7',     // 0x40-0x47
    '8',  '9', '-', '4', '5', '6', '+', '1',     // 0x48-0x4F
    '2',  '3', '0', '.', 0,   0,   0,   0,       // 0x50-0x57
    0,    0,   0,   0,   0,   0,   0,   0,       // 0x58-0x5F
    0,    0,   0,   0,   0,   0,   0,   0,       // 0x60-0x67
    0,    0,   0,   0,   0,   0,   0,   0,       // 0x68-0x6F
    0,    0,   0,   0,   0,   0,   0,   0,       // 0x70-0x77
    0,    0,   0,   0,   0,   0,   0,   0        // 0x78-0x7F
};

void Keyboard::initialize() {
    // Reset keyboard state
    shiftPressed = false;
    ctrlPressed = false;
    altPressed = false;
    capsLockOn = false;
}

char Keyboard::scan_code_to_ascii(u8 scanCode) {
    // Remove release flag
    u8 baseCode = get_base_scan_code(scanCode);
    
    // Check bounds
    if (baseCode >= 128) {
        return 0;
    }
    
    // Get character from appropriate table
    char ch;
    if (shiftPressed) {
        ch = scan_code_to_ascii_shifted_table[baseCode];
    } else {
        ch = scan_code_to_ascii_table[baseCode];
    }
    
    // Handle caps lock for letters
    if (capsLockOn && ch >= 'a' && ch <= 'z') {
        ch = ch - 'a' + 'A';
          } else if (capsLockOn && ch >= 'A' && ch <= 'Z') {
        ch = ch - 'A' + 'a';
    }
    
    return ch;
}

void Keyboard::handle_key_press(u8 scanCode) {
    u8 baseCode = get_base_scan_code(scanCode);
    bool isPress = is_key_press(scanCode);
    
    // Handle modifier keys
    switch (baseCode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            shiftPressed = isPress;
            break;
            
        case KEY_LCTRL:
            ctrlPressed = isPress;
            break;
            
        case KEY_LALT:
            altPressed = isPress;
            break;
            
        case KEY_CAPS_LOCK:
            if (isPress) {
                capsLockOn = !capsLockOn;
            }
            break;
    }

    // No enqueue here; IRQ handler decides whether to deliver to user or enqueue.
}

const char* Keyboard::get_key_name(u8 scanCode) {
    u8 baseCode = get_base_scan_code(scanCode);
    
    switch (baseCode) {
        case KEY_ESC: return "ESC";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_F7: return "F7";
        case KEY_F8: return "F8";
        case KEY_F9: return "F9";
        case KEY_F10: return "F10";
        case KEY_F11: return "F11";
        case KEY_F12: return "F12";
        
        case KEY_1: return "1";
        case KEY_2: return "2";
        case KEY_3: return "3";
        case KEY_4: return "4";
        case KEY_5: return "5";
        case KEY_6: return "6";
        case KEY_7: return "7";
        case KEY_8: return "8";
        case KEY_9: return "9";
        case KEY_0: return "0";
        
        case KEY_Q: return "Q";
        case KEY_W: return "W";
        case KEY_E: return "E";
        case KEY_R: return "R";
        case KEY_T: return "T";
        case KEY_Y: return "Y";
        case KEY_U: return "U";
        case KEY_I: return "I";
        case KEY_O: return "O";
        case KEY_P: return "P";
        case KEY_A: return "A";
        case KEY_S: return "S";
        case KEY_D: return "D";
        case KEY_F: return "F";
        case KEY_G: return "G";
        case KEY_H: return "H";
        case KEY_J: return "J";
        case KEY_K: return "K";
        case KEY_L: return "L";
        case KEY_Z: return "Z";
        case KEY_X: return "X";
        case KEY_C: return "C";
        case KEY_V: return "V";
        case KEY_B: return "B";
        case KEY_N: return "N";
        case KEY_M: return "M";
        
        case KEY_SPACE: return "SPACE";
        case KEY_ENTER: return "ENTER";
        case KEY_BACKSPACE: return "BACKSPACE";
        case KEY_TAB: return "TAB";
        case KEY_LSHIFT: return "LSHIFT";
        case KEY_RSHIFT: return "RSHIFT";
        case KEY_LCTRL: return "LCTRL";
        case KEY_LALT: return "LALT";
        case KEY_CAPS_LOCK: return "CAPS";
        
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        case KEY_PAGE_UP: return "PGUP";
        case KEY_PAGE_DOWN: return "PGDN";
        case KEY_HOME: return "HOME";
        case KEY_END: return "END";
        case KEY_INSERT: return "INS";
        case KEY_DELETE: return "DEL";
        
        default: return "UNKNOWN";
    }
}

} // namespace kira::system 