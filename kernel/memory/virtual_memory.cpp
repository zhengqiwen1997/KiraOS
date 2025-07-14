#include "memory/virtual_memory.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"
#include "debug/serial_debugger.hpp"

namespace kira::system {

using namespace kira::debug;

//=============================================================================
// Implementation-specific constants (internal use only)
//=============================================================================

// Bit manipulation constants
constexpr u32 PAGE_OFFSET_BITS = 12;                    // 12 bits for page offset
constexpr u32 PAGE_TABLE_INDEX_BITS = 22;               // 22 bits for page directory index
constexpr u32 PAGE_TABLE_INDEX_MASK = 0x3FF;            // 10 bits mask for page table index
constexpr u32 PAGE_OFFSET_MASK = 0xFFF;                 // 12 bits mask for page offset

// Kernel memory layout (implementation detail)
constexpr u32 KERNEL_CODE_START = 0x100000;             // 1MB - Kernel code start
constexpr u32 KERNEL_CODE_END = 0x400000;               // 4MB - Kernel code end
constexpr u32 VGA_BUFFER_ADDR = 0xB8000;                // VGA text buffer address
constexpr u32 LOW_MEMORY_END = 0x100000;                // 1MB - End of low memory
constexpr u32 MEMORY_MANAGER_START = 0x200000;          // 2MB - Memory manager structures start
constexpr u32 MEMORY_MANAGER_END = 0x800000;            // 8MB - Memory manager structures end

// Hardware register bits
constexpr u32 CR0_PAGING_BIT = 0x80000000;              // CR0 PG bit (bit 31)

// System limits
constexpr u32 MAX_USER_ADDRESS_SPACES = 16;             // Maximum user address spaces
constexpr u32 MEMORY_ALIGNMENT = 4;                     // Memory alignment for placement new

// Test constants
constexpr u32 TEST_MAGIC_VALUE = 0x12345678;            // Magic value for testing

// Static member definitions
VirtualMemoryManager* VirtualMemoryManager::instance = nullptr;

// Assembly functions for low-level paging operations
extern "C" {
    void load_page_directory(u32 pageDirPhys);
    void enable_paging_asm();
    u32 get_cr0();
    void flush_tlb_asm();
    void invlpg(u32 virtualAddr);
}

//=============================================================================
// AddressSpace Implementation
//=============================================================================

AddressSpace::AddressSpace(bool kernelSpace) : isKernelSpace(kernelSpace) {
    SerialDebugger::println("AddressSpace constructor called");
    
    auto& memoryManager = MemoryManager::get_instance();
    void* pageDirMem = memoryManager.allocate_physical_page();
    
    if (!pageDirMem) {
        SerialDebugger::println("ERROR: Failed to allocate page directory!");
        pageDirectory = nullptr;
        pageDirectoryPhys = 0;
        return;
    }
    
    pageDirectoryPhys = reinterpret_cast<u32>(pageDirMem);
    pageDirectory = reinterpret_cast<u32*>(pageDirMem);
    
    SerialDebugger::print("Page directory allocated at: ");
    SerialDebugger::print_hex(pageDirectoryPhys);
    SerialDebugger::println("");
    
    // Clear page directory
    for (u32 i = 0; i < PAGE_DIRECTORY_ENTRIES; i++) {
        pageDirectory[i] = 0;
    }
    
    if (kernelSpace) {
        SerialDebugger::println("Setting up kernel mappings...");
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
    u32 ptIndex = (virtualAddr >> PAGE_OFFSET_BITS) & PAGE_TABLE_INDEX_MASK;
    
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
    
    u32 ptIndex = (virtualAddr >> PAGE_OFFSET_BITS) & PAGE_TABLE_INDEX_MASK;
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
    
    u32 ptIndex = (virtualAddr >> PAGE_OFFSET_BITS) & PAGE_TABLE_INDEX_MASK;
    PageTableEntry& pte = pageTable[ptIndex];
    
    if (!pte.is_present()) return 0;
    
    return pte.get_address() | (virtualAddr & PAGE_OFFSET_MASK);
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
    
    u32 pdIndex = (virtualAddr >> PAGE_TABLE_INDEX_BITS) & PAGE_TABLE_INDEX_MASK;
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
    SerialDebugger::println("Setting up comprehensive kernel mappings...");
    
    // 1. Map kernel code and data (1MB - 4MB to be safe)
    SerialDebugger::print("Mapping kernel code/data: ");
    SerialDebugger::print_hex(KERNEL_CODE_START);
    SerialDebugger::print(" to ");
    SerialDebugger::print_hex(KERNEL_CODE_END);
    SerialDebugger::println("");
    
    for (u32 addr = KERNEL_CODE_START; addr < KERNEL_CODE_END; addr += PAGE_SIZE) {
        if (!map_page(addr, addr, true, false)) {
            SerialDebugger::print("ERROR: Failed to map kernel page at ");
            SerialDebugger::print_hex(addr);
            SerialDebugger::println("");
            break;
        }
    }
    
    // 2. Map VGA buffer
    SerialDebugger::println("Mapping VGA buffer...");
    if (!map_page(VGA_BUFFER_ADDR, VGA_BUFFER_ADDR, true, false)) {
        SerialDebugger::println("ERROR: Failed to map VGA buffer!");
    }
    
    // 3. Map low memory (0x0 - 1MB) for compatibility
    SerialDebugger::println("Mapping low memory (0x0 - 1MB)...");
    for (u32 addr = 0; addr < LOW_MEMORY_END; addr += PAGE_SIZE) {
        if (!map_page(addr, addr, true, false)) {
            // Don't spam errors for low memory, some pages might be reserved
            break;
        }
    }
    
    // 4. Map memory manager structures area (2MB - 8MB)
    SerialDebugger::println("Mapping memory manager area (2MB - 8MB)...");
    for (u32 addr = MEMORY_MANAGER_START; addr < MEMORY_MANAGER_END; addr += PAGE_SIZE) {
        if (!map_page(addr, addr, true, false)) {
            // Stop if we can't allocate more pages
            break;
        }
    }
    
    SerialDebugger::println("Kernel mappings setup complete");
}

//=============================================================================
// VirtualMemoryManager Implementation  
//=============================================================================

// Pre-allocate memory for AddressSpace objects to avoid dynamic allocation
static u8 kernelAddressSpaceMemory[sizeof(AddressSpace)] __attribute__((aligned(MEMORY_ALIGNMENT)));
static u8 userAddressSpaceMemory[MAX_USER_ADDRESS_SPACES][sizeof(AddressSpace)] __attribute__((aligned(MEMORY_ALIGNMENT)));
static u32 nextUserAddressSpaceIndex = 0;

VirtualMemoryManager& VirtualMemoryManager::get_instance() {
    if (!instance) {
        static VirtualMemoryManager vmManager;
        instance = &vmManager;
    }
    return *instance;
}

void VirtualMemoryManager::initialize() {
    // Initialize serial debugging
    SerialDebugger::init();
    SerialDebugger::println("=== VirtualMemoryManager::initialize() ===");
    
    // Create kernel address space using placement new
    SerialDebugger::println("Creating kernel address space...");
    kernelAddressSpace = new(kernelAddressSpaceMemory) AddressSpace(true);
    currentAddressSpace = kernelAddressSpace;
    
    // Check if kernel address space creation failed
    if (!kernelAddressSpace || kernelAddressSpace->get_page_directory_phys() == 0) {
        SerialDebugger::println("ERROR: Failed to create kernel address space!");
        return;
    }
    
    SerialDebugger::print("Kernel address space created successfully at: ");
    SerialDebugger::print_hex(kernelAddressSpace->get_page_directory_phys());
    SerialDebugger::println("");
    
    // Now let's try to enable paging step by step
    SerialDebugger::println("Loading page directory into CR3...");
    load_page_directory(kernelAddressSpace->get_page_directory_phys());
    
    SerialDebugger::println("Enabling paging...");
    enable_paging();
    
    SerialDebugger::println("Paging enabled successfully!");
    
    // Test that we can still access memory after paging
    SerialDebugger::println("Testing memory access after paging...");
    volatile u32 test_var = TEST_MAGIC_VALUE;
    SerialDebugger::print("Test variable value: ");
    SerialDebugger::print_hex(test_var);
    SerialDebugger::println("");
    
    SerialDebugger::println("=== VirtualMemoryManager initialization complete ===");
}

AddressSpace* VirtualMemoryManager::create_user_address_space() {
    if (nextUserAddressSpaceIndex >= MAX_USER_ADDRESS_SPACES) {
        return nullptr; // No more user address spaces available
    }
    
    AddressSpace* userSpace = new(&userAddressSpaceMemory[nextUserAddressSpaceIndex]) AddressSpace(false);
    nextUserAddressSpaceIndex++;
    return userSpace;
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
    return (get_cr0() & CR0_PAGING_BIT) != 0;
}

} // namespace kira::system

//=============================================================================
// Assembly functions (inline assembly implementations)
//=============================================================================

extern "C" {

void load_page_directory(kira::system::u32 pageDirPhys) {
    asm volatile(
        "mov %0, %%cr3"
        :
        : "r" (pageDirPhys)
        : "memory"
    );
}

void enable_paging_asm() {
    asm volatile(
        "mov %%cr0, %%eax\n\t"
        "or %0, %%eax\n\t"  // Set PG bit
        "mov %%eax, %%cr0"
        :
        : "i" (kira::system::CR0_PAGING_BIT)
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