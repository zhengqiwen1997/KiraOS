#include "memory/memory.hpp"
#include "memory/memory_manager.hpp"
#include "core/utils.hpp"

namespace kira::system {

// Global variables to store memory map info from bootloader
u32 gMemoryMapAddr = 0;
u32 gMemoryMapCount = 0;

// Kernel memory layout constants
constexpr u32 KERNEL_STRUCTURES_BASE = 0x00200000;  // 2MB - Base for kernel data structures (explicit 32-bit)
constexpr u32 KERNEL_STRUCTURES_SIZE = 0x00100000;  // 1MB - Reserved space for kernel structures
constexpr u32 MEMORY_MANAGER_ADDR = KERNEL_STRUCTURES_BASE;
constexpr u32 FREE_PAGE_STACK_ADDR = (KERNEL_STRUCTURES_BASE + 0x1000);  // 2MB + 4KB

// Compile-time validation of memory constants
// These static assertions will cause compilation to fail if dangerous values are used
static_assert(KERNEL_STRUCTURES_BASE != 0, 
    "KERNEL_STRUCTURES_BASE cannot be 0 (NULL pointer)");
static_assert(KERNEL_STRUCTURES_BASE >= 0x00100000, 
    "KERNEL_STRUCTURES_BASE must be >= 1MB to avoid low memory conflicts");
static_assert(KERNEL_STRUCTURES_BASE <= 0xFFFFFFFF, 
    "KERNEL_STRUCTURES_BASE must fit in 32-bit address space");
static_assert((KERNEL_STRUCTURES_BASE & 0xFFFFFFFF) == KERNEL_STRUCTURES_BASE,
    "KERNEL_STRUCTURES_BASE appears to be a 64-bit value that will be truncated!");
static_assert((KERNEL_STRUCTURES_BASE & 0xFFFFFFFF) != 0,
    "KERNEL_STRUCTURES_BASE truncates to 0 - this is dangerous!");
// More conservative limit for typical systems
static_assert(KERNEL_STRUCTURES_BASE < 0x10000000,
    "KERNEL_STRUCTURES_BASE > 256MB is likely beyond typical system RAM - use a smaller value");

// Additional warning for borderline values
#if KERNEL_STRUCTURES_BASE > 0x08000000
#warning "KERNEL_STRUCTURES_BASE > 128MB - ensure your system has sufficient RAM!"
#endif

// Safety constants for boundary checking
constexpr u32 MIN_SAFE_ADDRESS = 0x00100000;  // 1MB - Don't use low memory
constexpr u32 MAX_32BIT_ADDRESS = 0xFFFFFFFF;  // Maximum 32-bit address
constexpr u32 EXPECTED_MIN_RAM = 0x01000000;  // 16MB - Minimum expected RAM
constexpr u32 EXPECTED_MAX_RAM = 0x40000000;  // 1GB - Maximum reasonable RAM for this system

// Page-based constants (calculated at compile time)
constexpr u32 LOW_MEMORY_PAGES = 256;  // 1MB / 4KB = 256 pages
constexpr u32 KERNEL_START_PAGE = (KERNEL_STRUCTURES_BASE / PAGE_SIZE);  // 2MB / 4KB = 512
constexpr u32 KERNEL_END_PAGE = ((KERNEL_STRUCTURES_BASE + KERNEL_STRUCTURES_SIZE) / PAGE_SIZE);  // 3MB / 4KB = 768

// Boundary checking functions
static bool is_address_valid(u32 address) {
    // Check for obvious invalid addresses
    if (address == 0) return false;  // NULL pointer
    if (address < MIN_SAFE_ADDRESS) return false;  // Too low (avoid low memory)
    if (address > MAX_32BIT_ADDRESS) return false;  // Beyond 32-bit limit
    
    return true;
}

static bool is_address_in_physical_ram(u32 address, u32 totalRamSize) {
    // Check if address is within detected physical RAM
    if (address >= totalRamSize) return false;
    
    return true;
}

static u32 calculate_total_usable_ram() {
    u32 total = 0;
    
    if (gMemoryMapAddr == 0 || gMemoryMapCount == 0) {
        return 0;  // No memory map available
    }
    
    MemoryMapEntry* entries = (MemoryMapEntry*)gMemoryMapAddr;
    for (u32 i = 0; i < gMemoryMapCount; i++) {
        if (entries[i].type == static_cast<u32>(MemoryType::USABLE)) {
            // Find the highest usable address
            u32 endAddr = (u32)(entries[i].baseAddress + entries[i].length);
            if (endAddr > total) {
                total = endAddr;
            }
        }
    }
    
    return total;
}

static bool validate_kernel_structures_placement() {
    u32 totalRam = calculate_total_usable_ram();
    
    // Check if we have any RAM info
    if (totalRam == 0) {
        return false;  // Can't validate without memory map
    }
    
    // Check if KERNEL_STRUCTURES_BASE is valid
    if (!is_address_valid(KERNEL_STRUCTURES_BASE)) {
        return false;
    }
    
    // Check if kernel structures fit within physical RAM
    u32 structuresEnd = KERNEL_STRUCTURES_BASE + KERNEL_STRUCTURES_SIZE;
    if (!is_address_in_physical_ram(KERNEL_STRUCTURES_BASE, totalRam) ||
        !is_address_in_physical_ram(structuresEnd, totalRam)) {
        return false;
    }
    
    // Sanity check: RAM size should be reasonable
    if (totalRam < EXPECTED_MIN_RAM || totalRam > EXPECTED_MAX_RAM) {
        // Still allow it, but it's suspicious
    }
    
    return true;
}

// Global pointer to avoid constructor issues
static MemoryManager* gMemoryManager = nullptr;

MemoryManager& MemoryManager::get_instance() {
    if (!gMemoryManager) {
        // Validate kernel structures placement before using them
        if (!validate_kernel_structures_placement()) {
            // Fallback to a safe address if validation fails
            // Use a simple calculation based on available memory
            u32 totalRam = calculate_total_usable_ram();
            u32 safeAddr = 0x00200000;  // Default to 2MB
            
            // If we have enough RAM, use 1/8 of total RAM as base address
            if (totalRam > 0x00800000) {  // If more than 8MB
                safeAddr = totalRam / 8;
                // Align to 1MB boundary
                safeAddr = (safeAddr + 0x000FFFFF) & 0xFFF00000;
                // Ensure it's at least 2MB
                if (safeAddr < 0x00200000) safeAddr = 0x00200000;
            }
            
            gMemoryManager = (MemoryManager*)safeAddr;
        } else {
            // Use the configured address
            gMemoryManager = (MemoryManager*)MEMORY_MANAGER_ADDR;
        }
        
        // Initialize only the essential fields for stack-based allocator
        gMemoryManager->memoryMap = nullptr;
        gMemoryManager->memoryMapSize = 0;
        gMemoryManager->pageDirectory = nullptr;
        gMemoryManager->freePageStack = nullptr;
        gMemoryManager->freePageCount = 0;
        gMemoryManager->maxFreePages = 0;
        
        // Auto-initialize with memory map if available
        if (gMemoryMapAddr != 0 && gMemoryMapCount > 0) {
            MemoryMapEntry* entries = (MemoryMapEntry*)gMemoryMapAddr;
            gMemoryManager->initialize(entries, gMemoryMapCount);
        }
    }
    return *gMemoryManager;
}

// Initialize the stack-based allocator
void MemoryManager::initialize(const MemoryMapEntry* memoryMap, u32 memoryMapSize) {
    this->memoryMap = memoryMap;
    this->memoryMapSize = memoryMapSize;
    
    // Calculate safe stack address based on manager address
    u32 managerAddr = (u32)this;
    u32 stackAddr = managerAddr + 0x1000;  // Manager + 4KB
    
    // Validate stack address
    u32 totalRam = calculate_total_usable_ram();
    if (totalRam > 0 && !is_address_in_physical_ram(stackAddr + 0x1000, totalRam)) {
        // Stack would be outside RAM, use a safer location
        stackAddr = managerAddr + sizeof(MemoryManager);
        // Align to 4-byte boundary
        stackAddr = (stackAddr + 3) & ~3;
    }
    
    // Initialize the free page stack
    freePageStack = (u32*)stackAddr;
    freePageCount = 0;
    maxFreePages = 1024;  // Limit to 1024 pages (4KB stack size)
    
    // Populate the stack with free pages from usable memory regions
    for (u32 i = 0; i < memoryMapSize && freePageCount < maxFreePages; i++) {
        if (memoryMap[i].type == static_cast<u32>(MemoryType::USABLE)) {
            u32 startPage = memoryMap[i].baseAddress / PAGE_SIZE;
            u32 endPage = (memoryMap[i].baseAddress + memoryMap[i].length) / PAGE_SIZE;
            
            // Skip low memory (first 1MB) 
            if (startPage < LOW_MEMORY_PAGES) {
                startPage = LOW_MEMORY_PAGES;  // Skip first 1MB
            }
            
            // Skip kernel structures region to avoid allocating our own memory
            u32 kernelStartPage = managerAddr / PAGE_SIZE;
            u32 kernelEndPage = (managerAddr + KERNEL_STRUCTURES_SIZE) / PAGE_SIZE;
            if (startPage >= kernelStartPage && startPage < kernelEndPage) {
                startPage = kernelEndPage;  // Start after kernel structures
            }
            
            // Add pages to the free stack with boundary checking
            for (u32 page = startPage; page < endPage && freePageCount < maxFreePages; page++) {
                u32 pageAddr = page * PAGE_SIZE;
                
                // Validate page address before adding to stack
                if (is_address_valid(pageAddr) && 
                    (totalRam == 0 || is_address_in_physical_ram(pageAddr, totalRam))) {
                    freePageStack[freePageCount++] = pageAddr;
                }
            }
        }
    }
}

// Stack-based allocator: Pop from free page stack
void* MemoryManager::allocate_physical_page() {
    // Check if we have free pages
    if (freePageCount == 0) {
        return nullptr;  // No free pages available
    }
    
    // Pop from stack (take the last page)
    u32 pageAddr = freePageStack[--freePageCount];
    
    // Additional safety check
    if (!is_address_valid(pageAddr)) {
        return nullptr;  // Invalid address in stack
    }
    
    return (void*)pageAddr;
}

// Stack-based allocator: Push back to free page stack
void MemoryManager::free_physical_page(void* page) {
    if (!page || freePageCount >= maxFreePages) {
        return;  // Invalid page or stack full
    }
    
    u32 pageAddr = (u32)page;
    
    // Validate the page address before adding back to stack
    if (!is_address_valid(pageAddr)) {
        return;  // Don't add invalid addresses to stack
    }
    
    // Additional check: ensure it's page-aligned
    if (pageAddr & (PAGE_SIZE - 1)) {
        return;  // Not page-aligned
    }
    
    // Push back to stack
    freePageStack[freePageCount++] = pageAddr;
}

} // namespace kira::system 