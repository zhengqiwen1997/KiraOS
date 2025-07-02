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
    void invlpg(kira::system::u32 virtualAddr);
}

//=============================================================================
// AddressSpace Implementation
//=============================================================================

AddressSpace::AddressSpace(bool kernel_space) : isKernelSpace(kernel_space) {
    auto& memoryManager = MemoryManager::get_instance();
    void* pageDirMem = memoryManager.allocate_physical_page();
    
    if (!pageDirMem) {
        pageDirectory = nullptr;
        pageDirectoryPhys = 0;
        return;
    }
    
    pageDirectoryPhys = reinterpret_cast<u32>(pageDirMem);
    pageDirectory = reinterpret_cast<u32*>(pageDirMem);
    
    // Clear page directory
    for (u32 i = 0; i < PAGE_DIRECTORY_ENTRIES; i++) {
        pageDirectory[i] = 0;
    }
    
    if (kernel_space) {
        setup_kernel_mappings();
    }
}

AddressSpace::~AddressSpace() {
    if (!pageDirectory) return;
    
    auto& memoryManager = MemoryManager::get_instance();
    
    // Free all page tables
    for (u32 i = 0; i < PAGE_DIRECTORY_ENTRIES; i++) {
        PageDirectoryEntry pde;
        pde.value = pageDirectory[i];
        
        if (pde.is_present()) {
            memoryManager.free_physical_page(reinterpret_cast<void*>(pde.get_address()));
        }
    }
    
    // Free page directory
    memoryManager.free_physical_page(reinterpret_cast<void*>(pageDirectoryPhys));
}

bool AddressSpace::map_page(u32 virtualAddr, u32 physicalAddr, bool writable, bool user) {
    if (!pageDirectory) return false;
    
    // Align addresses to page boundaries
    virtualAddr &= PAGE_MASK;
    physicalAddr &= PAGE_MASK;
    
    // Get page table
    PageTableEntry* pageTable = get_page_table(virtualAddr, true);
    if (!pageTable) return false;
    
    // Calculate page table index
    u32 ptIndex = (virtualAddr >> 12) & 0x3FF;
    
    // Set up page table entry
    PageTableEntry& pte = pageTable[ptIndex];
    pte.value = 0;
    pte.set_address(physicalAddr);
    pte.set_present(true);
    pte.set_writable(writable);
    pte.set_user(user);
    
    // Flush TLB for this address
    VirtualMemoryManager::flush_tlb_single(virtualAddr);
    
    return true;
}

bool AddressSpace::unmap_page(u32 virtualAddr) {
    if (!pageDirectory) return false;
    
    virtualAddr &= PAGE_MASK;
    
    PageTableEntry* pageTable = get_page_table(virtualAddr, false);
    if (!pageTable) return false;
    
    u32 ptIndex = (virtualAddr >> 12) & 0x3FF;
    PageTableEntry& pte = pageTable[ptIndex];
    
    if (!pte.is_present()) return false;
    
    // Clear page table entry
    pte.value = 0;
    
    // Flush TLB for this address
    VirtualMemoryManager::flush_tlb_single(virtualAddr);
    
    return true;
}

u32 AddressSpace::get_physical_address(u32 virtualAddr) const {
    if (!pageDirectory) return 0;
    
    PageTableEntry* pageTable = const_cast<AddressSpace*>(this)->get_page_table(virtualAddr, false);
    if (!pageTable) return 0;
    
    u32 ptIndex = (virtualAddr >> 12) & 0x3FF;
    PageTableEntry& pte = pageTable[ptIndex];
    
    if (!pte.is_present()) return 0;
    
    return pte.get_address() | (virtualAddr & 0xFFF);
}

bool AddressSpace::is_mapped(u32 virtualAddr) const {
    return get_physical_address(virtualAddr) != 0;
}

