#include "arch/x86/tss.hpp"
#include "arch/x86/idt.hpp"
#include "core/io.hpp"

namespace kira::system {

// Static TSS instance
TSS TSSManager::tss;

void TSSManager::initialize() {
    // Clear the TSS structure
    u8* tss_ptr = reinterpret_cast<u8*>(&tss);
    for (u32 i = 0; i < sizeof(TSS); i++) {
        tss_ptr[i] = 0;
    }
    
    // Set up the essential fields
    tss.ss0 = 0x10;  // Kernel data segment selector
    tss.esp0 = 0;    // Will be set when switching processes
    tss.iomap_base = sizeof(TSS);  // No I/O permission bitmap
    
    // Note: We need to add the TSS to the GDT and load it
    // For now, we'll just initialize the structure
    // The GDT setup will be done when we modify the existing GDT code
}

void TSSManager::set_kernel_stack(u32 kernel_stack_top) {
    tss.esp0 = kernel_stack_top;
}

} // namespace kira::system 