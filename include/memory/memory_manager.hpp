#pragma once

#include "core/types.hpp"
#include "memory/memory.hpp"

namespace kira::system {

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

    // Simple per-physical-page refcount (for CoW tracking)
    void increment_page_ref(u32 physPageAddr);
    void decrement_page_ref(u32 physPageAddr);
    u32 get_page_ref(u32 physPageAddr) const;

    // Add getter methods for debugging
    u32 get_free_page_count() const { return freePageCount; }
    u32 get_max_free_pages() const { return maxFreePages; }
    
    // Calculate total usable RAM from memory map (static since it only uses global variables)
    static u32 calculate_total_usable_ram();

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

    // CoW refcount table (naive fixed-size)
    static constexpr u32 MAX_REF_ENTRIES = 4096;
    struct RefEntry { u32 phys; u32 count; };
    RefEntry refTable[MAX_REF_ENTRIES] = {};
    // If table overflows, treat unknown pages as shared (non-unique)
    bool refTableOverflow = false;
    u32 find_ref_entry(u32 phys) const;
    u32 alloc_ref_entry(u32 phys);
};

} // namespace kira::system 