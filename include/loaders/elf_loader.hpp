#pragma once

#include "core/types.hpp"
#include "formats/elf.hpp"
#include "memory/virtual_memory.hpp"
#include "core/process.hpp"

namespace kira::loaders {

using namespace kira::system;
using namespace kira::formats;

/**
 * @brief ELF Loading Result
 * 
 * Contains information about the loaded ELF program.
 */
struct ElfLoadResult {
    bool success;               // True if loading succeeded
    u32 entry_point;            // Program entry point address
    u32 load_base;              // Base address where program was loaded
    u32 memory_size;            // Total memory used by program
    const char* error_message;  // Error description if failed
};

/**
 * @brief ELF Loader - loads ELF executables into virtual memory
 * 
 * Handles parsing ELF files, setting up virtual memory mappings,
 * and creating processes from ELF executables.
 */
class ElfLoader {
public:
    /**
     * @brief Load ELF executable into new address space
     * @param elf_data Pointer to ELF file data
     * @param elf_size Size of ELF file data
     * @param address_space Target address space
     * @return Load result with success status and details
     */
    static ElfLoadResult load_executable(const void* elf_data, u32 elf_size, 
                                       AddressSpace* address_space);
    
    /**
     * @brief Create process from ELF executable
     * @param elf_data Pointer to ELF file data
     * @param elf_size Size of ELF file data
     * @param process_name Name for the new process
     * @return Process ID or 0 if failed
     */
    static u32 create_process_from_elf(const void* elf_data, u32 elf_size, 
                                      const char* process_name);
    
    /**
     * @brief Load ELF segment into memory
     * @param elf_data ELF file data
     * @param program_header Program header describing segment
     * @param address_space Target address space
     * @return true on success
     */
    static bool load_segment(const void* elf_data, const Elf32_Phdr* program_header,
                           AddressSpace* address_space);
    
    /**
     * @brief Calculate total memory needed for ELF
     * @param elf_data ELF file data
     * @return Total memory size in bytes
     */
    static u32 calculate_memory_requirements(const void* elf_data);
    
    /**
     * @brief Validate ELF for loading
     * @param elf_data ELF file data
     * @param elf_size Size of ELF data
     * @return true if ELF is valid and loadable
     */
    static bool validate_elf_for_loading(const void* elf_data, u32 elf_size);
    
    /**
     * @brief Get ELF entry point
     * @param elf_data ELF file data
     * @return Entry point address or 0 if invalid
     */
    static u32 get_entry_point(const void* elf_data);
    
    /**
     * @brief Setup user stack for ELF process
     * @param address_space Target address space
     * @param stack_size Size of stack in bytes
     * @return Stack top address or 0 if failed
     */
    static u32 setup_user_stack(AddressSpace* address_space, u32 stack_size = 0x10000);

private:
    /**
     * @brief Allocate physical pages for segment
     * @param size Size in bytes
     * @return Physical address or 0 if failed
     */
    static u32 allocate_segment_memory(u32 size);
    
    /**
     * @brief Copy segment data from ELF to memory
     * @param elf_data ELF file data
     * @param program_header Program header
     * @param physical_addr Target physical address
     */
    static void copy_segment_data(const void* elf_data, const Elf32_Phdr* program_header,
                                u32 physical_addr);
    
    /**
     * @brief Get page permissions from ELF flags
     * @param elf_flags ELF program header flags
     * @param writable Output: true if writable
     * @param executable Output: true if executable
     */
    static void get_page_permissions(u32 elf_flags, bool& writable, bool& executable);
    
    /**
     * @brief Round up to page boundary
     * @param size Size to round up
     * @return Page-aligned size
     */
    static u32 align_to_page(u32 size) {
        return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    /**
     * @brief Round down to page boundary
     * @param addr Address to round down
     * @return Page-aligned address
     */
    static u32 align_to_page_down(u32 addr) {
        return addr & ~(PAGE_SIZE - 1);
    }
};

/**
 * @brief ELF Process - represents a process loaded from ELF
 * 
 * Extends the basic Process structure with ELF-specific information.
 */
struct ElfProcess {
    Process base_process;       // Base process structure
    AddressSpace* address_space; // Process address space
    u32 elf_entry_point;       // ELF entry point
    u32 elf_load_base;         // Base address where ELF was loaded
    u32 elf_memory_size;       // Total ELF memory usage
    
    /**
     * @brief Initialize ELF process
     * @param process_name Name of the process
     * @param entry_point ELF entry point
     * @param addr_space Process address space
     */
    void initialize(const char* process_name, u32 entry_point, AddressSpace* addr_space);
    
    /**
     * @brief Start ELF process execution
     */
    void start();
    
    /**
     * @brief Terminate ELF process
     */
    void terminate();
};

} // namespace kira::loaders 