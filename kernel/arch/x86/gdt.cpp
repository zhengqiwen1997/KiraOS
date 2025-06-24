#include "arch/x86/gdt.hpp"
#include "arch/x86/tss.hpp"

namespace kira::system {

// Static GDT and descriptor
GDTEntry GDTManager::gdt[GDT_ENTRIES];
GDTDescriptor GDTManager::gdt_descriptor;

void GDTManager::initialize() {
    // Set up GDT entries
    set_gdt_entry(0, 0, 0, 0, 0);  // Null segment
    
    // Kernel segments (Ring 0)
    set_gdt_entry(1, 0, 0xFFFFFFFF, GDTAccess::KERNEL_CODE, GDTGranularity::STANDARD);
    set_gdt_entry(2, 0, 0xFFFFFFFF, GDTAccess::KERNEL_DATA, GDTGranularity::STANDARD);
    
    // User segments (Ring 3)
    set_gdt_entry(3, 0, 0xFFFFFFFF, GDTAccess::USER_CODE, GDTGranularity::STANDARD);
    set_gdt_entry(4, 0, 0xFFFFFFFF, GDTAccess::USER_DATA, GDTGranularity::STANDARD);
    
    // TSS segment
    TSS& tss = TSSManager::get_tss();
    set_gdt_entry(5, reinterpret_cast<u32>(&tss), sizeof(TSS) - 1, 
                  GDTAccess::TSS_ACCESS, 0x00);
    
    // Load the new GDT
    load_gdt();
    
    // Load the TSS
    asm volatile (
        "mov $0x28, %%ax\n\t"   // TSS selector
        "ltr %%ax"              // Load task register
        :
        :
        : "eax"
    );
}

void GDTManager::set_gdt_entry(u32 index, u32 base, u32 limit, u8 access, u8 granularity) {
    if (index >= GDT_ENTRIES) return;
    
    GDTEntry& entry = gdt[index];
    
    // Base address
    entry.base_low = base & 0xFFFF;
    entry.base_middle = (base >> 16) & 0xFF;
    entry.base_high = (base >> 24) & 0xFF;
    
    // Limit
    entry.limit_low = limit & 0xFFFF;
    entry.granularity = (limit >> 16) & 0x0F;
    
    // Granularity flags
    entry.granularity |= granularity & 0xF0;
    
    // Access byte
    entry.access = access;
}

void GDTManager::load_gdt() {
    // Set up GDT descriptor
    gdt_descriptor.limit = (sizeof(GDTEntry) * GDT_ENTRIES) - 1;
    gdt_descriptor.base = reinterpret_cast<u32>(&gdt);
    
    // Load GDT using inline assembly
    asm volatile (
        "lgdt %0\n\t"           // Load GDT
        "mov $0x10, %%ax\n\t"   // Load kernel data segment
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"  // Far jump to reload CS
        "1:\n\t"
        :
        : "m"(gdt_descriptor)
        : "eax"
    );
}

} // namespace kira::system 