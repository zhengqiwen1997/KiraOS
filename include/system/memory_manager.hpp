#pragma once

#include "system/memory.hpp"
#include "system/types.hpp"

namespace kira::system {

class MemoryManager {
public:
    static MemoryManager& get_instance();

    // Initialize memory management
    void initialize(const MemoryMapEntry* memory_map, u32 memory_map_size);

    // Physical memory allocation
    void* allocate_physical_page();
    void free_physical_page(void* page);

    // Virtual memory management
    bool map_page(void* virtual_addr, void* physical_addr, bool writable = true, bool user = false);
    bool unmap_page(void* virtual_addr);
    void* get_physical_address(void* virtual_addr);

    // Page directory management
    void switch_page_directory(u32* page_directory);
    void flush_tlb();

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Memory map
    const MemoryMapEntry* memory_map;
    u32 memory_map_size;

    // Stack-based allocator data (simpler than bitmap)
    u32* free_page_stack;      // Array of free page addresses
    u32 free_page_count;       // Number of free pages in stack
    u32 max_free_pages;        // Maximum pages we can track

    // Page directory and tables (simplified)
    u32* page_directory;

    // Helper functions (simplified)
    void initialize_page_directory();
};

} // namespace kira::system 