#pragma once

#include "core/types.hpp"

namespace kira::system {

/**
 * @brief IRQ (Interrupt Request) frame structure
 * 
 * Represents the CPU state when a hardware interrupt occurs.
 * This structure is created by the assembly interrupt stubs.
 */
struct IRQFrame {
    // Registers saved by pusha (in reverse order)
    u32 edi, esi, ebp, espDummy, ebx, edx, ecx, eax;
    
    // Interrupt information pushed by our stub
    u32 interruptNumber;
    u32 errorCode;  // Always 0 for IRQs
    
    // CPU state saved automatically by processor
    u32 eip, cs, eflags, esp, ss;
} __attribute__((packed));

/**
 * @brief IRQ handler function type
 * 
 * All IRQ handlers must match this signature.
 * @param frame Pointer to IRQ frame containing CPU state
 */
using IRQHandler = void(*)(IRQFrame* frame);

/**
 * @brief IRQ Management System
 * 
 * Provides a framework for handling hardware interrupts and
 * registering device-specific interrupt handlers.
 */
namespace irq {

/**
 * @brief Initialize the IRQ system
 * 
 * Sets up the PIC, installs default handlers, and prepares
 * the system for hardware interrupts.
 */
void initialize();

/**
 * @brief Register a custom IRQ handler
 * @param irqNumber IRQ number (0-15)
 * @param handler Pointer to handler function
 * @return true if successful, false if invalid IRQ number
 */
bool register_handler(u8 irqNumber, IRQHandler handler);

/**
 * @brief Unregister an IRQ handler (restore default)
 * @param irqNumber IRQ number (0-15)
 * @return true if successful, false if invalid IRQ number
 */
bool unregister_handler(u8 irqNumber);

/**
 * @brief Enable a specific IRQ
 * @param irqNumber IRQ number (0-15)
 * @return true if successful, false if invalid IRQ number
 */
bool enable_irq(u8 irqNumber);

/**
 * @brief Disable a specific IRQ
 * @param irqNumber IRQ number (0-15)
 * @return true if successful, false if invalid IRQ number
 */
bool disable_irq(u8 irqNumber);

/**
 * @brief Check if an IRQ is enabled
 * @param irqNumber IRQ number (0-15)
 * @return true if enabled, false if disabled or invalid
 */
bool is_irq_enabled(u8 irqNumber);

/**
 * @brief Get IRQ statistics for debugging
 * @param irqNumber IRQ number (0-15)
 * @return Number of times this IRQ has been triggered
 */
u32 get_irq_count(u8 irqNumber);

/**
 * @brief Default IRQ handler (called by assembly stub)
 * @param frame Pointer to IRQ frame
 * 
 * This is the main entry point for all hardware interrupts.
 * It dispatches to the appropriate registered handler.
 */
void default_handler(IRQFrame* frame);

/**
 * @brief Print IRQ statistics to VGA display
 * 
 * Useful for debugging interrupt activity.
 */
void print_statistics();

// Specific IRQ handlers
namespace handlers {

/**
 * @brief Timer interrupt handler (IRQ 0)
 * @param frame IRQ frame
 */
void timer_handler(IRQFrame* frame);

/**
 * @brief Keyboard interrupt handler (IRQ 1)
 * @param frame IRQ frame
 */
void keyboard_handler(IRQFrame* frame);

/**
 * @brief Generic handler for unhandled IRQs
 * @param frame IRQ frame
 */
void unhandled_irq(IRQFrame* frame);

} // namespace handlers

} // namespace irq
} // namespace kira::system

// Assembly stub declarations
extern "C" {
    void irq_stub_0();
    void irq_stub_1();
    void irq_stub_2();
    void irq_stub_3();
    void irq_stub_4();
    void irq_stub_5();
    void irq_stub_6();
    void irq_stub_7();
    void irq_stub_8();
    void irq_stub_9();
    void irq_stub_10();
    void irq_stub_11();
    void irq_stub_12();
    void irq_stub_13();
    void irq_stub_14();
    void irq_stub_15();
} 