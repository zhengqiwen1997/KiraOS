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
    bool success;                    // Whether loading succeeded
    u32 entryPoint;                  // Program entry point address
    u32 loadBase;                    // Base address where program was loaded
    u32 memorySize;                  // Total memory used by program
    const char* errorMessage;       // Error description if failed
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
     * @brief Load an ELF executable into memory
     * @param elfData Pointer to ELF file data
     * @param elfSize Size of ELF data
     * @param addressSpace Target address space
     * @return Load result with entry point and memory info
     */
    static ElfLoadResult load_executable(const void* elfData, u32 elfSize,
                                       AddressSpace* addressSpace);
    
    /**
     * @brief Create a process from ELF data
     * @param elfData Pointer to ELF file data  
     * @param elfSize Size of ELF data
     * @param processName Name for the new process
     * @return Process ID of created process, or 0 on failure
     */
    static u32 create_process_from_elf(const void* elfData, u32 elfSize,
                                     const char* processName);
    
    /**
     * @brief Load ELF segment into memory
     * @param elfData ELF file data
     * @param programHeader Program header describing segment
     * @param addressSpace Target address space
     * @return true on success
     */
    static bool load_segment(const void* elfData, const Elf32_Phdr* programHeader,
                           AddressSpace* addressSpace);
    
    /**
     * @brief Calculate total memory needed for ELF
     * @param elfData ELF file data
     * @return Total memory size in bytes
     */
    static u32 calculate_memory_requirements(const void* elfData);
    
    /**
     * @brief Validate ELF file for loading
     * @param elfData Pointer to ELF data
     * @param elfSize Size of ELF data
     * @return true if valid for loading
     */
    static bool validate_elf_for_loading(const void* elfData, u32 elfSize);
    
    /**
     * @brief Get ELF entry point
     * @param elfData ELF file data
     * @return Entry point address or 0 if invalid
     */
    static u32 get_entry_point(const void* elfData);
    
    /**
     * @brief Set up user stack for process
     * @param addressSpace Target address space
     * @param stackSize Size of stack in bytes
     * @return Stack top address
     */
    static u32 setup_user_stack(AddressSpace* addressSpace, u32 stackSize = 0x10000);

private:
    /**
     * @brief Allocate physical pages for segment
     * @param size Size in bytes
     * @return Physical address or 0 if failed
     */
    static u32 allocate_segment_memory(u32 size);
    
    /**
     * @brief Copy segment data from ELF to memory
     * @param elfData ELF file data
     * @param programHeader Program header
     * @param physicalAddr Target physical address
     */
    static void copy_segment_data(const void* elfData, const Elf32_Phdr* programHeader,
                                u32 physicalAddr);
    
    /**
     * @brief Get page permissions from ELF flags
     * @param elfFlags ELF program header flags
     * @param writable Output: true if writable
     * @param executable Output: true if executable
     */
    static void get_page_permissions(u32 elfFlags, bool& writable, bool& executable);
    
    /**
     * @brief Round up to page boundary
     * @param size Size to round up
     * @return Page-aligned size
     */
    static u32 align_to_page(u32 size) {
        return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    /**
     * @brief Align address down to page boundary
     * @param addr Address to align
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
    Process baseProcess;           // Base process structure
    AddressSpace* addressSpace;      // Process address space
    u32 elfEntryPoint;              // ELF entry point
    u32 elfLoadBase;                // Base address where ELF was loaded
    u32 elfMemorySize;              // Total ELF memory usage
    
    /**
     * @brief Initialize ELF process
     * @param processName Name of process
     * @param entryPoint ELF entry point
     * @param addrSpace Address space
     */
    void initialize(const char* processName, u32 entryPoint, AddressSpace* addrSpace);
    
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