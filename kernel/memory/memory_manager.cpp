#include "memory/memory.hpp"
#include "memory/memory_manager.hpp"
#include "display/vga.hpp"
#include "core/utils.hpp"

// Kernel memory layout constants
#define KERNEL_STRUCTURES_BASE  0x00200000  // 2MB - Base for kernel data structures (explicit 32-bit)
#define KERNEL_STRUCTURES_SIZE  0x00100000  // 1MB - Reserved space for kernel structures
#define MEMORY_MANAGER_ADDR     KERNEL_STRUCTURES_BASE
#define FREE_PAGE_STACK_ADDR    (KERNEL_STRUCTURES_BASE + 0x1000)  // 2MB + 4KB

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
#define MIN_SAFE_ADDRESS        0x00100000  // 1MB - Don't use low memory
#define MAX_32BIT_ADDRESS       0xFFFFFFFF  // Maximum 32-bit address
#define EXPECTED_MIN_RAM        0x01000000  // 16MB - Minimum expected RAM
#define EXPECTED_MAX_RAM        0x40000000  // 1GB - Maximum reasonable RAM for this system

// Page-based constants (calculated at compile time)
#define LOW_MEMORY_PAGES        256       // 1MB / 4KB = 256 pages
#define KERNEL_START_PAGE       (KERNEL_STRUCTURES_BASE / PAGE_SIZE)  // 2MB / 4KB = 512
#define KERNEL_END_PAGE         ((KERNEL_STRUCTURES_BASE + KERNEL_STRUCTURES_SIZE) / PAGE_SIZE)  // 3MB / 4KB = 768

// Global variables to store memory map info from bootloader
kira::system::u32 g_memory_map_addr = 0;
kira::system::u32 g_memory_map_count = 0;

