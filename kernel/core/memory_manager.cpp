#include "system/memory_manager.hpp"
#include "system/memory.hpp"

// Global variables to store memory map info from bootloader
kira::system::u32 g_memory_map_addr = 0;
kira::system::u32 g_memory_map_count = 0;

namespace kira::system {

// Simple memset implementation for kernel use
static void* memset(void* dest, int value, u32 count) {
    u8* ptr = (u8*)dest;
    for (u32 i = 0; i < count; i++) {
        ptr[i] = (u8)value;
    }
    return dest;
}

// Test 1: Simple C-style function
extern "C" int simple_test() {
    return 42;
}

// Global pointer to avoid constructor issues
static MemoryManager* g_memory_manager = nullptr;

MemoryManager& MemoryManager::get_instance() {
    if (!g_memory_manager) {
        // Allocate at a safer address, well away from VGA buffer (0xB8000)
        g_memory_manager = (MemoryManager*)0x10000;
        
        // Initialize only the essential fields for stack-based allocator
        g_memory_manager->memory_map = nullptr;
        g_memory_manager->memory_map_size = 0;
        g_memory_manager->page_directory = nullptr;
        g_memory_manager->free_page_stack = nullptr;
        g_memory_manager->free_page_count = 0;
        g_memory_manager->max_free_pages = 0;
        
        // Auto-initialize with memory map if available
        if (g_memory_map_addr != 0 && g_memory_map_count > 0) {
            MemoryMapEntry* entries = (MemoryMapEntry*)g_memory_map_addr;
            g_memory_manager->initialize(entries, g_memory_map_count);
        }
    }
    return *g_memory_manager;
}

// Initialize the stack-based allocator
void MemoryManager::initialize(const MemoryMapEntry* memory_map, u32 memory_map_size) {
    this->memory_map = memory_map;
    this->memory_map_size = memory_map_size;
    
    // Initialize the free page stack
    free_page_stack = (u32*)0x9E000;  // Use same location as before
    free_page_count = 0;
    max_free_pages = 1024;  // Limit to 1024 pages (4MB) to keep it simple
    
    // Populate the stack with free pages from usable memory regions
    for (u32 i = 0; i < memory_map_size && free_page_count < max_free_pages; i++) {
        if (memory_map[i].type == static_cast<u32>(MemoryType::USABLE)) {
            u32 start_page = memory_map[i].base_address / PAGE_SIZE;
            u32 end_page = (memory_map[i].base_address + memory_map[i].length) / PAGE_SIZE;
            
            // Skip low memory (first 1MB) to avoid conflicts
            if (start_page < 256) {  // 256 pages = 1MB
                start_page = 256;
            }
            
            // Add pages to the free stack
            for (u32 page = start_page; page < end_page && free_page_count < max_free_pages; page++) {
                free_page_stack[free_page_count++] = page * PAGE_SIZE;
            }
        }
    }
}

// Stack-based allocator: Pop from free page stack
void* MemoryManager::allocate_physical_page() {
    // Check if we have free pages
    if (free_page_count == 0) {
        return nullptr;  // No free pages available
    }
    
    // Pop from stack (take the last page)
    u32 page_addr = free_page_stack[--free_page_count];
    
    return (void*)page_addr;
}

// Stack-based allocator: Push back to free page stack
void MemoryManager::free_physical_page(void* page) {
    if (!page || free_page_count >= max_free_pages) {
        return;  // Invalid page or stack full
    }
    
    // Push back to stack
    free_page_stack[free_page_count++] = (u32)page;
}

} // namespace kira::system 