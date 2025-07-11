#pragma once

#include "display/vga.hpp"
#include "drivers/keyboard.hpp"
#include "core/types.hpp"
#include "core/utils.hpp"  // For String class

namespace kira::display {

using namespace kira::system;
using kira::utils::String;  // Add String to scope

/**
 * @brief Scrollable Console with Keyboard Controls
 * 
 * Provides a scrollable text console with keyboard navigation:
 * - Arrow Up/Down: Scroll one line
 * - Page Up/Down: Scroll one page
 * - Home/End: Jump to top/bottom
 * - Stores much more text than fits on screen
 */
class ScrollableConsole {
private:
    // Console configuration
    static constexpr u32 BUFFER_LINES = 1000;  // Total lines to store
    static constexpr u32 DISPLAY_LINES = 24;   // Lines visible on screen (lines 0-23)
    static constexpr u32 LINE_WIDTH = 80;      // Characters per line
    
    // Console buffer
    char buffer[BUFFER_LINES][LINE_WIDTH];
    u16 colors[BUFFER_LINES];              // Color for each line
    u32 currentLine;                       // Next line to write to
    u32 scrollOffset;                      // Top line currently displayed
    u32 totalLines;                        // Total lines written
    
    bool active;                           // Whether console is actively scrolling
    
public:
    /**
     * @brief Initialize the scrollable console
     */
    void initialize();
    
    /**
     * @brief Add a message to the console buffer
     * @param message Text to add
     * @param color Color for the text
     */
    void add_message(const char* message, u16 color = VGA_WHITE_ON_BLUE);
    
    /**
     * @brief Add a message using a String object
     * @param message String object containing the message
     * @param color Color for the text
     */
    void add_message(const String& message, u16 color = VGA_WHITE_ON_BLUE) {
        add_message(message.c_str(), color);
    }
    
    /**
     * @brief Add a formatted message with line/column info
     * @param line Line number to display
     * @param col Column to start at
     * @param message Text to add
     * @param color Color for the text
     */
    void add_formatted_message(u32 line, u32 col, const char* message, u16 color = VGA_WHITE_ON_BLUE);
    
    /**
     * @brief Handle keyboard input for scrolling
     * @param scanCode Keyboard scan code
     * @return true if key was handled
     */
    bool handle_keyboard_input(u8 scanCode);
    
    /**
     * @brief Refresh the display with current scroll position
     */
    void refresh_display();
    
    /**
     * @brief Toggle console active mode (scrolling vs normal display)
     */
    void toggle_active_mode();
    
    /**
     * @brief Check if console is in active scrolling mode
     */
    bool is_active() const { return active; }
    
    /**
     * @brief Scroll up by specified number of lines
     * @param lines Number of lines to scroll up
     */
    void scroll_up(u32 lines = 1);
    
    /**
     * @brief Scroll down by specified number of lines
     * @param lines Number of lines to scroll down
     */
    void scroll_down(u32 lines = 1);
    
    /**
     * @brief Jump to top of buffer
     */
    void scroll_to_top();
    
    /**
     * @brief Jump to bottom of buffer (latest messages)
     */
    void scroll_to_bottom();
    
    /**
     * @brief Get current scroll position info
     */
    void get_scroll_info(u32& currentTop, u32& totalLinesAvailable) const;

private:
    /**
     * @brief Clear a line in the buffer
     */
    void clear_buffer_line(u32 lineIndex);
    
    /**
     * @brief Copy string to buffer line (with bounds checking)
     */
    void copy_to_buffer_line(u32 lineIndex, const char* text, u16 color);
    
    /**
     * @brief Handle multiline messages (parse \n characters)
     */
    void add_multiline_message(const char* message, u16 color);
    
    /**
     * @brief Internal refresh function (assumes interrupts disabled)
     */
    void refresh_display_internal();
    
    /**
     * @brief Internal toggle active mode (assumes interrupts disabled)
     */
    void toggle_active_mode_internal();
    
    /**
     * @brief Internal add message (assumes interrupts disabled)
     */
    void add_message_internal(const char* message, u16 color = VGA_WHITE_ON_BLUE);
};

} // namespace kira::display 