#pragma once

#include "core/types.hpp"

namespace kira::system {

/**
 * @brief GDT Entry structure
 */
struct GDTEntry {
    u16 limit_low;      // Lower 16 bits of limit
    u16 base_low;       // Lower 16 bits of base
    u8  base_middle;    // Next 8 bits of base
    u8  access;         // Access flags
    u8  granularity;    // Granularity and upper limit
    u8  base_high;      // Upper 8 bits of base
} __attribute__((packed));

/**
 * @brief GDT Descriptor (GDTR)
 */
struct GDTDescriptor {
    u16 limit;          // Size of GDT - 1
    u32 base;           // Base address of GDT
} __attribute__((packed));

/**
 * @brief GDT Access byte flags
 */
namespace GDTAccess {
    constexpr u8 PRESENT        = 0x80;  // Present bit
    constexpr u8 RING_0         = 0x00;  // Ring 0 (kernel)
    constexpr u8 RING_3         = 0x60;  // Ring 3 (user)
    constexpr u8 DESCRIPTOR     = 0x10;  // Descriptor type (1 = code/data)
    constexpr u8 EXECUTABLE     = 0x08;  // Executable (code segment)
    constexpr u8 DIRECTION      = 0x04;  // Direction/Conforming
    constexpr u8 READABLE       = 0x02;  // Readable (code) / Writable (data)
    constexpr u8 ACCESSED       = 0x01;  // Accessed bit
    
    // Common combinations
    constexpr u8 KERNEL_CODE    = PRESENT | RING_0 | DESCRIPTOR | EXECUTABLE | READABLE;
    constexpr u8 KERNEL_DATA    = PRESENT | RING_0 | DESCRIPTOR | READABLE;
    constexpr u8 USER_CODE      = PRESENT | RING_3 | DESCRIPTOR | EXECUTABLE | READABLE;
    constexpr u8 USER_DATA      = PRESENT | RING_3 | DESCRIPTOR | READABLE;
    constexpr u8 TSS_ACCESS     = PRESENT | RING_0 | 0x09;  // TSS (32-bit available)
}

/**
 * @brief GDT Granularity byte flags
 */
namespace GDTGranularity {
    constexpr u8 GRANULARITY_4K = 0x80;  // 4KB granularity
    constexpr u8 SIZE_32        = 0x40;  // 32-bit protected mode
    constexpr u8 LONG_MODE      = 0x20;  // Long mode (unused)
    constexpr u8 AVL            = 0x10;  // Available for system use
    
    // Common combination
    constexpr u8 STANDARD       = GRANULARITY_4K | SIZE_32;
}

/**
 * @brief GDT Segment Selectors
 */
namespace GDTSelector {
    constexpr u16 NULL_SEG      = 0x00;
    constexpr u16 KERNEL_CODE   = 0x08;
    constexpr u16 KERNEL_DATA   = 0x10;
    constexpr u16 USER_CODE     = 0x18 | 0x03;  // Ring 3
    constexpr u16 USER_DATA     = 0x20 | 0x03;  // Ring 3
    constexpr u16 TSS           = 0x28;
}

/**
 * @brief GDT Manager class
 * 
 * Manages the Global Descriptor Table for user mode support.
 * Extends the bootloader GDT with user segments and TSS.
 */
class GDTManager {
public:
    /**
     * @brief Initialize the GDT with user mode support
     */
    static void initialize();
    
    /**
     * @brief Set up a GDT entry
     */
    static void set_gdt_entry(u32 index, u32 base, u32 limit, u8 access, u8 granularity);
    
    /**
     * @brief Load the GDT
     */
    static void load_gdt();

private:
    static constexpr u32 GDT_ENTRIES = 6;  // null, kcode, kdata, ucode, udata, tss
    static GDTEntry gdt[GDT_ENTRIES];
    static GDTDescriptor gdt_descriptor;
};

} // namespace kira::system 