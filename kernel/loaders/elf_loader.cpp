#include "loaders/elf_loader.hpp"
#include "memory/memory_manager.hpp"
#include "memory/virtual_memory.hpp"
#include "core/process.hpp"
#include "core/utils.hpp"

namespace kira::loaders {

using namespace kira::system::utils;

//=============================================================================
// ElfLoader Implementation
//=============================================================================

ElfLoadResult ElfLoader::load_executable(const void* elfData, u32 elfSize,
                                         AddressSpace* addressSpace) {
    ElfLoadResult result = {};
    result.success = false;
    result.errorMessage = "Unknown error";
    
    // Basic validation
    if (!elfData || !addressSpace || elfSize == 0) {
        result.errorMessage = "Invalid parameters";
        return result;
    }
    
    // Validate ELF format
    if (!validate_elf_for_loading(elfData, elfSize)) {
        result.errorMessage = "Invalid ELF format";
        return result;
    }
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elfData);
    
    // Set initial values
    result.entryPoint = header->e_entry;
    result.loadBase = 0xFFFFFFFF; // Will be set to lowest load address
    result.memorySize = 0;
    
    // Load all program segments
    for (u32 i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr* ph = ElfValidator::get_program_header(elfData, i);
        if (!ph || ph->p_type != PT_LOAD) continue;
        
        // Track memory usage
        if (ph->p_vaddr < result.loadBase) {
            result.loadBase = ph->p_vaddr;
        }
        
        u32 segmentEnd = ph->p_vaddr + ph->p_memsz;
        if (segmentEnd > result.memorySize) {
            result.memorySize = segmentEnd;
        }
        
        // Load the segment
        if (!load_segment(elfData, ph, addressSpace)) {
            result.errorMessage = "Failed to load segment";
            return result;
        }
    }
    
    // Calculate actual memory size used
    result.memorySize -= result.loadBase;
    
    result.success = true;
    result.errorMessage = nullptr;
    return result;
}

u32 ElfLoader::create_process_from_elf(const void* elfData, u32 elfSize,
                                      const char* processName) {
    // Create new address space
    AddressSpace* addressSpace = new AddressSpace(false); // false = user space
    if (!addressSpace) {
        return 0;
    }
    
    // Load ELF into address space
    ElfLoadResult loadResult = load_executable(elfData, elfSize, addressSpace);
    if (!loadResult.success) {
        delete addressSpace;
        return 0;
    }
    
    // Set up user stack
    u32 stackTop = setup_user_stack(addressSpace);
    if (stackTop == 0) {
        delete addressSpace;
        return 0;
    }
    
    // Create ELF process structure
    ElfProcess* elfProcess = new ElfProcess();
    elfProcess->initialize(processName, loadResult.entryPoint, addressSpace);
    
    // Start the process
    elfProcess->start();
    
    return elfProcess->baseProcess.pid;
}

