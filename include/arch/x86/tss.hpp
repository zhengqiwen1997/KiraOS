#pragma once

#include "core/types.hpp"

namespace kira::system {

/**
 * @brief Task State Segment (TSS) structure for x86
 * 
 * The TSS is used by the CPU to find the kernel stack when switching
 * from user mode (Ring 3) to kernel mode (Ring 0) during interrupts
 * or system calls.
 */
struct TSS {
    u32 prevTss;   // Previous TSS (unused in our implementation)
    u32 esp0;       // Stack pointer for Ring 0 (kernel stack)
    u32 ss0;        // Stack segment for Ring 0 (kernel data segment)
    u32 esp1;       // Stack pointer for Ring 1 (unused)
    u32 ss1;        // Stack segment for Ring 1 (unused)
    u32 esp2;       // Stack pointer for Ring 2 (unused)
    u32 ss2;        // Stack segment for Ring 2 (unused)
    u32 cr3;        // Page directory base (for paging, unused for now)
    u32 eip;        // Instruction pointer (unused in our implementation)
    u32 eflags;     // Flags register (unused in our implementation)
    u32 eax, ecx, edx, ebx;  // General purpose registers (unused)
    u32 esp, ebp, esi, edi;  // More registers (unused)
    u32 es, cs, ss, ds, fs, gs;  // Segment registers (unused)
    u32 ldt;        // Local Descriptor Table selector (unused)
    u16 trap;       // Debug trap flag (unused)
    u16 iomapBase; // I/O permission bitmap base (unused)
} __attribute__((packed));

/**
 * @brief TSS Manager class
 * 
 * Manages the Task State Segment for user mode support.
 * In our simple implementation, we use a single TSS for all processes.
 */
class TSSManager {
public:
    /**
     * @brief Initialize the TSS
     */
    static void initialize();
    
    /**
     * @brief Set the kernel stack for the current process
     * @param kernelStackTop Top of the kernel stack for this process
     */
    static void set_kernel_stack(u32 kernelStackTop);
    
    /**
     * @brief Get the TSS instance
     */
    static TSS& get_tss() { return tss; }

private:
    static TSS tss;
    static constexpr u16 TSS_SELECTOR = 0x28;  // GDT selector for TSS
};

} // namespace kira::system 