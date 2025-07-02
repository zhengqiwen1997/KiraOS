#pragma once

#include "core/types.hpp"
#include "memory/memory.hpp"

namespace kira::system {

class MemoryManager {
public:
    static MemoryManager& get_instance();

    // Initialize memory management
    void initialize(const MemoryMapEntry* memoryMap, u32 memoryMapSize);

    // Physical memory allocation
    void* allocate_physical_page();
    void free_physical_page(void* page);

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