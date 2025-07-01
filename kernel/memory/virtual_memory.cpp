#include "memory/virtual_memory.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"

namespace kira::system {

// Static member definitions
VirtualMemoryManager* VirtualMemoryManager::instance = nullptr;

// Assembly functions for low-level paging operations
extern "C" {
    void load_page_directory(kira::system::u32 page_dir_phys);
    void enable_paging_asm();
    kira::system::u32 get_cr0();
    void flush_tlb_asm();
    void invlpg(kira::system::u32 virtual_addr);
}

//=============================================================================
// AddressSpace Implementation
//=============================================================================

AddressSpace::AddressSpace(bool kernel_space) : is_kernel_space(kernel_space) {
    // Allocate page directory
    auto& memory_manager = MemoryManager::get_instance();
    void* page_dir_mem = memory_manager.allocate_physical_page();
    
    if (!page_dir_mem) {
        page_directory = nullptr;
        page_directory_phys = 0;
        return;
    }
    
    page_directory_phys = reinterpret_cast<u32>(page_dir_mem);
    page_directory = reinterpret_cast<u32*>(page_dir_mem);
    
    // Clear page directory
    for (u32 i = 0; i < PAGE_DIRECTORY_ENTRIES; i++) {
        page_directory[i] = 0;
    }
    
    if (is_kernel_space) {
        setup_kernel_mappings();
    }
}

AddressSpace::~AddressSpace() {
    if (!page_directory) return;
    
    auto& memory_manager = MemoryManager::get_instance();
    
    // Free all page tables
    for (u32 i = 0; i < PAGE_DIRECTORY_ENTRIES; i++) {
        PageDirectoryEntry pde;
        pde.value = page_directory[i];
        
        if (pde.is_present()) {
            // Free the page table
            memory_manager.free_physical_page(reinterpret_cast<void*>(pde.get_address()));
        }
    }
    
    // Free page directory
    memory_manager.free_physical_page(reinterpret_cast<void*>(page_directory_phys));
}

bool AddressSpace::map_page(u32 virtual_addr, u32 physical_addr, bool writable, bool user) {
    if (!page_directory) return false;
    
    // Align addresses to page boundaries
    virtual_addr &= PAGE_MASK;
    physical_addr &= PAGE_MASK;
    
    // Get page table
    PageTableEntry* page_table = get_page_table(virtual_addr, true);
    if (!page_table) return false;
    
    // Calculate page table index
    u32 pt_index = (virtual_addr >> 12) & 0x3FF;
    
    // Set up page table entry
    PageTableEntry& pte = page_table[pt_index];
    pte.value = 0;
    pte.set_address(physical_addr);
    pte.set_present(true);
    pte.set_writable(writable);
    pte.set_user(user);
    
    // Flush TLB for this address
    VirtualMemoryManager::flush_tlb_single(virtual_addr);
    
    return true;
}

bool AddressSpace::unmap_page(u32 virtual_addr) {
    if (!page_directory) return false;
    
    virtual_addr &= PAGE_MASK;
    
    PageTableEntry* page_table = get_page_table(virtual_addr, false);
    if (!page_table) return false;
    
    u32 pt_index = (virtual_addr >> 12) & 0x3FF;
    PageTableEntry& pte = page_table[pt_index];
    
    if (!pte.is_present()) return false;
    
    // Clear page table entry
    pte.value = 0;
    
    // Flush TLB for this address
    VirtualMemoryManager::flush_tlb_single(virtual_addr);
    
    return true;
}

u32 AddressSpace::get_physical_address(u32 virtual_addr) const {
    if (!page_directory) return 0;
    
    PageTableEntry* page_table = const_cast<AddressSpace*>(this)->get_page_table(virtual_addr, false);
    if (!page_table) return 0;
    
    u32 pt_index = (virtual_addr >> 12) & 0x3FF;
    PageTableEntry& pte = page_table[pt_index];
    
    if (!pte.is_present()) return 0;
    
    return pte.get_address() | (virtual_addr & 0xFFF);
}

bool AddressSpace::is_mapped(u32 virtual_addr) const {
    return get_physical_address(virtual_addr) != 0;
}

void AddressSpace::switch_to() const {
    if (page_directory_phys != 0) {
        load_page_directory(page_directory_phys);
    }
}

bool AddressSpace::map_region(u32 virtual_start, u32 physical_start, u32 size, 
                             bool writable, bool user) {
    // Align to page boundaries
    u32 virtual_end = virtual_start + size;
    virtual_start &= PAGE_MASK;
    virtual_end = (virtual_end + PAGE_SIZE - 1) & PAGE_MASK;
    physical_start &= PAGE_MASK;
    
    u32 pages = (virtual_end - virtual_start) / PAGE_SIZE;
    
    for (u32 i = 0; i < pages; i++) {
        u32 virt_addr = virtual_start + (i * PAGE_SIZE);
        u32 phys_addr = physical_start + (i * PAGE_SIZE);
        
        if (!map_page(virt_addr, phys_addr, writable, user)) {
            // Cleanup on failure
            for (u32 j = 0; j < i; j++) {
                unmap_page(virtual_start + (j * PAGE_SIZE));
            }
            return false;
        }
    }
    
    return true;
}

bool AddressSpace::unmap_region(u32 virtual_start, u32 size) {
    u32 virtual_end = virtual_start + size;
    virtual_start &= PAGE_MASK;
    virtual_end = (virtual_end + PAGE_SIZE - 1) & PAGE_MASK;
    
    u32 pages = (virtual_end - virtual_start) / PAGE_SIZE;
    
    for (u32 i = 0; i < pages; i++) {
        unmap_page(virtual_start + (i * PAGE_SIZE));
    }
    
    return true;
}

PageTableEntry* AddressSpace::get_page_table(u32 virtual_addr, bool create) {
    if (!page_directory) return nullptr;
    
    u32 pd_index = (virtual_addr >> 22) & 0x3FF;
    PageDirectoryEntry pde;
    pde.value = page_directory[pd_index];
    
    if (!pde.is_present()) {
        if (!create) return nullptr;
        
        // Create new page table
        u32 page_table_phys = create_page_table();
        if (page_table_phys == 0) return nullptr;
        
        // Update page directory entry
        pde.value = 0;
        pde.set_address(page_table_phys);
        pde.set_present(true);
        pde.set_writable(true);
        pde.set_user(!is_kernel_space);
        
        page_directory[pd_index] = pde.value;
    }
    
    return reinterpret_cast<PageTableEntry*>(pde.get_address());
}

u32 AddressSpace::create_page_table() {
    auto& memory_manager = MemoryManager::get_instance();
    void* page_table_mem = memory_manager.allocate_physical_page();
    
    if (!page_table_mem) return 0;
    
    // Clear page table
    PageTableEntry* page_table = reinterpret_cast<PageTableEntry*>(page_table_mem);
    for (u32 i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        page_table[i].value = 0;
    }
    
    return reinterpret_cast<u32>(page_table_mem);
}

void AddressSpace::setup_kernel_mappings() {
    // Identity map kernel space (3GB - 4GB)
    // This allows kernel code to run with paging enabled
    for (u32 addr = KERNEL_SPACE_START; addr < KERNEL_SPACE_END; addr += PAGE_SIZE) {
        // Skip if we would overflow
        if (addr < KERNEL_SPACE_START) break;
        
        map_page(addr, addr, true, false); // Writable, kernel-only
    }
}

//=============================================================================
// VirtualMemoryManager Implementation  
//=============================================================================

VirtualMemoryManager& VirtualMemoryManager::get_instance() {
    if (!instance) {
        static VirtualMemoryManager vm_manager;
        instance = &vm_manager;
    }
    return *instance;
}

void VirtualMemoryManager::initialize() {
    // Create kernel address space
    kernel_address_space = new AddressSpace(true);
    current_address_space = kernel_address_space;
    
    // Enable paging
    enable_paging();
    
    // Switch to kernel address space
    kernel_address_space->switch_to();
}

AddressSpace* VirtualMemoryManager::create_user_address_space() {
    return new AddressSpace(false);
}

void VirtualMemoryManager::switch_address_space(AddressSpace* address_space) {
    if (address_space && address_space != current_address_space) {
        current_address_space = address_space;
        address_space->switch_to();
    }
}

void VirtualMemoryManager::flush_tlb() {
    flush_tlb_asm();
}

void VirtualMemoryManager::flush_tlb_single(u32 virtual_addr) {
    invlpg(virtual_addr);
}

void VirtualMemoryManager::enable_paging() {
    enable_paging_asm();
}

bool VirtualMemoryManager::is_paging_enabled() {
    return (get_cr0() & 0x80000000) != 0;
}

} // namespace kira::system

//=============================================================================
// Assembly functions (inline assembly implementations)
//=============================================================================

extern "C" {

void load_page_directory(kira::system::u32 page_dir_phys) {
    asm volatile(
        "mov %0, %%cr3"
        :
        : "r" (page_dir_phys)
        : "memory"
    );
}

void enable_paging_asm() {
    asm volatile(
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"  // Set PG bit
        "mov %%eax, %%cr0"
        :
        :
        : "eax", "memory"
    );
}

kira::system::u32 get_cr0() {
    kira::system::u32 cr0;
    asm volatile(
        "mov %%cr0, %0"
        : "=r" (cr0)
    );
    return cr0;
}

void flush_tlb_asm() {
    asm volatile(
        "mov %%cr3, %%eax\n\t"
        "mov %%eax, %%cr3"
        :
        :
        : "eax", "memory"
    );
}

void invlpg(kira::system::u32 virtual_addr) {
    asm volatile(
        "invlpg (%0)"
        :
        : "r" (virtual_addr)
        : "memory"
    );
}

} 