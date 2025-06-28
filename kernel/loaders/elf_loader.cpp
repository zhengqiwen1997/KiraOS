#include "loaders/elf_loader.hpp"
#include "memory/memory_manager.hpp"
#include "memory/virtual_memory.hpp"
#include "core/process.hpp"
#include "display/vga.hpp"
#include "core/utils.hpp"

namespace kira::loaders {

using namespace kira::system::utils;

//=============================================================================
// ElfLoader Implementation
//=============================================================================

ElfLoadResult ElfLoader::load_executable(const void* elf_data, u32 elf_size, 
                                        AddressSpace* address_space) {
    ElfLoadResult result = {};
    result.success = false;
    result.error_message = "Unknown error";
    
    // Validate inputs
    if (!elf_data || !address_space || elf_size == 0) {
        result.error_message = "Invalid parameters";
        return result;
    }
    
    // Validate ELF file
    if (!validate_elf_for_loading(elf_data, elf_size)) {
        result.error_message = "Invalid ELF file";
        return result;
    }
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elf_data);
    
    // Get entry point
    result.entry_point = header->e_entry;
    result.load_base = 0xFFFFFFFF; // Will be set to lowest load address
    result.memory_size = 0;
    
    // Load all PT_LOAD segments
    for (u32 i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr* ph = ElfValidator::get_program_header(elf_data, i);
        if (!ph || ph->p_type != PT_LOAD) {
            continue;
        }
        
        // Update load base to lowest address
        if (ph->p_vaddr < result.load_base) {
            result.load_base = ph->p_vaddr;
        }
        
        // Update total memory size
        u32 segment_end = ph->p_vaddr + ph->p_memsz;
        if (segment_end > result.memory_size) {
            result.memory_size = segment_end;
        }
        
        // Load this segment
        if (!load_segment(elf_data, ph, address_space)) {
            result.error_message = "Failed to load segment";
            return result;
        }
    }
    
    // Calculate actual memory size
    result.memory_size -= result.load_base;
    
    result.success = true;
    result.error_message = "Success";
    return result;
}

u32 ElfLoader::create_process_from_elf(const void* elf_data, u32 elf_size, 
                                      const char* process_name) {
    // Create new address space
    auto& vm_manager = VirtualMemoryManager::get_instance();
    AddressSpace* address_space = vm_manager.create_user_address_space();
    
    if (!address_space) {
        return 0;
    }
    
    // Load ELF into address space
    ElfLoadResult load_result = load_executable(elf_data, elf_size, address_space);
    if (!load_result.success) {
        delete address_space;
        return 0;
    }
    
    // Setup user stack
    u32 stack_top = setup_user_stack(address_space);
    if (stack_top == 0) {
        delete address_space;
        return 0;
    }
    
    // Create process using ProcessManager
    auto& process_manager = ProcessManager::get_instance();
    
    // For now, we'll create a wrapper function that switches to the ELF entry point
    // In a full implementation, this would be handled by the scheduler
    
    // TODO: Integrate with ProcessManager to create ELF-based processes
    // This is a placeholder that demonstrates the concept
    
    return 1; // Placeholder PID
}

