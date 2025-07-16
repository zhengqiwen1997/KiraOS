#pragma once

#include "core/types.hpp"

namespace kira::system {

// IDT entry structure (8 bytes per entry)
struct IDTEntry {
    u16 offsetLow;     // Lower 16 bits of handler address
    u16 selector;       // Code segment selector
    u8  reserved;       // Always 0
    u8  typeAttr;      // Type and attributes
    u16 offsetHigh;    // Upper 16 bits of handler address
} __attribute__((packed));

// IDT descriptor for LIDT instruction
struct IDTDescriptor {
    u16 limit;          // Size of IDT - 1
    u32 base;           // Base address of IDT
} __attribute__((packed));

// IDT Configuration Constants
constexpr u32 IDT_SIZE = 256;           // 256 interrupt vectors
constexpr u32 IDT_ENTRY_SIZE = 8;       // Each entry is 8 bytes

// IDT Entry Flags
constexpr u8 IDT_PRESENT = 0x80;        // Present bit
constexpr u8 IDT_RING0 = 0x00;          // Ring 0 (kernel)
constexpr u8 IDT_RING3 = 0x60;          // Ring 3 (user)
constexpr u8 IDT_INTERRUPT_GATE = 0x0E; // 32-bit interrupt gate
constexpr u8 IDT_TRAP_GATE = 0x0F;      // 32-bit trap gate

// CPU Exception Numbers (0-31)
constexpr u8 INT_DIVIDE_ERROR = 0;
constexpr u8 INT_DEBUG = 1;
constexpr u8 INT_NMI = 2;
constexpr u8 INT_BREAKPOINT = 3;
constexpr u8 INT_OVERFLOW = 4;
constexpr u8 INT_BOUND_RANGE = 5;
constexpr u8 INT_INVALID_OPCODE = 6;
constexpr u8 INT_DEVICE_NOT_AVAILABLE = 7;
constexpr u8 INT_DOUBLE_FAULT = 8;
constexpr u8 INT_COPROCESSOR_SEGMENT_OVERRUN = 9;  // Legacy, not used on modern CPUs
constexpr u8 INT_INVALID_TSS = 10;
constexpr u8 INT_SEGMENT_NOT_PRESENT = 11;
constexpr u8 INT_STACK_FAULT = 12;
constexpr u8 INT_GENERAL_PROTECTION = 13;
constexpr u8 INT_PAGE_FAULT = 14;
// 15 is reserved
constexpr u8 INT_X87_FPU_ERROR = 16;
constexpr u8 INT_ALIGNMENT_CHECK = 17;
constexpr u8 INT_MACHINE_CHECK = 18;
constexpr u8 INT_SIMD_FPU_ERROR = 19;
constexpr u8 INT_VIRTUALIZATION_ERROR = 20;
constexpr u8 INT_CONTROL_PROTECTION_ERROR = 21;
// 22-31 are reserved

// Hardware Interrupt Numbers (32-47)
constexpr u8 IRQ0_TIMER = 32;
constexpr u8 IRQ1_KEYBOARD = 33;

/**
 * @brief IDT (Interrupt Descriptor Table) Manager
 * 
 * Manages the interrupt descriptor table for handling interrupts and exceptions.
 * Provides a clean interface for setting up interrupt handlers.
 */
class IDT {
private:
    static IDTEntry idtTable[IDT_SIZE];
    static IDTDescriptor idtDescriptor;
    
public:
    /**
     * @brief Initialize the IDT
     * Sets up the IDT with default handlers and loads it into the CPU
     */
    static void initialize();
    
    /**
     * @brief Set an interrupt handler
     * @param interruptNumber Interrupt/exception number (0-255)
     * @param handler Pointer to handler function
     * @param selector Code segment selector (usually 0x08)
     * @param typeAttr Type and attribute flags
     */
    static void set_handler(u8 interruptNumber, void* handler, u16 selector, u8 typeAttr);
    
    /**
     * @brief Set a standard interrupt gate
     * @param interruptNumber Interrupt number
     * @param handler Handler function pointer
     */
    static void set_interrupt_gate(u8 interruptNumber, void* handler);
    
    /**
     * @brief Set a user-accessible interrupt gate (Ring 3)
     * @param interruptNumber Interrupt number
     * @param handler Handler function pointer
     */
    static void set_user_interrupt_gate(u8 interruptNumber, void* handler);
    
    /**
     * @brief Set a trap gate (doesn't disable interrupts)
     * @param interruptNumber Interrupt number  
     * @param handler Handler function pointer
     */
    static void set_trap_gate(u8 interruptNumber, void* handler);
    
    /**
     * @brief Load the IDT into the CPU
     */
    static void load();
    
    /**
     * @brief Get IDT entry for debugging
     * @param interruptNumber Interrupt number
     * @return Pointer to IDT entry or nullptr if invalid
     */
    static const IDTEntry* get_entry(u32 interruptNumber);
};

} // namespace kira::system 