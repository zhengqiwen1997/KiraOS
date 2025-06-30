#include "display/console.hpp"
#include "core/utils.hpp"

namespace kira::display {

using namespace kira::system;

void ScrollableConsole::initialize() {
    // Initialize all buffer lines
    for (u32 i = 0; i < BUFFER_LINES; i++) {
        clear_buffer_line(i);
        colors[i] = VGA_WHITE_ON_BLUE;
    }
    
    current_line = 0;
    scroll_offset = 0;
    total_lines = 0;
    active = false;
}

void ScrollableConsole::add_message(const char* message, u16 color) {
    if (!message) return;
    
    // Copy message to current buffer line
    copy_to_buffer_line(current_line, message, color);
    
    // Move to next line
    current_line = (current_line + 1) % BUFFER_LINES;
    total_lines++;
    
    // If not in active scroll mode, automatically refresh display
    if (!active) {
        refresh_display();
    }
}

void ScrollableConsole::add_formatted_message(u32 line, u32 col, const char* message, u16 color) {
    // For now, just add as regular message
    // TODO: Implement proper formatting if needed
    add_message(message, color);
}

bool ScrollableConsole::handle_keyboard_input(u8 scan_code) {
    // Handle F1 key to toggle active mode
    if (scan_code == KEY_F1) {
        toggle_active_mode();
        return true;
    }
    
    // Only handle other keys if in active mode
    if (!active) {
        return false;
    }
    
    // Handle scrolling keys
    switch (scan_code) {
        case KEY_UP:
            if (active) {
                scroll_down(1);  // UP arrow should show older messages (scroll down in buffer)
                refresh_display();
                return true;
            }
            break;
            
        case KEY_DOWN:
            if (active) {
                scroll_up(1);    // DOWN arrow should show newer messages (scroll up in buffer)
                refresh_display();
                return true;
            }
            break;
            
        case KEY_PAGE_UP:
            if (active) {
                scroll_up(DISPLAY_LINES);
                refresh_display();
                return true;
            }
            break;
            
        case KEY_PAGE_DOWN:
            if (active) {
                scroll_down(DISPLAY_LINES);
                refresh_display();
                return true;
            }
            break;
            
        case KEY_HOME:
            if (active) {
                scroll_to_top();
                refresh_display();
                return true;
            }
            break;
            
        case KEY_END:
            if (active) {
                scroll_to_bottom();
                refresh_display();
                return true;
            }
            break;
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
    
    if (total_lines > max_display_lines) {
        // We have more messages than can fit, so calculate the start position
        if (active) {
            // In scroll mode, use scroll_offset
            display_start = total_lines - max_display_lines - scroll_offset;
        } else {
            // In normal mode, always show the latest messages
            display_start = total_lines - max_display_lines;
        }
        
        // Ensure display_start is within bounds
        if (display_start < 0) display_start = 0;
        if (display_start + max_display_lines > total_lines) {
            display_start = total_lines - max_display_lines;
        }
    }
    
    // Display the messages using direct buffer access
    int lines_shown = 0;
    for (int i = 0; i < total_lines && lines_shown < max_display_lines; i++) {
        int message_index = display_start + i;
        if (message_index >= total_lines) break;
        
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
    u32 old_offset = scroll_offset;
    if (scroll_offset >= lines) {
        scroll_offset -= lines;
    } else {
        scroll_offset = 0;
    }
}

void ScrollableConsole::scroll_down(u32 lines) {
    u32 max_offset = (total_lines > DISPLAY_LINES) ? total_lines - DISPLAY_LINES : 0;
    u32 old_offset = scroll_offset;
    
    if (scroll_offset + lines <= max_offset) {
        scroll_offset += lines;
    } else {
        scroll_offset = max_offset;
    }
}

void ScrollableConsole::scroll_to_top() {
    scroll_offset = 0;
}

void ScrollableConsole::scroll_to_bottom() {
    if (total_lines > DISPLAY_LINES) {
        scroll_offset = total_lines - DISPLAY_LINES;
    } else {
        scroll_offset = 0;
    }
}

void ScrollableConsole::get_scroll_info(u32& current_top, u32& total_lines_available) const {
    current_top = scroll_offset;
    total_lines_available = total_lines;
}

void ScrollableConsole::clear_buffer_line(u32 line_index) {
    if (line_index >= BUFFER_LINES) return;
    
    for (u32 i = 0; i < LINE_WIDTH; i++) {
        buffer[line_index][i] = ' ';
    }
    buffer[line_index][LINE_WIDTH - 1] = '\0';
}

void ScrollableConsole::copy_to_buffer_line(u32 line_index, const char* text, u16 color) {
    if (line_index >= BUFFER_LINES || !text) return;
    
    clear_buffer_line(line_index);
    colors[line_index] = color;
    
    for (u32 i = 0; text[i] != '\0' && i < (LINE_WIDTH - 1); i++) {
        buffer[line_index][i] = text[i];
    }
}

} // namespace kira::display 