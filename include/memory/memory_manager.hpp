#pragma once

#include "core/types.hpp"
#include "memory/memory.hpp"

namespace kira::system {

// Global variables to store memory map info from bootloader
extern u32 gMemoryMapAddr;
extern u32 gMemoryMapCount;

class MemoryManager {
public:
    static MemoryManager& get_instance();

    // Initialize memory management
    void initialize(const MemoryMapEntry* memoryMap, u32 memoryMapSize);

    // Physical memory allocation
    void* allocate_physical_page();
    void free_physical_page(void* page);

    // Add getter methods for debugging
    u32 get_free_page_count() const { return freePageCount; }
    u32 get_max_free_pages() const { return maxFreePages; }
    
    // Calculate total usable RAM from memory map (static since it only uses global variables)
    static u32 calculate_total_usable_ram() {
        u32 total = 0;
        
        if (gMemoryMapAddr == 0 || gMemoryMapCount == 0) {
            return 0;  // No memory map available
        }
        
        MemoryMapEntry* entries = (MemoryMapEntry*)gMemoryMapAddr;
        for (u32 i = 0; i < gMemoryMapCount; i++) {
            if (entries[i].type == static_cast<u32>(MemoryType::USABLE)) {
                // Find the highest usable address
                u32 endAddr = (u32)(entries[i].baseAddress + entries[i].length);
                if (endAddr > total) {
                    total = endAddr;
                }
            }
        }
        
        return total;
    }

    // Virtual memory management
    bool map_page(void* virtualAddr, void* physicalAddr, bool writable = true, bool user = false);
    bool unmap_page(void* virtualAddr);
    void* get_physical_address(void* virtualAddr);

    // Page directory management
    void switch_page_directory(u32* pageDirectory);
    void flush_tlb();

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Memory map from bootloader
    const MemoryMapEntry* memoryMap;
    u32 memoryMapSize;
    
    // Simple stack-based page allocator
    u32* freePageStack;      // Array of free page addresses
    u32 freePageCount;       // Number of free pages in stack
    u32 maxFreePages;        // Maximum pages we can track
    
    // Page directory for kernel space
    u32* pageDirectory;

    // Helper functions (simplified)
    void initialize_page_directory();
};

} // namespace kira::system 