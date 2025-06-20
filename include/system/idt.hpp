#pragma once

#include "types.hpp"

namespace kira::system {

// IDT entry structure (8 bytes per entry)
struct IDTEntry {
    u16 offset_low;     // Lower 16 bits of handler address
    u16 selector;       // Code segment selector
    u8  reserved;       // Always 0
    u8  type_attr;      // Type and attributes
    u16 offset_high;    // Upper 16 bits of handler address
} __attribute__((packed));

// IDT descriptor for LIDT instruction
struct IDTDescriptor {
    u16 limit;          // Size of IDT - 1
    u32 base;           // Base address of IDT
} __attribute__((packed));

// IDT constants
const u32 IDT_SIZE = 256;           // 256 interrupt vectors
const u32 IDT_ENTRY_SIZE = 8;       // Each entry is 8 bytes

// IDT entry type attributes
const u8 IDT_PRESENT = 0x80;        // Present bit
const u8 IDT_RING0 = 0x00;          // Ring 0 (kernel)
const u8 IDT_RING3 = 0x60;          // Ring 3 (user)
const u8 IDT_INTERRUPT_GATE = 0x0E; // 32-bit interrupt gate
const u8 IDT_TRAP_GATE = 0x0F;      // 32-bit trap gate

// Standard interrupt/exception numbers
const u8 INT_DIVIDE_ERROR = 0;
const u8 INT_DEBUG = 1;
const u8 INT_NMI = 2;
const u8 INT_BREAKPOINT = 3;
const u8 INT_OVERFLOW = 4;
const u8 INT_BOUND_RANGE = 5;
const u8 INT_INVALID_OPCODE = 6;
const u8 INT_DEVICE_NOT_AVAILABLE = 7;
const u8 INT_DOUBLE_FAULT = 8;
const u8 INT_COPROCESSOR_SEGMENT_OVERRUN = 9;  // Legacy, not used on modern CPUs
const u8 INT_INVALID_TSS = 10;
const u8 INT_SEGMENT_NOT_PRESENT = 11;
const u8 INT_STACK_FAULT = 12;
const u8 INT_GENERAL_PROTECTION = 13;
const u8 INT_PAGE_FAULT = 14;
// 15 is reserved
const u8 INT_X87_FPU_ERROR = 16;
const u8 INT_ALIGNMENT_CHECK = 17;
const u8 INT_MACHINE_CHECK = 18;
const u8 INT_SIMD_FPU_ERROR = 19;
const u8 INT_VIRTUALIZATION_ERROR = 20;
const u8 INT_CONTROL_PROTECTION_ERROR = 21;
// 22-31 are reserved

// Hardware interrupts (after PIC remapping)
const u8 IRQ0_TIMER = 32;
const u8 IRQ1_KEYBOARD = 33;

/**
 * @brief IDT (Interrupt Descriptor Table) Manager
 * 
 * Manages the interrupt descriptor table for handling interrupts and exceptions.
 * Provides a clean interface for setting up interrupt handlers.
 */
class IDT {
private:
    static IDTEntry idt_table[IDT_SIZE];
    static IDTDescriptor idt_descriptor;
    
public:
    /**
     * @brief Initialize the IDT
     * Sets up the IDT with default handlers and loads it into the CPU
     */
    static void initialize();
    
    /**
     * @brief Set an interrupt handler
     * @param interrupt_number Interrupt/exception number (0-255)
     * @param handler Pointer to handler function
     * @param selector Code segment selector (usually 0x08)
     * @param type_attr Type and attribute flags
     */
    static void set_handler(u8 interrupt_number, void* handler, u16 selector, u8 type_attr);
    
    /**
     * @brief Set a standard interrupt gate
     * @param interrupt_number Interrupt number
     * @param handler Handler function pointer
     */
    static void set_interrupt_gate(u8 interrupt_number, void* handler);
    
    /**
     * @brief Set a trap gate (doesn't disable interrupts)
     * @param interrupt_number Interrupt number  
     * @param handler Handler function pointer
     */
    static void set_trap_gate(u8 interrupt_number, void* handler);
    
    /**
     * @brief Load the IDT into the CPU
     */
    static void load();
    
    /**
     * @brief Get IDT entry for debugging
     * @param interrupt_number Interrupt number
     * @return Pointer to IDT entry or nullptr if invalid
     */
    static const IDTEntry* get_entry(u8 interrupt_number);
};

} // namespace kira::system 