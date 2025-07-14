#include "display/console.hpp"
#include "drivers/keyboard.hpp"

namespace kira::display {

using namespace kira::system;

// Helper functions for interrupt safety
static inline void disable_interrupts() {
    asm volatile("cli");
}

static inline void enable_interrupts() {
    asm volatile("sti");
}

// Check if interrupts are currently enabled
static inline bool interrupts_enabled() {
    u32 flags;
    asm volatile("pushfl; popl %0" : "=r"(flags));
    return (flags & 0x200) != 0;
}

void ScrollableConsole::initialize() {
    // Initialize all buffer lines
    for (u32 i = 0; i < BUFFER_LINES; i++) {
        clear_buffer_line(i);
        colors[i] = VGA_WHITE_ON_BLUE;
    }
    
    currentLine = 0;
    scrollOffset = 0;
    totalLines = 0;
    active = false;
}

void ScrollableConsole::add_message(const char* message, u16 color) {
    if (!message) return;
    
    // Check if we're in an exception context (interrupts disabled)
    bool was_interrupts_enabled = interrupts_enabled();
    
    // Check if message contains newlines
    bool has_newlines = false;
    for (u32 i = 0; message[i] != '\0'; i++) {
        if (message[i] == '\n') {
            has_newlines = true;
            break;
        }
    }
    
    if (has_newlines) {
        // Parse message and split on newlines
        add_multiline_message(message, color);
    } else {
        // Single line message - use original logic
        copy_to_buffer_line(currentLine, message, color);
        currentLine = (currentLine + 1) % BUFFER_LINES;
        totalLines++;
    }
    
    // If not in active scroll mode, automatically refresh display
    if (!active) {
        // Use the safe refresh approach from original console.cpp
        refresh_display();
    }
    
    // Don't mess with interrupt state if we're in an exception handler
    (void)was_interrupts_enabled;  // Suppress unused variable warning
}

void ScrollableConsole::add_multiline_message(const char* message, u16 color) {
    if (!message) return;
    
    char line_buffer[LINE_WIDTH];
    u32 line_pos = 0;
    u32 message_len = 0;
    
    // Calculate message length
    while (message[message_len] != '\0') {
        message_len++;
    }
    
    // Track if the message ends with a newline
    bool ends_with_newline = (message_len > 0 && message[message_len - 1] == '\n');
    
    for (u32 i = 0; i < message_len; i++) {
        if (message[i] == '\n') {
            // End current line
            line_buffer[line_pos] = '\0';
            
            // Add the line to console
            copy_to_buffer_line(currentLine, line_buffer, color);
            currentLine = (currentLine + 1) % BUFFER_LINES;
            totalLines++;
            
            // Reset for next line
            line_pos = 0;
        } else if (line_pos < LINE_WIDTH - 1) {
            // Add character to current line
            line_buffer[line_pos++] = message[i];
        } else {
            // Line too long, force a break
            line_buffer[line_pos] = '\0';
            copy_to_buffer_line(currentLine, line_buffer, color);
            currentLine = (currentLine + 1) % BUFFER_LINES;
            totalLines++;
            
            // Start new line with current character
            line_buffer[0] = message[i];
            line_pos = 1;
        }
    }
    
    // Add any remaining content as final line
    // If message ends with \n, this will be empty (which is what we want)
    if (line_pos > 0 || ends_with_newline) {
        line_buffer[line_pos] = '\0';
        copy_to_buffer_line(currentLine, line_buffer, color);
        currentLine = (currentLine + 1) % BUFFER_LINES;
        totalLines++;
    }
}

void ScrollableConsole::add_formatted_message(u32 line, u32 col, const char* message, u16 color) {
    // For now, just add as regular message
    // TODO: Implement proper formatting if needed
    add_message(message, color);
}

bool ScrollableConsole::handle_keyboard_input(u8 scanCode) {
    // Only disable interrupts if they're currently enabled
    bool was_enabled = interrupts_enabled();
    if (was_enabled) {
        disable_interrupts();
    }
    
    // Handle F1 key to toggle active mode
    if (scanCode == KEY_F1) {
        toggle_active_mode();
        if (was_enabled) {
            enable_interrupts();
        }
        return true;
    }
    
    // Only handle other keys if in active mode
    if (!active) {
        if (was_enabled) {
            enable_interrupts();
        }
        return false;
    }
    
    // Handle scrolling keys
    switch (scanCode) {
        case KEY_UP:
            scroll_up(1);  // UP arrow should show older messages (scroll down in buffer)
            refresh_display();
            break;
            
        case KEY_DOWN:
            scroll_down(1);    // DOWN arrow should show newer messages (scroll up in buffer)
            refresh_display();
            break;
            
        case KEY_PAGE_UP:
            scroll_up(DISPLAY_LINES);
            refresh_display();
            break;
            
        case KEY_PAGE_DOWN:
            scroll_down(DISPLAY_LINES);
            refresh_display();
            break;
            
        case KEY_HOME:
            scroll_to_top();
            refresh_display();
            break;
            
        case KEY_END:
            scroll_to_bottom();
            refresh_display();
            break;
            
        default:
            if (was_enabled) {
                enable_interrupts();
            }
            return false;  // Key not handled
    }
    
    if (was_enabled) {
        enable_interrupts();
    }
    return true;
}

void ScrollableConsole::refresh_display() {
    // Use direct VGA buffer access instead of vga class
    volatile u16* vgaBuffer = (volatile u16*)0xB8000;
    
    // Clear the entire console area (lines 0-23) using direct buffer access
    for (int line = 0; line <= 23; line++) {
        for (int col = 0; col < 80; col++) {
            vgaBuffer[line * 80 + col] = VGA_WHITE_ON_BLUE | ' ';
        }
    }
    
    // Calculate which messages to display
    int displayStart = 0;
    int maxDisplayLines = 24;  // Lines 0-23 = 24 lines
    
    if (totalLines > maxDisplayLines) {
        // We have more messages than can fit, so calculate the start position
        if (active) {
            // In scroll mode, use scrollOffset
            displayStart = totalLines - maxDisplayLines - scrollOffset;
        } else {
            // In normal mode, always show the latest messages
            displayStart = totalLines - maxDisplayLines;
        }
        
        // Ensure display_start is within bounds
        if (displayStart < 0) displayStart = 0;
        if (displayStart + maxDisplayLines > totalLines) {
            displayStart = totalLines - maxDisplayLines;
        }
    }
    
    // Display the messages using direct buffer access
    int linesShown = 0;
    for (int i = 0; i < totalLines && linesShown < maxDisplayLines; i++) {
        int messageIndex = displayStart + i;
        if (messageIndex >= totalLines) break;
        
        int bufferIndex = messageIndex % BUFFER_LINES;
        int displayLine = 0 + linesShown;  // Start from line 0
        
        // Copy message to VGA buffer directly
        for (int col = 0; col < 80 && buffer[bufferIndex][col] != '\0'; col++) {
            vgaBuffer[displayLine * 80 + col] = colors[bufferIndex] | buffer[bufferIndex][col];
        }
        linesShown++;
    }
    
    // Draw status line at line 24 using direct buffer access
    if (active) {
        const char* statusMsg = " SCROLL MODE - Use Arrow Keys, Page Up/Down, Home/End, F1 to Exit ";
        for (int i = 0; statusMsg[i] != '\0' && i < 80; i++) {
            vgaBuffer[24 * 80 + i] = VGA_BLACK_ON_CYAN | statusMsg[i];
        }
    } else {
        const char* statusMsg = " NORMAL MODE - F1 to Enter Scroll Mode ";
        for (int i = 0; statusMsg[i] != '\0' && i < 80; i++) {
            vgaBuffer[24 * 80 + i] = VGA_WHITE_ON_BLUE | statusMsg[i];
        }
        // Clear rest of status line
        for (int i = 39; i < 80; i++) {
            vgaBuffer[24 * 80 + i] = VGA_WHITE_ON_BLUE | ' ';
        }
    }
}

void ScrollableConsole::toggle_active_mode() {
    active = !active;
    
    if (active) {
        // Entering scroll mode - stay at current position
        add_message(">>> SCROLL MODE ACTIVE <<<", VGA_GREEN_ON_BLUE);
    } else {
        // Exiting scroll mode - jump to bottom
        scroll_to_bottom();
        add_message(">>> NORMAL MODE <<<", VGA_YELLOW_ON_BLUE);
    }
    
    refresh_display();
}

void ScrollableConsole::scroll_up(u32 lines) {
    u32 maxOffset = (totalLines > DISPLAY_LINES) ? totalLines - DISPLAY_LINES : 0;
    
    if (scrollOffset + lines <= maxOffset) {
        scrollOffset += lines;
    } else {
        scrollOffset = maxOffset;
    }
}

void ScrollableConsole::scroll_down(u32 lines) {
    if (scrollOffset >= lines) {
        scrollOffset -= lines;
    } else {
        scrollOffset = 0;
    }
}

void ScrollableConsole::scroll_to_top() {
    if (totalLines > DISPLAY_LINES) {
        scrollOffset = totalLines - DISPLAY_LINES;
    } else {
        scrollOffset = 0;
    }
}

void ScrollableConsole::scroll_to_bottom() {
    scrollOffset = 0;
}

void ScrollableConsole::get_scroll_info(u32& currentTop, u32& totalLinesAvailable) const {
    currentTop = scrollOffset;
    totalLinesAvailable = totalLines;
}

void ScrollableConsole::clear_buffer_line(u32 lineIndex) {
    if (lineIndex >= BUFFER_LINES) return;
    
    for (u32 i = 0; i < LINE_WIDTH; i++) {
        buffer[lineIndex][i] = ' ';
    }
    buffer[lineIndex][LINE_WIDTH - 1] = '\0';
}

void ScrollableConsole::copy_to_buffer_line(u32 lineIndex, const char* text, u16 color) {
    if (lineIndex >= BUFFER_LINES || !text) return;
    
    clear_buffer_line(lineIndex);
    colors[lineIndex] = color;
    
    for (u32 i = 0; text[i] != '\0' && i < (LINE_WIDTH - 1); i++) {
        buffer[lineIndex][i] = text[i];
    }
}

} // namespace kira::display 