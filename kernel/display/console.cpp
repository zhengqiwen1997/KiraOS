#include "display/console.hpp"
#include "core/utils.hpp"

namespace kira::display {

using namespace kira::system;

void ScrollableConsole::initialize() {
    // Clear all buffer lines
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
    
    // Add to circular buffer
    copy_to_buffer_line(currentLine, message, color);
    
    // Move to next line (circular)
    currentLine = (currentLine + 1) % BUFFER_LINES;
    totalLines++;
    
    // Auto-refresh display if not in active scroll mode
    if (!active) {
        refresh_display();
    }
}

void ScrollableConsole::add_formatted_message(u32 line, u32 col, const char* message, u16 color) {
    // For now, just add as regular message - formatting could be enhanced later
    add_message(message, color);
}

bool ScrollableConsole::handle_keyboard_input(u8 scanCode) {
    if (!active) {
        if (scanCode == KEY_F1) {
            active = true;
            refresh_display();
            return true;
        }
        return false;
    }
    
    // Handle scroll mode keys
    switch (scanCode) {
        case KEY_UP:
            scroll_down(1);  // UP arrow should show older messages (scroll down in buffer)
            refresh_display();
            return true;
            
        case KEY_DOWN:
            scroll_up(1);    // DOWN arrow should show newer messages (scroll up in buffer)
            refresh_display();
            return true;
            
        case KEY_PAGE_UP:
            scroll_up(DISPLAY_LINES);
            refresh_display();
            return true;
            
        case KEY_PAGE_DOWN:
            scroll_down(DISPLAY_LINES);
            refresh_display();
            return true;
            
        case KEY_HOME:
            scroll_to_top();
            refresh_display();
            return true;
            
        case KEY_END:
            scroll_to_bottom();
            refresh_display();
            return true;
    }
    
    return false;  // Key not handled
}

void ScrollableConsole::refresh_display() {
    // Use direct VGA buffer access instead of vga class
    volatile u16* vga_buffer = (volatile u16*)0xB8000;
    
    // Clear the entire console area (lines 0-23) using direct buffer access
    for (int line = 0; line <= 23; line++) {
        for (int col = 0; col < 80; col++) {
            vga_buffer[line * 80 + col] = VGA_WHITE_ON_BLUE | ' ';
        }
    }
    
    // Calculate which messages to display
    int display_start = 0;
    int max_display_lines = 24;  // Lines 0-23 = 24 lines
    
    if (totalLines > max_display_lines) {
        // We have more messages than can fit, so calculate the start position
        if (active) {
            // In scroll mode, use scroll_offset
            display_start = totalLines - max_display_lines - scrollOffset;
        } else {
            // In normal mode, always show the latest messages
            display_start = totalLines - max_display_lines;
        }
        
        // Ensure display_start is within bounds
        if (display_start < 0) display_start = 0;
        if (display_start + max_display_lines > totalLines) {
            display_start = totalLines - max_display_lines;
        }
    }
    
    // Display the messages using direct buffer access
    int lines_shown = 0;
    for (int i = 0; i < totalLines && lines_shown < max_display_lines; i++) {
        int message_index = display_start + i;
        if (message_index >= totalLines) break;
        
        int buffer_index = message_index % BUFFER_LINES;
        int display_line = 0 + lines_shown;  // Start from line 0
        
        // Copy message to VGA buffer directly
        for (int col = 0; col < 80 && buffer[buffer_index][col] != '\0'; col++) {
            vga_buffer[display_line * 80 + col] = colors[buffer_index] | buffer[buffer_index][col];
        }
        lines_shown++;
    }
    
    // Draw status line at line 24 using direct buffer access
    if (active) {
        const char* status_msg = " SCROLL MODE - Use Arrow Keys, Page Up/Down, Home/End, F1 to Exit ";
        for (int i = 0; status_msg[i] != '\0' && i < 80; i++) {
            vga_buffer[24 * 80 + i] = VGA_BLACK_ON_CYAN | status_msg[i];
        }
    } else {
        const char* status_msg = " NORMAL MODE - F1 to Enter Scroll Mode ";
        for (int i = 0; status_msg[i] != '\0' && i < 80; i++) {
            vga_buffer[24 * 80 + i] = VGA_WHITE_ON_BLUE | status_msg[i];
        }
        // Clear rest of status line
        for (int i = 39; i < 80; i++) {
            vga_buffer[24 * 80 + i] = VGA_WHITE_ON_BLUE | ' ';
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
    u32 old_offset = scrollOffset;
    if (scrollOffset >= lines) {
        scrollOffset -= lines;
    } else {
        scrollOffset = 0;
    }
}

void ScrollableConsole::scroll_down(u32 lines) {
    u32 max_offset = (totalLines > DISPLAY_LINES) ? totalLines - DISPLAY_LINES : 0;
    u32 old_offset = scrollOffset;
    
    if (scrollOffset + lines <= max_offset) {
        scrollOffset += lines;
    } else {
        scrollOffset = max_offset;
    }
}

void ScrollableConsole::scroll_to_top() {
    scrollOffset = 0;
}

void ScrollableConsole::scroll_to_bottom() {
    if (totalLines > DISPLAY_LINES) {
        scrollOffset = totalLines - DISPLAY_LINES;
    } else {
        scrollOffset = 0;
    }
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