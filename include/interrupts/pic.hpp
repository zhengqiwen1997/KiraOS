#pragma once

#include "core/types.hpp"

namespace kira::system {

/**
 * @brief Programmable Interrupt Controller (8259 PIC) Driver
 * 
 * Manages hardware interrupts from external devices.
 * The PC has two 8259 PIC chips (master and slave) that handle 16 IRQ lines.
 */
class PIC {
public:
    // PIC I/O port addresses
    static constexpr u16 MASTER_PIC_COMMAND = 0x20;
    static constexpr u16 MASTER_PIC_DATA    = 0x21;
    static constexpr u16 SLAVE_PIC_COMMAND  = 0xA0;
    static constexpr u16 SLAVE_PIC_DATA     = 0xA1;
    
    // PIC commands
    static constexpr u8 PIC_EOI = 0x20;  // End of Interrupt
    
    // ICW1 (Initialization Command Word 1)
    static constexpr u8 ICW1_ICW4       = 0x01;  // ICW4 (not) needed
    static constexpr u8 ICW1_SINGLE     = 0x02;  // Single (cascade) mode
    static constexpr u8 ICW1_INTERVAL4  = 0x04;  // Call address interval 4 (8)
    static constexpr u8 ICW1_LEVEL      = 0x08;  // Level triggered (edge) mode
    static constexpr u8 ICW1_INIT       = 0x10;  // Initialization - required!
    
    // ICW4
    static constexpr u8 ICW4_8086       = 0x01;  // 8086/88 (MCS-80/85) mode
    static constexpr u8 ICW4_AUTO       = 0x02;  // Auto (normal) EOI
    static constexpr u8 ICW4_BUF_SLAVE  = 0x08;  // Buffered mode/slave
    static constexpr u8 ICW4_BUF_MASTER = 0x0C;  // Buffered mode/master
    static constexpr u8 ICW4_SFNM       = 0x10;  // Special fully nested (not)
    
    // IRQ numbers
    static constexpr u8 IRQ_TIMER    = 0;   // System timer
    static constexpr u8 IRQ_KEYBOARD = 1;   // Keyboard
    static constexpr u8 IRQ_CASCADE  = 2;   // Cascade (never raised)
    static constexpr u8 IRQ_COM2     = 3;   // COM2
    static constexpr u8 IRQ_COM1     = 4;   // COM1
    static constexpr u8 IRQ_LPT2     = 5;   // LPT2
    static constexpr u8 IRQ_FLOPPY   = 6;   // Floppy disk
    static constexpr u8 IRQ_LPT1     = 7;   // LPT1
    static constexpr u8 IRQ_RTC      = 8;   // Real-time clock
    static constexpr u8 IRQ_FREE1    = 9;   // Free for peripherals
    static constexpr u8 IRQ_FREE2    = 10;  // Free for peripherals
    static constexpr u8 IRQ_FREE3    = 11;  // Free for peripherals
    static constexpr u8 IRQ_MOUSE    = 12;  // PS/2 mouse
    static constexpr u8 IRQ_FPU      = 13;  // FPU / Coprocessor
    static constexpr u8 IRQ_ATA1     = 14;  // Primary ATA hard disk
    static constexpr u8 IRQ_ATA2     = 15;  // Secondary ATA hard disk
    
    // Remapped interrupt vectors (IRQ 0-15 -> INT 32-47)
    static constexpr u8 IRQ_BASE = 32;
    
public:
    /**
     * @brief Initialize and remap the PIC
     * 
     * Remaps IRQ 0-15 to interrupts 32-47 to avoid conflicts with CPU exceptions.
     * This is essential because by default, IRQs 0-7 map to interrupts 8-15,
     * which conflict with CPU exception numbers.
     */
    static void initialize();
    
    /**
     * @brief Send End of Interrupt (EOI) signal
     * @param irq IRQ number (0-15)
     * 
     * Must be called at the end of every IRQ handler to signal that
     * the interrupt has been processed.
     */
    static void send_eoi(u8 irq);
    
    /**
     * @brief Enable a specific IRQ line
     * @param irq IRQ number (0-15)
     */
    static void enable_irq(u8 irq);
    
    /**
     * @brief Disable a specific IRQ line
     * @param irq IRQ number (0-15)
     */
    static void disable_irq(u8 irq);
    
    /**
     * @brief Get the interrupt vector for an IRQ
     * @param irq IRQ number (0-15)
     * @return Interrupt vector number (32-47)
     */
    static constexpr u8 irq_to_interrupt(u8 irq) {
        return IRQ_BASE + irq;
    }
    
    /**
     * @brief Get the IRQ number from an interrupt vector
     * @param interrupt Interrupt vector (32-47)
     * @return IRQ number (0-15) or 0xFF if not a hardware interrupt
     */
    static constexpr u8 interrupt_to_irq(u8 interrupt) {
        if (interrupt >= IRQ_BASE && interrupt < IRQ_BASE + 16) {
            return interrupt - IRQ_BASE;
        }
        return 0xFF; // Not a hardware interrupt
    }
    
    /**
     * @brief Check if all IRQs are masked (disabled)
     * @return true if all IRQs are disabled
     */
    static bool all_irqs_masked();
    
    /**
     * @brief Get current IRQ mask for debugging
     * @return 16-bit mask (bit 0 = IRQ 0, bit 1 = IRQ 1, etc.)
     */
    static u16 get_irq_mask();

private:
    /**
     * @brief Remap PIC to new interrupt vectors
     * @param master_offset New interrupt base for master PIC (IRQ 0-7)
     * @param slave_offset New interrupt base for slave PIC (IRQ 8-15)
     */
    static void remap(u8 master_offset, u8 slave_offset);
};

} // namespace kira::system 