bool ElfLoader::load_segment(const void* elf_data, const Elf32_Phdr* program_header,
                           AddressSpace* address_space) {
    if (!elf_data || !program_header || !address_space) {
        return false;
    }
    
    // Get segment properties
    u32 virtual_addr = program_header->p_vaddr;
    u32 file_size = program_header->p_filesz;
    u32 memory_size = program_header->p_memsz;
    u32 file_offset = program_header->p_offset;
    
    // Align to page boundaries
    u32 virtual_start = align_to_page_down(virtual_addr);
    u32 virtual_end = align_to_page(virtual_addr + memory_size);
    u32 total_pages = (virtual_end - virtual_start) / PAGE_SIZE;
    
    // Get page permissions
    bool writable, executable;
    get_page_permissions(program_header->p_flags, writable, executable);
    
    // Allocate physical memory for the segment
    auto& memory_manager = MemoryManager::get_instance();
    
    for (u32 i = 0; i < total_pages; i++) {
        u32 page_virtual = virtual_start + (i * PAGE_SIZE);
        
        // Allocate physical page
        void* physical_page = memory_manager.allocate_physical_page();
        if (!physical_page) {
            // Cleanup already allocated pages
            for (u32 j = 0; j < i; j++) {
                u32 cleanup_virtual = virtual_start + (j * PAGE_SIZE);
                u32 cleanup_physical = address_space->get_physical_address(cleanup_virtual);
                if (cleanup_physical != 0) {
                    address_space->unmap_page(cleanup_virtual);
                    memory_manager.free_physical_page(reinterpret_cast<void*>(cleanup_physical));
                }
            }
            return false;
        }
        
        // Clear the page
        memset(physical_page, 0, PAGE_SIZE);
        
        // Map virtual page to physical page
        if (!address_space->map_page(page_virtual, reinterpret_cast<u32>(physical_page), 
                                   writable, true)) {
            memory_manager.free_physical_page(physical_page);
            return false;
        }
    }
    
    // Copy segment data from ELF file
    if (file_size > 0) {
        const u8* source = static_cast<const u8*>(elf_data) + file_offset;
        
        // Copy data page by page
        for (u32 offset = 0; offset < file_size; offset += PAGE_SIZE) {
            u32 copy_virtual = virtual_addr + offset;
            u32 copy_physical = address_space->get_physical_address(copy_virtual);
            
            if (copy_physical == 0) continue;
            
            u32 copy_size = PAGE_SIZE;
            if (offset + copy_size > file_size) {
                copy_size = file_size - offset;
            }
            
            memcpy(reinterpret_cast<void*>(copy_physical), source + offset, copy_size);
        }
    }
    
    return true;
}

u32 ElfLoader::calculate_memory_requirements(const void* elf_data) {
    if (!elf_data) return 0;
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elf_data);
    u32 min_addr = 0xFFFFFFFF;
    u32 max_addr = 0;
    
    for (u32 i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr* ph = ElfValidator::get_program_header(elf_data, i);
        if (!ph || ph->p_type != PT_LOAD) {
            continue;
        }
        
        if (ph->p_vaddr < min_addr) {
            min_addr = ph->p_vaddr;
        }
        
        u32 segment_end = ph->p_vaddr + ph->p_memsz;
        if (segment_end > max_addr) {
            max_addr = segment_end;
        }
    }
    
    if (min_addr == 0xFFFFFFFF) return 0;
    
    return max_addr - min_addr;
}

bool ElfLoader::validate_elf_for_loading(const void* elf_data, u32 elf_size) {
    // Basic ELF validation
    if (!ElfValidator::is_valid_elf32(elf_data, elf_size)) {
        return false;
    }
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elf_data);
    
    // Check if it's executable for i386
    if (!ElfValidator::is_executable_i386(header)) {
        return false;
    }
    
    // Check if it has loadable segments
    if (ElfValidator::count_loadable_segments(elf_data) == 0) {
        return false;
    }
    
    // Validate entry point is in user space
    if (header->e_entry >= KERNEL_SPACE_START) {
        return false;
    }
    
    return true;
}

u32 ElfLoader::get_entry_point(const void* elf_data) {
    if (!elf_data) return 0;
    
    const Elf32_Ehdr* header = static_cast<const Elf32_Ehdr*>(elf_data);
    return header->e_entry;
}

