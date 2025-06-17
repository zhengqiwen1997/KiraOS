#include "system/types.hpp"
#include "system/memory.hpp"
#include "system/memory_manager.hpp"
#include "display/vga_display.hpp"

using namespace kira::system;
using namespace kira::display;

// Forward declaration of main kernel function
namespace kira::kernel {
    void main(volatile unsigned short* vga_buffer) noexcept;
}

// Helper function to get memory type string
const char* get_memory_type_string(u32 type) {
    switch (type) {
        case 1: return "Usable RAM";
        case 2: return "Reserved";
        case 3: return "ACPI Reclaim";
        case 4: return "ACPI NVS";
        case 5: return "Bad Memory";
        default: return "Unknown";
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
    // Initialize VGA display system
    VGADisplay vga;
    
    // Clear screen with blue background
    vga.clear_screen(VGA_WHITE_ON_BLUE);
    
    // Display kernel entry message at top
    vga.print_string(0, 0, "!!!KiraOS Kernel - Memory Map Analysis!!!", VGA_WHITE_ON_BLUE);
    
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
    vga.print_string(1, 0, "Found ");
    vga.print_decimal(1, 6, memory_map_count);
    vga.print_string(1, 8, " memory regions at 0x");
    vga.print_hex(1, 30, memory_map_addr);
    
    // Parse and display each memory region
    if (memory_map_count > 0 && memory_map_addr != 0) {
        MemoryMapEntry* entries = (MemoryMapEntry*)memory_map_addr;
        
        // Display header on line 3
        vga.print_string(3, 0, "Base Address    Size (bytes)    Type", VGA_YELLOW_ON_BLUE);
        
        // Display each entry starting from line 4 (limit to 8 entries to fit on screen)
        u32 display_count = (memory_map_count > 8) ? 8 : memory_map_count;
        for (u32 i = 0; i < display_count; i++) {
            u32 line = 4 + i;
            
            // Display base address
            vga.print_hex(line, 0, (u32)entries[i].base_address);
            
            // Display size
            vga.print_decimal(line, 16, (u32)entries[i].length);
            
            // Display type
            const char* type_str = get_memory_type_string(entries[i].type);
            vga.print_string(line, 32, type_str);
        }
        
        // Display total usable memory on line 12
        u64 total_usable = 0;
        for (u32 i = 0; i < memory_map_count; i++) {
            if (entries[i].type == 1) { // Usable RAM
                total_usable += entries[i].length;
            }
        }
        
        vga.print_string(12, 0, "Total Usable Memory: ", VGA_GREEN_ON_BLUE);
        u32 total_mb = (u32)(total_usable / (1024 * 1024));
        vga.print_decimal(12, 21, total_mb, VGA_GREEN_ON_BLUE);
        vga.print_string(12, 25, " MB", VGA_GREEN_ON_BLUE);
        
        // If there are more entries than displayed, show a note
        if (memory_map_count > display_count) {
            vga.print_string(14, 0, "(... and ", VGA_RED_ON_BLUE);
            vga.print_decimal(14, 9, memory_map_count - display_count, VGA_RED_ON_BLUE);
            vga.print_string(14, 11, " more entries)", VGA_RED_ON_BLUE);
        }
    }
    
    // Store memory map info in global variables for MemoryManager to access later
    extern u32 g_memory_map_addr;
    extern u32 g_memory_map_count;
    g_memory_map_addr = memory_map_addr;
    g_memory_map_count = memory_map_count;
    
    // Set cursor position to line 18 (where kernel main messages will start)
    set_cursor_position(18, 0);
    
    // Call the main kernel function with VGA buffer for compatibility
    kira::kernel::main((volatile unsigned short*)VGA_BUFFER);
    
    // Should never reach here
    while (true) {
        asm volatile("hlt");
    }
} 