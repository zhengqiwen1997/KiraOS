#include "core/types.hpp"
#include "core/test_processes.hpp"

namespace kira::kernel {

using namespace kira::system;

// Simple test processes for demonstrating scheduling (single iteration)
void process_counter() {
    static u32 counter = 0;
    volatile u16* vga_mem = (volatile u16*)0xB8000;
    
    // Display counter on line 15 (left-aligned)
    int line15_offset = 15 * 80;
    const char* text = "Counter: ";
    int pos = 0; // Start at column 0 (left-aligned)
    
    for (int i = 0; text[i] != '\0'; i++) {
        vga_mem[line15_offset + pos++] = 0x0E00 + text[i]; // Yellow
    }
    
    // Display counter value (last 4 digits)
    vga_mem[line15_offset + pos++] = 0x0F00 + ('0' + ((counter / 1000) % 10));
    vga_mem[line15_offset + pos++] = 0x0F00 + ('0' + ((counter / 100) % 10));
    vga_mem[line15_offset + pos++] = 0x0F00 + ('0' + ((counter / 10) % 10));
    vga_mem[line15_offset + pos++] = 0x0F00 + ('0' + (counter % 10));
    
    counter++;
}

void process_spinner() {
    static u32 spin_pos = 0;
    static const char spinner[] = "|/-\\";
    volatile u16* vga_mem = (volatile u16*)0xB8000;
    
    // Display spinner on line 16 (left-aligned)
    int line16_offset = 16 * 80;
    const char* text = "Spinner: ";
    int pos = 0; // Start at column 0 (left-aligned)
    
    for (int i = 0; text[i] != '\0'; i++) {
        vga_mem[line16_offset + pos++] = 0x0D00 + text[i]; // Magenta
    }
    
    // Display current spinner character
    vga_mem[line16_offset + pos] = 0x0F00 + spinner[spin_pos % 4]; // White
    
    spin_pos++;
}

void process_dots() {
    static u32 dot_pos = 0;
    volatile u16* vga_mem = (volatile u16*)0xB8000;
    
    // Display moving dots on line 17 (left-aligned)
    int line17_offset = 17 * 80;
    
    // Clear previous dots (first 30 columns)
    for (int i = 0; i < 30; i++) {
        vga_mem[line17_offset + i] = 0x0720; // Space
    }
    
    // Display "Dots: " label
    const char* text = "Dots: ";
    int pos = 0; // Start at column 0 (left-aligned)
    for (int i = 0; text[i] != '\0'; i++) {
        vga_mem[line17_offset + pos++] = 0x0A00 + text[i]; // Bright green
    }
    
    // Display moving dot
    int dot_col = pos + (dot_pos % 20); // Move dot across 20 positions after label
    vga_mem[line17_offset + dot_col] = 0x0C00 + '*'; // Red asterisk
    
    dot_pos++;
}

} // namespace kira::kernel 