#include "system/types.hpp"
#include "system/memory.hpp"

using namespace kira::system;

// Forward declaration of main kernel function
namespace kira::kernel {
    void main(volatile unsigned short* vga_buffer) noexcept;
}

// Helper function to display hex number
void display_hex(volatile unsigned short* pos, u32 value, int& offset) {
    for (int i = 7; i >= 0; i--) {
        u32 digit = (value >> (i * 4)) & 0xF;
        char hex_char = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        pos[offset++] = 0x1F00 | hex_char;
    }
}

// Helper function to display decimal number
void display_decimal(volatile unsigned short* pos, u32 value, int& offset) {
    if (value == 0) {
        pos[offset++] = 0x1F00 | '0';
        return;
    }
    
    // Convert to string (simple method for small numbers)
    char buffer[12];
    int i = 0;
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Display in reverse order
    for (int j = i - 1; j >= 0; j--) {
        pos[offset++] = 0x1F00 | buffer[j];
    }
}

// Helper function to set cursor position
void set_cursor_position(u8 row, u8 col) {
    u16 position = row * 80 + col;
    
    // Set cursor position via VGA registers
    asm volatile(
        "mov $0x3D4, %%dx\n"     // VGA index register
        "mov $0x0F, %%al\n"      // Cursor location low byte index
        "out %%al, %%dx\n"       // Write index
        "inc %%dx\n"             // VGA data register (0x3D5)
        "mov %0, %%al\n"         // Low byte of position
        "out %%al, %%dx\n"       // Write low byte
        
        "dec %%dx\n"             // Back to index register
        "mov $0x0E, %%al\n"      // Cursor location high byte index
        "out %%al, %%dx\n"       // Write index
        "inc %%dx\n"             // VGA data register
        "mov %1, %%al\n"         // High byte of position
        "out %%al, %%dx"         // Write high byte
        :
        : "r"((u8)(position & 0xFF)), "r"((u8)((position >> 8) & 0xFF))
        : "eax", "edx"
    );
}

// This is called from the custom bootloader (stage2.asm)
extern "C" __attribute__((section(".text._start"))) void _start() {
    // Initialize VGA buffer
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    
    // Clear screen with dark blue background
    for (int i = 0; i < 2000; i++) {
        vga[i] = 0x1F20; // White on blue background, space character
    }
    
    // Display kernel entry message at top (Line 0)
    const char* msg = "!!!KiraOS Kernel - Memory Map Analysis!!!";
    volatile unsigned short* pos = vga;
    for (int i = 0; msg[i] != '\0'; i++) {
        pos[i] = 0x1F00 | msg[i]; // White on blue
    }
    
    // Get memory map from bootloader (passed in registers)
    u32 memory_map_addr = 0;
    u32 memory_map_count = 0;
    
    asm volatile(
        "mov %%ebx, %0\n"
        "mov %%edi, %1"
        : "=r"(memory_map_addr), "=r"(memory_map_count)
        :
        : "ebx", "edi"
    );
    
    // Display memory map summary on line 1
    const char* summary = "Found ";
    volatile unsigned short* line1 = vga + 80;
    int pos_idx = 0;
    
    for (int i = 0; summary[i] != '\0'; i++) {
        line1[pos_idx++] = 0x1F00 | summary[i];
    }
    
    display_decimal(line1, memory_map_count, pos_idx);
    
    const char* regions_text = " memory regions at 0x";
    for (int i = 0; regions_text[i] != '\0'; i++) {
        line1[pos_idx++] = 0x1F00 | regions_text[i];
    }
    
    display_hex(line1, memory_map_addr, pos_idx);
    
    // Parse and display each memory region
    if (memory_map_count > 0 && memory_map_addr != 0) {
        MemoryMapEntry* entries = (MemoryMapEntry*)memory_map_addr;
        
        // Display header on line 3 (skip line 2 for spacing)
        volatile unsigned short* line3 = vga + 240; // Line 3
        const char* header = "Base Address    Size (bytes)    Type";
        for (int i = 0; header[i] != '\0'; i++) {
            line3[i] = 0x1E00 | header[i]; // Yellow on blue
        }
        
        // Display each entry starting from line 4 (limit to 8 entries to fit on screen)
        u32 display_count = (memory_map_count > 8) ? 8 : memory_map_count;
        for (u32 i = 0; i < display_count; i++) {
            volatile unsigned short* line = vga + 320 + (i * 80); // Starting from line 4
            int offset = 0;
            
            // Display base address
            line[offset++] = 0x1F00 | '0';
            line[offset++] = 0x1F00 | 'x';
            display_hex(line, (u32)entries[i].base_address, offset);
            
            // Add spacing to column 16
            while (offset < 16) line[offset++] = 0x1F00 | ' ';
            
            // Display size
            display_decimal(line, (u32)entries[i].length, offset);
            
            // Add spacing to column 32
            while (offset < 32) line[offset++] = 0x1F00 | ' ';
            
            // Display type
            const char* type_str = "Unknown";
            switch (entries[i].type) {
                case 1: type_str = "Usable RAM"; break;
                case 2: type_str = "Reserved"; break;
                case 3: type_str = "ACPI Reclaim"; break;
                case 4: type_str = "ACPI NVS"; break;
                case 5: type_str = "Bad Memory"; break;
            }
            
            for (int j = 0; type_str[j] != '\0'; j++) {
                line[offset++] = 0x1F00 | type_str[j];
            }
        }
        
        // Display total usable memory on line 12 (well separated from entries)
        u64 total_usable = 0;
        for (u32 i = 0; i < memory_map_count; i++) {
            if (entries[i].type == 1) { // Usable RAM
                total_usable += entries[i].length;
            }
        }
        
        volatile unsigned short* line12 = vga + 960; // Line 12
        const char* total_text = "Total Usable Memory: ";
        int bottom_offset = 0;
        for (int i = 0; total_text[i] != '\0'; i++) {
            line12[bottom_offset++] = 0x1A00 | total_text[i]; // Green on blue
        }
        
        // Display total in MB
        u32 total_mb = (u32)(total_usable / (1024 * 1024));
        display_decimal(line12, total_mb, bottom_offset);
        line12[bottom_offset++] = 0x1A00 | ' ';
        line12[bottom_offset++] = 0x1A00 | 'M';
        line12[bottom_offset++] = 0x1A00 | 'B';
        
        // If there are more entries than displayed, show a note on line 16
        if (memory_map_count > display_count) {
            volatile unsigned short* line16 = vga + 1280; // Line 16
            const char* more_text = "(... and ";
            int more_offset = 0;
            for (int i = 0; more_text[i] != '\0'; i++) {
                line16[more_offset++] = 0x1C00 | more_text[i]; // Red on blue
            }
            display_decimal(line16, memory_map_count - display_count, more_offset);
            const char* more_entries = " more entries)";
            for (int i = 0; more_entries[i] != '\0'; i++) {
                line16[more_offset++] = 0x1C00 | more_entries[i];
            }
        }
    }
    
    // Set cursor position to line 18 (where kernel main messages will start)
    set_cursor_position(23, 10);
    
    // Store memory map info in global variables for MemoryManager to access later
    // This avoids calling MemoryManager functions in kernel_entry which causes size issues
    extern u32 g_memory_map_addr;
    extern u32 g_memory_map_count;
    g_memory_map_addr = memory_map_addr;
    g_memory_map_count = memory_map_count;
    
    // Call the main kernel function with VGA buffer
    kira::kernel::main(vga);
    
    // If kernel main returns, halt
    while (true) {
        asm volatile("hlt");
    }
} 