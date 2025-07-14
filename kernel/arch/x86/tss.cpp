#include "arch/x86/tss.hpp"

namespace kira::system {

// Static TSS instance
TSS TSSManager::tss;

void TSSManager::initialize() {
    // Clear the TSS structure
    u8* tssPtr = reinterpret_cast<u8*>(&tss);
    for (u32 i = 0; i < sizeof(TSS); i++) {
        tssPtr[i] = 0;
    }
    
    // Set up the essential fields
    tss.ss0 = 0x10;  // Kernel data segment selector
    tss.esp0 = 0;    // Will be set when switching processes
    tss.iomapBase = sizeof(TSS);  // No I/O permission bitmap
    
    // Note: We need to add the TSS to the GDT and load it
    // For now, we'll just initialize the structure
    // The GDT setup will be done when we modify the existing GDT code
}

void TSSManager::set_kernel_stack(u32 kernelStackTop) {
    tss.esp0 = kernelStackTop;
}

} // namespace kira::system 