namespace kira::system {



// Boundary checking functions
static bool is_address_valid(u32 address) {
    // Check for obvious invalid addresses
    if (address == 0) return false;  // NULL pointer
    if (address < MIN_SAFE_ADDRESS) return false;  // Too low (avoid low memory)
    if (address > MAX_32BIT_ADDRESS) return false;  // Beyond 32-bit limit
    
    return true;
}

static bool is_address_in_physical_ram(u32 address, u32 total_ram_size) {
    // Check if address is within detected physical RAM
    if (address >= total_ram_size) return false;
    
    return true;
}

static u32 calculate_total_usable_ram() {
    u32 total = 0;
    
    if (g_memory_map_addr == 0 || g_memory_map_count == 0) {
        return 0;  // No memory map available
    }
    
    MemoryMapEntry* entries = (MemoryMapEntry*)g_memory_map_addr;
    for (u32 i = 0; i < g_memory_map_count; i++) {
        if (entries[i].type == static_cast<u32>(MemoryType::USABLE)) {
            // Find the highest usable address
            u32 end_addr = (u32)(entries[i].base_address + entries[i].length);
            if (end_addr > total) {
                total = end_addr;
            }
        }
    }
    
    return total;
}

static bool validate_kernel_structures_placement() {
    u32 total_ram = calculate_total_usable_ram();
    
    // Check if we have any RAM info
    if (total_ram == 0) {
        return false;  // Can't validate without memory map
    }
    
    // Check if KERNEL_STRUCTURES_BASE is valid
    if (!is_address_valid(KERNEL_STRUCTURES_BASE)) {
        return false;
    }
    
    // Check if kernel structures fit within physical RAM
    u32 structures_end = KERNEL_STRUCTURES_BASE + KERNEL_STRUCTURES_SIZE;
    if (!is_address_in_physical_ram(KERNEL_STRUCTURES_BASE, total_ram) ||
        !is_address_in_physical_ram(structures_end, total_ram)) {
        return false;
    }
    
    // Sanity check: RAM size should be reasonable
    if (total_ram < EXPECTED_MIN_RAM || total_ram > EXPECTED_MAX_RAM) {
        // Still allow it, but it's suspicious
    }
    
    return true;
}

// Global pointer to avoid constructor issues
static MemoryManager* g_memory_manager = nullptr;

MemoryManager& MemoryManager::get_instance() {
    if (!g_memory_manager) {
        // Validate kernel structures placement before using them
        if (!validate_kernel_structures_placement()) {
            // Fallback to a safe address if validation fails
            // Use a simple calculation based on available memory
            u32 total_ram = calculate_total_usable_ram();
            u32 safe_addr = 0x00200000;  // Default to 2MB
            
            // If we have enough RAM, use 1/8 of total RAM as base address
            if (total_ram > 0x00800000) {  // If more than 8MB
                safe_addr = total_ram / 8;
                // Align to 1MB boundary
                safe_addr = (safe_addr + 0x000FFFFF) & 0xFFF00000;
                // Ensure it's at least 2MB
                if (safe_addr < 0x00200000) safe_addr = 0x00200000;
            }
            
            g_memory_manager = (MemoryManager*)safe_addr;
        } else {
            // Use the configured address
            g_memory_manager = (MemoryManager*)MEMORY_MANAGER_ADDR;
        }
        
        // Initialize only the essential fields for stack-based allocator
        g_memory_manager->memory_map = nullptr;
        g_memory_manager->memory_map_size = 0;
        g_memory_manager->page_directory = nullptr;
        g_memory_manager->free_page_stack = nullptr;
        g_memory_manager->free_page_count = 0;
        g_memory_manager->max_free_pages = 0;
        
        // Auto-initialize with memory map if available
        if (g_memory_map_addr != 0 && g_memory_map_count > 0) {
            MemoryMapEntry* entries = (MemoryMapEntry*)g_memory_map_addr;
            g_memory_manager->initialize(entries, g_memory_map_count);
        }
    }
    return *g_memory_manager;
}

// Initialize the stack-based allocator
void MemoryManager::initialize(const MemoryMapEntry* memory_map, u32 memory_map_size) {
    this->memory_map = memory_map;
    this->memory_map_size = memory_map_size;
    
    // Calculate safe stack address based on manager address
    u32 manager_addr = (u32)this;
    u32 stack_addr = manager_addr + 0x1000;  // Manager + 4KB
    
    // Validate stack address
    u32 total_ram = calculate_total_usable_ram();
    if (total_ram > 0 && !is_address_in_physical_ram(stack_addr + 0x1000, total_ram)) {
        // Stack would be outside RAM, use a safer location
        stack_addr = manager_addr + sizeof(MemoryManager);
        // Align to 4-byte boundary
        stack_addr = (stack_addr + 3) & ~3;
    }
    
    // Initialize the free page stack
    free_page_stack = (u32*)stack_addr;
    free_page_count = 0;
    max_free_pages = 1024;  // Limit to 1024 pages (4KB stack size)
    
    // Populate the stack with free pages from usable memory regions
    for (u32 i = 0; i < memory_map_size && free_page_count < max_free_pages; i++) {
        if (memory_map[i].type == static_cast<u32>(MemoryType::USABLE)) {
            u32 start_page = memory_map[i].base_address / PAGE_SIZE;
            u32 end_page = (memory_map[i].base_address + memory_map[i].length) / PAGE_SIZE;
            
            // Skip low memory (first 1MB) 
            if (start_page < LOW_MEMORY_PAGES) {
                start_page = LOW_MEMORY_PAGES;  // Skip first 1MB
            }
            
            // Skip kernel structures region to avoid allocating our own memory
            u32 kernel_start_page = manager_addr / PAGE_SIZE;
            u32 kernel_end_page = (manager_addr + KERNEL_STRUCTURES_SIZE) / PAGE_SIZE;
            if (start_page >= kernel_start_page && start_page < kernel_end_page) {
                start_page = kernel_end_page;  // Start after kernel structures
            }
            
            // Add pages to the free stack with boundary checking
            for (u32 page = start_page; page < end_page && free_page_count < max_free_pages; page++) {
                u32 page_addr = page * PAGE_SIZE;
                
                // Validate page address before adding to stack
                if (is_address_valid(page_addr) && 
                    (total_ram == 0 || is_address_in_physical_ram(page_addr, total_ram))) {
                    free_page_stack[free_page_count++] = page_addr;
                }
            }
        }
    }
}

// Stack-based allocator: Pop from free page stack
void* MemoryManager::allocate_physical_page() {
    // Check if we have free pages
    if (free_page_count == 0) {
        return nullptr;  // No free pages available
    }
    
    // Pop from stack (take the last page)
    u32 page_addr = free_page_stack[--free_page_count];
    
    // Additional safety check
    if (!is_address_valid(page_addr)) {
        return nullptr;  // Invalid address in stack
    }
    
    return (void*)page_addr;
}

// Stack-based allocator: Push back to free page stack
void MemoryManager::free_physical_page(void* page) {
    if (!page || free_page_count >= max_free_pages) {
        return;  // Invalid page or stack full
    }
    
    u32 page_addr = (u32)page;
    
    // Validate the page address before adding back to stack
    if (!is_address_valid(page_addr)) {
        return;  // Don't add invalid addresses to stack
    }
    
    // Additional check: ensure it's page-aligned
    if (page_addr & (PAGE_SIZE - 1)) {
        return;  // Not page-aligned
    }
    
    // Push back to stack
    free_page_stack[free_page_count++] = page_addr;
}

} // namespace kira::system 