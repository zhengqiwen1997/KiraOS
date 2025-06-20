#pragma once

#include "core/types.hpp"

namespace kira::system {

// Memory map entry structure (matches E820h format)
struct MemoryMapEntry {
    u64 base_address;    // Base address of the memory region
    u64 length;         // Length of the memory region
    u32 type;          // Type of memory region
    u32 acpi;          // ACPI 3.0 extended attributes
} __attribute__((packed));

// Memory region types (from E820h)
enum class MemoryType : u32 {
    USABLE = 1,        // Usable memory
    RESERVED = 2,      // Reserved memory
    ACPI_RECLAIM = 3,  // ACPI reclaimable memory
    ACPI_NVS = 4,      // ACPI NVS memory
    BAD = 5            // Bad memory
};

// Page size constants
constexpr u32 PAGE_SIZE = 4096;           // 4KB page size
constexpr u32 PAGE_SHIFT = 12;            // Shift for page size
constexpr u32 PAGE_MASK = 0xFFFFF000;     // Mask for page alignment

// Page table entry flags
constexpr u32 PAGE_PRESENT = 1 << 0;      // Page is present
constexpr u32 PAGE_WRITE = 1 << 1;        // Page is writable
constexpr u32 PAGE_USER = 1 << 2;         // Page is user accessible
constexpr u32 PAGE_WRITETHROUGH = 1 << 3; // Write-through caching
constexpr u32 PAGE_NOCACHE = 1 << 4;      // Disable caching
constexpr u32 PAGE_ACCESSED = 1 << 5;     // Page has been accessed
constexpr u32 PAGE_DIRTY = 1 << 6;        // Page has been written to
constexpr u32 PAGE_GLOBAL = 1 << 8;       // Global page

// Page directory entry (PDE)
struct PageDirectoryEntry {
    u32 value;

    // Get/set physical address (20 bits)
    u32 get_address() const { return value & PAGE_MASK; }
    void set_address(u32 addr) { value = (value & ~PAGE_MASK) | (addr & PAGE_MASK); }

    // Get/set flags
    bool is_present() const { return value & PAGE_PRESENT; }
    void set_present(bool present) { value = (value & ~PAGE_PRESENT) | (present ? PAGE_PRESENT : 0); }

    bool is_writable() const { return value & PAGE_WRITE; }
    void set_writable(bool writable) { value = (value & ~PAGE_WRITE) | (writable ? PAGE_WRITE : 0); }

    bool is_user() const { return value & PAGE_USER; }
    void set_user(bool user) { value = (value & ~PAGE_USER) | (user ? PAGE_USER : 0); }
} __attribute__((packed));

// Page table entry (PTE)
struct PageTableEntry {
    u32 value;

    // Get/set physical address (20 bits)
    u32 get_address() const { return value & PAGE_MASK; }
    void set_address(u32 addr) { value = (value & ~PAGE_MASK) | (addr & PAGE_MASK); }

    // Get/set flags
    bool is_present() const { return value & PAGE_PRESENT; }
    void set_present(bool present) { value = (value & ~PAGE_PRESENT) | (present ? PAGE_PRESENT : 0); }

    bool is_writable() const { return value & PAGE_WRITE; }
    void set_writable(bool writable) { value = (value & ~PAGE_WRITE) | (writable ? PAGE_WRITE : 0); }

    bool is_user() const { return value & PAGE_USER; }
    void set_user(bool user) { value = (value & ~PAGE_USER) | (user ? PAGE_USER : 0); }
} __attribute__((packed));

} // namespace kira::system 