bool ElfLoader::load_segment(const void* elfData, const Elf32_Phdr* programHeader,
                           AddressSpace* addressSpace) {
    if (!elfData || !programHeader || !addressSpace) {
        return false;
    }
    
    // Get segment properties
    u32 virtualAddr = programHeader->p_vaddr;
    u32 fileSize = programHeader->p_filesz;
    u32 memorySize = programHeader->p_memsz;
    u32 fileOffset = programHeader->p_offset;
    
    // Align to page boundaries
    u32 virtualStart = align_to_page_down(virtualAddr);
    u32 virtualEnd = align_to_page(virtualAddr + memorySize);
    u32 totalPages = (virtualEnd - virtualStart) / PAGE_SIZE;
    
    // Get page permissions
    bool writable, executable;
    get_page_permissions(programHeader->p_flags, writable, executable);
    
    // Allocate physical memory for the segment
    auto& memoryManager = MemoryManager::get_instance();
    
    for (u32 i = 0; i < totalPages; i++) {
        u32 pageVirtual = virtualStart + (i * PAGE_SIZE);
        
        // Allocate physical page
        void* physicalPage = memoryManager.allocate_physical_page();
        if (!physicalPage) {
            // Cleanup already allocated pages
            for (u32 j = 0; j < i; j++) {
                u32 cleanupVirtual = virtualStart + (j * PAGE_SIZE);
                u32 cleanupPhysical = addressSpace->get_physical_address(cleanupVirtual);
                if (cleanupPhysical != 0) {
                    addressSpace->unmap_page(cleanupVirtual);
                    memoryManager.free_physical_page(reinterpret_cast<void*>(cleanupPhysical));
                }
            }
            return false;
        }
        
        // Clear the page
        memset(physicalPage, 0, PAGE_SIZE);
        
        // Map virtual page to physical page
        if (!addressSpace->map_page(pageVirtual, reinterpret_cast<u32>(physicalPage), 
                                   writable, true)) {
            memoryManager.free_physical_page(physicalPage);
            return false;
        }
    }
    
    // Copy segment data from ELF file
    if (fileSize > 0) {
        const u8* source = static_cast<const u8*>(elfData) + fileOffset;
        
        // Copy data page by page
        for (u32 offset = 0; offset < fileSize; offset += PAGE_SIZE) {
            u32 copyVirtual = virtualAddr + offset;
            u32 copyPhysical = addressSpace->get_physical_address(copyVirtual);
            
            if (copyPhysical == 0) continue;
            
            u32 copySize = PAGE_SIZE;
            if (offset + copySize > fileSize) {
                copySize = fileSize - offset;
            }
            
            memcpy(reinterpret_cast<void*>(copyPhysical), source + offset, copySize);
        }
    }
    
    return true;
}

u32 ElfLoader::calculate_memory_requirements(const void* elfData) {
    if (!elfData) return 0;
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elfData);
    u32 minAddr = 0xFFFFFFFF;
    u32 maxAddr = 0;
    
    for (u32 i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr* ph = ElfValidator::get_program_header(elfData, i);
        if (!ph || ph->p_type != PT_LOAD) {
            continue;
        }
        
        if (ph->p_vaddr < minAddr) {
            minAddr = ph->p_vaddr;
        }
        
        u32 segmentEnd = ph->p_vaddr + ph->p_memsz;
        if (segmentEnd > maxAddr) {
            maxAddr = segmentEnd;
        }
    }
    
    if (minAddr == 0xFFFFFFFF) return 0;
    
    return maxAddr - minAddr;
}

bool ElfLoader::validate_elf_for_loading(const void* elfData, u32 elfSize) {
    // Use ElfValidator for basic validation
    if (!ElfValidator::is_valid_elf32(elfData, elfSize)) {
        return false;
    }
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elfData);
    
    // Check if it's executable
    if (!ElfValidator::is_executable_i386(header)) {
        return false;
    }
    
    // Check if we have loadable segments
    u32 loadableSegments = ElfValidator::count_loadable_segments(elfData);
    if (loadableSegments == 0) {
        return false;
    }
    
    // Additional validation could be added here
    return true;
}

u32 ElfLoader::get_entry_point(const void* elfData) {
    if (!elfData) return 0;
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elfData);
    return header->e_entry;
}

