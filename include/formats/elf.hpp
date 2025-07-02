#pragma once

#include "core/types.hpp"

namespace kira::formats {

using namespace kira::system;

// ELF magic number
constexpr u32 ELF_MAGIC = 0x464C457F; // "\x7FELF"

// ELF file classes
constexpr u8 ELF_CLASS_32 = 1;  // 32-bit
constexpr u8 ELF_CLASS_64 = 2;  // 64-bit

// ELF data encodings
constexpr u8 ELF_DATA_LSB = 1;  // Little endian
constexpr u8 ELF_DATA_MSB = 2;  // Big endian

// ELF file types
constexpr u16 ELF_TYPE_NONE = 0;    // No file type
constexpr u16 ELF_TYPE_REL = 1;     // Relocatable file
constexpr u16 ELF_TYPE_EXEC = 2;    // Executable file
constexpr u16 ELF_TYPE_DYN = 3;     // Shared object file
constexpr u16 ELF_TYPE_CORE = 4;    // Core file

// ELF machine types
constexpr u16 ELF_MACHINE_NONE = 0;   // No machine
constexpr u16 ELF_MACHINE_386 = 3;    // Intel 80386

// ELF versions
constexpr u32 ELF_VERSION_CURRENT = 1;

// Program header types
constexpr u32 PT_NULL = 0;      // Unused entry
constexpr u32 PT_LOAD = 1;      // Loadable segment
constexpr u32 PT_DYNAMIC = 2;   // Dynamic linking info
constexpr u32 PT_INTERP = 3;    // Interpreter path
constexpr u32 PT_NOTE = 4;      // Note section
constexpr u32 PT_SHLIB = 5;     // Reserved
constexpr u32 PT_PHDR = 6;      // Program header table

// Program header flags
constexpr u32 PF_X = 1;         // Execute
constexpr u32 PF_W = 2;         // Write  
constexpr u32 PF_R = 4;         // Read

// Section header types
constexpr u32 SHT_NULL = 0;         // Unused
constexpr u32 SHT_PROGBITS = 1;     // Program data
constexpr u32 SHT_SYMTAB = 2;       // Symbol table
constexpr u32 SHT_STRTAB = 3;       // String table
constexpr u32 SHT_RELA = 4;         // Relocation entries with addends
constexpr u32 SHT_HASH = 5;         // Symbol hash table
constexpr u32 SHT_DYNAMIC = 6;      // Dynamic linking info
constexpr u32 SHT_NOTE = 7;         // Notes
constexpr u32 SHT_NOBITS = 8;       // BSS section
constexpr u32 SHT_REL = 9;          // Relocation entries
constexpr u32 SHT_SHLIB = 10;       // Reserved
constexpr u32 SHT_DYNSYM = 11;      // Dynamic symbol table

/**
 * @brief ELF32 Header Structure
 * 
 * The main header at the beginning of every ELF file.
 * Contains metadata about the file format and layout.
 */
struct Elf32_Ehdr {
    u8  e_ident[16];        // ELF identification
    u16 e_type;             // Object file type
    u16 e_machine;          // Architecture
    u32 e_version;          // Object file version
    u32 e_entry;            // Entry point virtual address
    u32 e_phoff;            // Program header table file offset
    u32 e_shoff;            // Section header table file offset
    u32 e_flags;            // Processor-specific flags
    u16 e_ehsize;           // ELF header size in bytes
    u16 e_phentsize;        // Program header table entry size
    u16 e_phnum;            // Program header table entry count
    u16 e_shentsize;        // Section header table entry size
    u16 e_shnum;            // Section header table entry count
    u16 e_shstrndx;         // Section header string table index
} __attribute__((packed));

/**
 * @brief ELF32 Program Header Structure
 * 
 * Describes a segment or other information needed for execution.
 * Used to create the process image in memory.
 */
struct Elf32_Phdr {
    u32 p_type;             // Segment type
    u32 p_offset;           // Segment file offset
    u32 p_vaddr;            // Segment virtual address
    u32 p_paddr;            // Segment physical address (ignored)
    u32 p_filesz;           // Segment size in file
    u32 p_memsz;            // Segment size in memory
    u32 p_flags;            // Segment flags
    u32 p_align;            // Segment alignment
} __attribute__((packed));

/**
 * @brief ELF32 Section Header Structure
 * 
 * Describes sections within the ELF file.
 * Mainly used for linking and debugging.
 */
struct Elf32_Shdr {
    u32 sh_name;            // Section name (string table offset)
    u32 sh_type;            // Section type
    u32 sh_flags;           // Section flags
    u32 sh_addr;            // Section virtual address at execution
    u32 sh_offset;          // Section file offset
    u32 sh_size;            // Section size in bytes
    u32 sh_link;            // Link to another section
    u32 sh_info;            // Additional section information
    u32 sh_addralign;       // Section alignment
    u32 sh_entsize;         // Entry size if section holds table
} __attribute__((packed));

/**
 * @brief ELF Validation and Utility Functions
 */
class ElfValidator {
public:
    /**
     * @brief Check if data contains a valid ELF32 header
     * @param data Pointer to potential ELF data
     * @param size Size of data buffer
     * @return true if valid ELF32 file
     */
    static bool is_valid_elf32(const void* data, u32 size);
    
    /**
     * @brief Check if ELF is executable for i386
     * @param header ELF header to check
     * @return true if executable i386 ELF
     */
    static bool is_executable_i386(const Elf32_Ehdr* header);
    
    /**
     * @brief Get program header by index
     * @param elfData ELF file data
     * @param index Program header index
     * @return Pointer to program header or nullptr
     */
    static const Elf32_Phdr* get_program_header(const void* elfData, u32 index);
    
    /**
     * @brief Count loadable segments in ELF
     * @param elfData ELF file data
     * @return Number of PT_LOAD segments
     */
    static u32 count_loadable_segments(const void* elfData);
};

} // namespace kira::formats 