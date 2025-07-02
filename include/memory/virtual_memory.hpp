#pragma once

#include "core/types.hpp"
#include "memory/memory.hpp"

namespace kira::system {

// Virtual memory constants
constexpr u32 PAGE_TABLE_ENTRIES = 1024;        // 1024 entries per page table
constexpr u32 PAGE_DIRECTORY_ENTRIES = 1024;    // 1024 entries per page directory
constexpr u64 VIRTUAL_MEMORY_SIZE = 0x100000000ULL; // 4GB virtual address space

// Standard virtual memory layout
constexpr u32 USER_SPACE_START = 0x00000000;    // User space starts at 0
constexpr u32 USER_SPACE_END = 0xBFFFFFFF;      // User space ends at 3GB
constexpr u32 KERNEL_SPACE_START = 0xC0000000;  // Kernel space starts at 3GB
constexpr u32 KERNEL_SPACE_END = 0xFFFFFFFF;    // Kernel space ends at 4GB

// Common virtual addresses
constexpr u32 USER_STACK_TOP = 0xC0000000;      // User stack grows down from 3GB
constexpr u32 USER_HEAP_START = 0x40000000;     // User heap starts at 1GB
constexpr u32 USER_TEXT_START = 0x08048000;     // Standard ELF text start

/**
 * @brief Address Space - represents a virtual memory address space
 * 
 * Each process has its own address space with separate page directory.
 * Provides isolation between processes and kernel.
 */
class AddressSpace {
private:
    u32* pageDirectory;        // Physical address of page directory
    u32 pageDirectoryPhys;    // Physical address (for CR3)
    bool isKernelSpace;       // True if this is kernel address space
    
public:
    /**
     * @brief Create new address space
     * @param kernelSpace True to create kernel address space
     */
    AddressSpace(bool kernelSpace = false);
    
    /**
     * @brief Destructor - cleans up page tables
     */
    ~AddressSpace();
    
    /**
     * @brief Map virtual page to physical page
     * @param virtualAddr Virtual address (page-aligned)
     * @param physicalAddr Physical address (page-aligned)  
     * @param writable True if page should be writable
     * @param user True if page should be user-accessible
     * @return true on success
     */
    bool map_page(u32 virtualAddr, u32 physicalAddr, bool writable = true, bool user = true);
    
    /**
     * @brief Unmap virtual page
     * @param virtualAddr Virtual address to unmap
     * @return true on success
     */
    bool unmap_page(u32 virtualAddr);
    
    /**
     * @brief Get physical address for virtual address
     * @param virtualAddr Virtual address to translate
     * @return Physical address or 0 if not mapped
     */
    u32 get_physical_address(u32 virtualAddr) const;
    
    /**
     * @brief Check if virtual address is mapped
     * @param virtualAddr Virtual address to check
     * @return true if mapped
     */
    bool is_mapped(u32 virtualAddr) const;
    
    /**
     * @brief Switch to this address space
     */
    void switch_to() const;
    
    /**
     * @brief Get page directory physical address
     * @return Physical address of page directory
     */
    u32 get_page_directory_phys() const { return pageDirectoryPhys; }
    
    /**
     * @brief Map multiple pages for a memory region
     * @param virtualStart Virtual start address
     * @param physicalStart Physical start address
     * @param size Size in bytes
     * @param writable True if pages should be writable
     * @param user True if pages should be user-accessible
     * @return true on success
     */
    bool map_region(u32 virtualStart, u32 physicalStart, u32 size, 
                   bool writable = true, bool user = true);
    
    /**
     * @brief Unmap multiple pages for a memory region
     * @param virtualStart Virtual start address
     * @param size Size in bytes
     * @return true on success
     */
    bool unmap_region(u32 virtualStart, u32 size);

private:
    /**
     * @brief Get or create page table for virtual address
     * @param virtualAddr Virtual address
     * @param create True to create if doesn't exist
     * @return Pointer to page table or nullptr
     */
    PageTableEntry* get_page_table(u32 virtualAddr, bool create = false);
    
    /**
     * @brief Create new page table
     * @return Physical address of new page table
     */
    u32 create_page_table();
    
    /**
     * @brief Setup kernel mappings (identity map kernel space)
     */
    void setup_kernel_mappings();
};

/**
 * @brief Virtual Memory Manager - manages virtual memory system
 * 
 * Handles page directory switching, TLB flushing, and provides
 * high-level virtual memory operations.
 */
class VirtualMemoryManager {
private:
    static VirtualMemoryManager* instance;
    AddressSpace* currentAddressSpace;
    AddressSpace* kernelAddressSpace;
    
public:
    /**
     * @brief Get singleton instance
     */
    static VirtualMemoryManager& get_instance();
    
    /**
     * @brief Initialize virtual memory system
     */
    void initialize();
    
    /**
     * @brief Create new user address space
     * @return Pointer to new address space
     */
    AddressSpace* create_user_address_space();
    
    /**
     * @brief Switch to address space
     * @param addressSpace Address space to switch to
     */
    void switch_address_space(AddressSpace* addressSpace);
    
    /**
     * @brief Get current address space
     */
    AddressSpace* get_current_address_space() const { return currentAddressSpace; }
    
    /**
     * @brief Get kernel address space
     */
    AddressSpace* get_kernel_address_space() const { return kernelAddressSpace; }
    
    /**
     * @brief Flush TLB (Translation Lookaside Buffer)
     */
    static void flush_tlb();
    
    /**
     * @brief Flush single TLB entry
     * @param virtualAddr Virtual address to flush
     */
    static void flush_tlb_single(u32 virtualAddr);
    
    /**
     * @brief Enable paging
     */
    static void enable_paging();
    
    /**
     * @brief Check if paging is enabled
     */
    static bool is_paging_enabled();

private:
    VirtualMemoryManager() = default;
    ~VirtualMemoryManager() = default;
    VirtualMemoryManager(const VirtualMemoryManager&) = delete;
    VirtualMemoryManager& operator=(const VirtualMemoryManager&) = delete;
};

} // namespace kira::system 