u32 ElfLoader::setup_user_stack(AddressSpace* addressSpace, u32 stackSize) {
    if (!addressSpace) return 0;
    
    // Calculate pages needed for stack
    u32 stackPages = align_to_page(stackSize) / PAGE_SIZE;
    u32 stackBottom = USER_STACK_TOP - (stackPages * PAGE_SIZE);
    
    auto& memoryManager = MemoryManager::get_instance();
    
    // Allocate and map stack pages
    for (u32 i = 0; i < stackPages; i++) {
        u32 stackPage = stackBottom + (i * PAGE_SIZE);
        
        // Allocate physical page
        void* physicalPage = memoryManager.allocate_physical_page();
        if (!physicalPage) {
            // Clean up previously allocated pages
            for (u32 j = 0; j < i; j++) {
                u32 cleanupPage = stackBottom + (j * PAGE_SIZE);
                u32 cleanupPhysical = addressSpace->get_physical_address(cleanupPage);
                if (cleanupPhysical) {
                    memoryManager.free_physical_page(reinterpret_cast<void*>(cleanupPhysical));
                }
                addressSpace->unmap_page(cleanupPage);
            }
            return 0;
        }
        
        // Map page as user-writable
        if (!addressSpace->map_page(stackPage, reinterpret_cast<u32>(physicalPage),
                                   true, true)) {
            memoryManager.free_physical_page(physicalPage);
            return 0;
        }
    }
    
    return USER_STACK_TOP;
}

u32 ElfLoader::allocate_segment_memory(u32 size) {
    auto& memoryManager = MemoryManager::get_instance();
    u32 pagesNeeded = align_to_page(size) / PAGE_SIZE;
    
    // For simplicity, just allocate the first page
    // In a real implementation, we'd allocate all needed pages
    void* firstPage = memoryManager.allocate_physical_page();
    return reinterpret_cast<u32>(firstPage);
}

void ElfLoader::copy_segment_data(const void* elfData, const Elf32_Phdr* programHeader,
                                u32 physicalAddr) {
    if (!elfData || !programHeader || physicalAddr == 0) {
        return;
    }
    
    const u8* source = static_cast<const u8*>(elfData) + programHeader->p_offset;
    u8* destination = reinterpret_cast<u8*>(physicalAddr);
    
    // Copy file data
    memcpy(destination, source, programHeader->p_filesz);
    
    // Zero out BSS section (if memory size > file size)
    if (programHeader->p_memsz > programHeader->p_filesz) {
        u32 bssSize = programHeader->p_memsz - programHeader->p_filesz;
        memset(destination + programHeader->p_filesz, 0, bssSize);
    }
}

void ElfLoader::get_page_permissions(u32 elfFlags, bool& writable, bool& executable) {
    writable = (elfFlags & PF_W) != 0;
    executable = (elfFlags & PF_X) != 0;
    
    // Always allow read access for user pages
    // Note: x86 doesn't have separate execute permission in page tables
    // Execute permission is handled by segment descriptors
}

//=============================================================================
// ElfProcess Implementation
//=============================================================================

void ElfProcess::initialize(const char* processName, u32 entryPoint, AddressSpace* addrSpace) {
    // Initialize base process
    baseProcess.pid = 0; // Will be assigned by ProcessManager
    const char* src = processName ? processName : "elf_process";
    u32 maxLen = sizeof(baseProcess.name) - 1;
    
    strcpy_s(baseProcess.name, src, maxLen);
    
    baseProcess.state = ProcessState::READY;
    baseProcess.priority = 5;
    
    // Initialize ELF-specific data
    addressSpace = addrSpace;
    elfEntryPoint = entryPoint;
    elfLoadBase = 0;
    elfMemorySize = 0;
    
    // Validate initialization
    if (!addressSpace || elfEntryPoint == 0) {
        baseProcess.state = ProcessState::TERMINATED;
    }
}

void ElfProcess::start() {
    if (!addressSpace || elfEntryPoint == 0) {
        return;
    }
    
    // Switch to process address space
    auto& vmManager = VirtualMemoryManager::get_instance();
    vmManager.switch_address_space(addressSpace);
    
    // TODO: Jump to ELF entry point in user mode
    // This would involve setting up user mode context and jumping to elfEntryPoint
    // For now, this is a placeholder
}

void ElfProcess::terminate() {
    if (addressSpace) {
        delete addressSpace;
        addressSpace = nullptr;
    }
    
    baseProcess.state = ProcessState::TERMINATED;
}

} // namespace kira::loaders 