void AddressSpace::switch_to() const {
    if (pageDirectoryPhys != 0) {
        load_page_directory(pageDirectoryPhys);
    }
}

bool AddressSpace::map_region(u32 virtualStart, u32 physicalStart, u32 size, 
                             bool writable, bool user) {
    // Align to page boundaries
    u32 virtualEnd = virtualStart + size;
    virtualStart &= PAGE_MASK;
    virtualEnd = (virtualEnd + PAGE_SIZE - 1) & PAGE_MASK;
    physicalStart &= PAGE_MASK;
    
    u32 pages = (virtualEnd - virtualStart) / PAGE_SIZE;
    
    for (u32 i = 0; i < pages; i++) {
        u32 virtAddr = virtualStart + (i * PAGE_SIZE);
        u32 physAddr = physicalStart + (i * PAGE_SIZE);
        
        if (!map_page(virtAddr, physAddr, writable, user)) {
            // Cleanup on failure
            for (u32 j = 0; j < i; j++) {
                unmap_page(virtualStart + (j * PAGE_SIZE));
            }
            return false;
        }
    }
    
    return true;
}

bool AddressSpace::unmap_region(u32 virtualStart, u32 size) {
    u32 virtualEnd = virtualStart + size;
    virtualStart &= PAGE_MASK;
    virtualEnd = (virtualEnd + PAGE_SIZE - 1) & PAGE_MASK;
    
    u32 pages = (virtualEnd - virtualStart) / PAGE_SIZE;
    
    for (u32 i = 0; i < pages; i++) {
        unmap_page(virtualStart + (i * PAGE_SIZE));
    }
    
    return true;
}

PageTableEntry* AddressSpace::get_page_table(u32 virtualAddr, bool create) {
    if (!pageDirectory) return nullptr;
    
    u32 pdIndex = (virtualAddr >> 22) & 0x3FF;
    PageDirectoryEntry pde;
    pde.value = pageDirectory[pdIndex];
    
    if (!pde.is_present()) {
        if (!create) return nullptr;
        
        // Create new page table
        u32 pageTablePhys = create_page_table();
        if (pageTablePhys == 0) return nullptr;
        
        // Update page directory entry
        pde.value = 0;
        pde.set_address(pageTablePhys);
        pde.set_present(true);
        pde.set_writable(true);
        pde.set_user(!isKernelSpace);
        
        pageDirectory[pdIndex] = pde.value;
    }
    
    return reinterpret_cast<PageTableEntry*>(pde.get_address());
}

u32 AddressSpace::create_page_table() {
    auto& memoryManager = MemoryManager::get_instance();
    void* pageTableMem = memoryManager.allocate_physical_page();
    
    if (!pageTableMem) return 0;
    
    // Clear page table
    PageTableEntry* pageTable = reinterpret_cast<PageTableEntry*>(pageTableMem);
    for (u32 i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        pageTable[i].value = 0;
    }
    
    return reinterpret_cast<u32>(pageTableMem);
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
    kernelAddressSpace = new AddressSpace(true);
    currentAddressSpace = kernelAddressSpace;
    
    // Enable paging
    enable_paging();
    
    // Switch to kernel address space
    kernelAddressSpace->switch_to();
}

AddressSpace* VirtualMemoryManager::create_user_address_space() {
    return new AddressSpace(false);
}

void VirtualMemoryManager::switch_address_space(AddressSpace* addressSpace) {
    if (addressSpace && addressSpace != currentAddressSpace) {
        currentAddressSpace = addressSpace;
        addressSpace->switch_to();
    }
}

void VirtualMemoryManager::flush_tlb() {
    flush_tlb_asm();
}

void VirtualMemoryManager::flush_tlb_single(u32 virtualAddr) {
    invlpg(virtualAddr);
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

void invlpg(kira::system::u32 virtualAddr) {
    asm volatile("invlpg (%0)" 
                 : 
                 : "r" (virtualAddr)
                 : "memory");
}

} 