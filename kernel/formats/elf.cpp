#include "formats/elf.hpp"

namespace kira::formats {

bool ElfValidator::is_valid_elf32(const void* data, u32 size) {
    if (!data || size < sizeof(Elf32_Ehdr)) {
        return false;
    }
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(data);
    
    // Check ELF magic number
    if (*reinterpret_cast<const u32*>(header->e_ident) != ELF_MAGIC) {
        return false;
    }
    
    // Check for 32-bit ELF
    if (header->e_ident[4] != ELF_CLASS_32) {
        return false;
    }
    
    // Check for little endian
    if (header->e_ident[5] != ELF_DATA_LSB) {
        return false;
    }
    
    // Check ELF version
    if (header->e_version != ELF_VERSION_CURRENT) {
        return false;
    }
    
    // Check header size
    if (header->e_ehsize != sizeof(Elf32_Ehdr)) {
        return false;
    }
    
    // Check program header size
    if (header->e_phentsize != sizeof(Elf32_Phdr)) {
        return false;
    }
    
    // Validate program header table is within file
    if (header->e_phoff != 0) {
        u32 phTableEnd = header->e_phoff + (header->e_phnum * header->e_phentsize);
        if (phTableEnd > size) {
            return false;
        }
    }
    
    return true;
}

bool ElfValidator::is_executable_i386(const Elf32_Ehdr* header) {
    if (!header) {
        return false;
    }
    
    // Check if it's an executable
    if (header->e_type != ELF_TYPE_EXEC) {
        return false;
    }
    
    // Check if it's for i386 architecture
    if (header->e_machine != ELF_MACHINE_386) {
        return false;
    }
    
    // Check if it has an entry point
    if (header->e_entry == 0) {
        return false;
    }
    
    return true;
}

const Elf32_Phdr* ElfValidator::get_program_header(const void* elfData, u32 index) {
    if (!elfData) {
        return nullptr;
    }
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elfData);
    
    if (index >= header->e_phnum) {
        return nullptr;
    }
    
    if (header->e_phoff == 0) {
        return nullptr;
    }
    
    const u8* phTable = static_cast<const u8*>(elfData) + header->e_phoff;
    const Elf32_Phdr* ph = reinterpret_cast<const Elf32_Phdr*>(
        phTable + (index * header->e_phentsize)
    );
    
    return ph;
}

u32 ElfValidator::count_loadable_segments(const void* elfData) {
    if (!elfData) {
        return 0;
    }
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elfData);
    u32 count = 0;
    
    for (u32 i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr* ph = get_program_header(elfData, i);
        if (ph && ph->p_type == PT_LOAD) {
            count++;
        }
    }
    
    return count;
}

} // namespace kira::formats 