u32 ElfLoader::setup_user_stack(AddressSpace* address_space, u32 stack_size) {
    if (!address_space) return 0;
    
    // Allocate stack pages
    u32 stack_pages = align_to_page(stack_size) / PAGE_SIZE;
    u32 stack_bottom = USER_STACK_TOP - (stack_pages * PAGE_SIZE);
    
    auto& memory_manager = MemoryManager::get_instance();
    
    for (u32 i = 0; i < stack_pages; i++) {
        u32 stack_page = stack_bottom + (i * PAGE_SIZE);
        
        // Allocate physical page
        void* physical_page = memory_manager.allocate_physical_page();
        if (!physical_page) {
            // Cleanup on failure
            for (u32 j = 0; j < i; j++) {
                u32 cleanup_page = stack_bottom + (j * PAGE_SIZE);
                u32 cleanup_physical = address_space->get_physical_address(cleanup_page);
                if (cleanup_physical != 0) {
                    address_space->unmap_page(cleanup_page);
                    memory_manager.free_physical_page(reinterpret_cast<void*>(cleanup_physical));
                }
            }
            return 0;
        }
        
        // Clear stack page
        memset(physical_page, 0, PAGE_SIZE);
        
        // Map stack page (writable, user-accessible)
        if (!address_space->map_page(stack_page, reinterpret_cast<u32>(physical_page), 
                                   true, true)) {
            memory_manager.free_physical_page(physical_page);
            return 0;
        }
    }
    
    return USER_STACK_TOP; // Return stack top
}

u32 ElfLoader::allocate_segment_memory(u32 size) {
    auto& memory_manager = MemoryManager::get_instance();
    u32 pages_needed = align_to_page(size) / PAGE_SIZE;
    
    // For simplicity, allocate first page and return its address
    // In a full implementation, this would allocate contiguous physical memory
    void* first_page = memory_manager.allocate_physical_page();
    return reinterpret_cast<u32>(first_page);
}

void ElfLoader::copy_segment_data(const void* elf_data, const Elf32_Phdr* program_header,
                                u32 physical_addr) {
    if (!elf_data || !program_header || physical_addr == 0) {
        return;
    }
    
    const u8* source = static_cast<const u8*>(elf_data) + program_header->p_offset;
    u8* destination = reinterpret_cast<u8*>(physical_addr);
    
    // Copy file data
    memcpy(destination, source, program_header->p_filesz);
    
    // Zero out BSS section (if memory size > file size)
    if (program_header->p_memsz > program_header->p_filesz) {
        u32 bss_size = program_header->p_memsz - program_header->p_filesz;
        memset(destination + program_header->p_filesz, 0, bss_size);
    }
}

void ElfLoader::get_page_permissions(u32 elf_flags, bool& writable, bool& executable) {
    writable = (elf_flags & PF_W) != 0;
    executable = (elf_flags & PF_X) != 0;
    
    // Always allow read access for user pages
    // Note: x86 doesn't have separate execute permission in page tables
    // Execute permission is handled by segment descriptors
}

//=============================================================================
// ElfProcess Implementation
//=============================================================================

void ElfProcess::initialize(const char* process_name, u32 entry_point, AddressSpace* addr_space) {
    // Initialize base process
    base_process.pid = 0; // Will be set by ProcessManager
    
    // Copy process name safely
    const char* src = process_name ? process_name : "elf_process";
    char* dst = base_process.name;
    u32 max_len = sizeof(base_process.name) - 1;
    
    u32 i = 0;
    while (i < max_len && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    
    base_process.state = ProcessState::READY;
    base_process.priority = 5;
    base_process.time_slice = 10;
    base_process.time_used = 0;
    
    base_process.is_user_mode = true;
    base_process.has_started = false;
    
    // Set ELF-specific fields
    address_space = addr_space;
    elf_entry_point = entry_point;
    elf_load_base = 0;
    elf_memory_size = 0;
}

void ElfProcess::start() {
    if (!address_space || elf_entry_point == 0) {
        return;
    }
    
    // Switch to process address space
    auto& vm_manager = VirtualMemoryManager::get_instance();
    vm_manager.switch_address_space(address_space);
    
    // TODO: Jump to ELF entry point in user mode
    // This would involve setting up user mode context and jumping to elf_entry_point
    // For now, this is a placeholder
}

void ElfProcess::terminate() {
    if (address_space) {
        delete address_space;
        address_space = nullptr;
    }
    
    base_process.state = ProcessState::TERMINATED;
}

} // namespace kira::loaders 