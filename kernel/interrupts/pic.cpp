#include "interrupts/pic.hpp"
#include "core/io.hpp"

namespace kira::system {

void PIC::initialize() {
    // Save current IRQ masks
    u8 masterMask = inb(MASTER_PIC_DATA);
    u8 slaveMask = inb(SLAVE_PIC_DATA);
    
    // Remap PIC to interrupts 32-47
    remap(IRQ_BASE, IRQ_BASE + 8);
    
    // Enable timer (IRQ 0), keyboard (IRQ 1), and cascade (IRQ 2)
    // Mask all others initially
    outb(MASTER_PIC_DATA, 0xF8);  // Enable IRQ 0,1,2 - mask others (11111000)
    outb(SLAVE_PIC_DATA, 0xFF);   // Mask all slave IRQs initially
}

void PIC::remap(u8 masterOffset, u8 slaveOffset) {
    // Save current masks
    u8 masterMask = inb(MASTER_PIC_DATA);
    u8 slaveMask = inb(SLAVE_PIC_DATA);
    
    // Start initialization sequence
    outb(MASTER_PIC_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(SLAVE_PIC_COMMAND, ICW1_INIT | ICW1_ICW4);
    
    // Set interrupt vector offsets
    outb(MASTER_PIC_DATA, masterOffset);    // Master PIC offset
    outb(SLAVE_PIC_DATA, slaveOffset);      // Slave PIC offset
    
    // Configure cascade
    outb(MASTER_PIC_DATA, 4);               // IRQ2 has slave
    outb(SLAVE_PIC_DATA, 2);                // Slave cascade identity
    
    // Set mode
    outb(MASTER_PIC_DATA, ICW4_8086);
    outb(SLAVE_PIC_DATA, ICW4_8086);
    
    // Restore masks
    outb(MASTER_PIC_DATA, masterMask);
    outb(SLAVE_PIC_DATA, slaveMask);
}

void PIC::send_eoi(u8 irq) {
    // If IRQ is from slave PIC (IRQ 8-15), send EOI to both
    if (irq >= 8) {
        outb(SLAVE_PIC_COMMAND, PIC_EOI);
    }
    
    // Always send EOI to master PIC
    outb(MASTER_PIC_COMMAND, PIC_EOI);
}

void PIC::enable_irq(u8 irq) {
    u16 port;
    
    if (irq < 8) {
        // Master PIC
        port = MASTER_PIC_DATA;
    } else {
        // Slave PIC
        port = SLAVE_PIC_DATA;
        irq -= 8;
    }
    
    // Clear the bit to enable the IRQ
    u8 mask = inb(port);
    mask &= ~(1 << irq);
    outb(port, mask);
}

void PIC::disable_irq(u8 irq) {
    u16 port;
    
    if (irq < 8) {
        // Master PIC
        port = MASTER_PIC_DATA;
    } else {
        // Slave PIC
        port = SLAVE_PIC_DATA;
        irq -= 8;
    }
    
    // Set the bit to disable the IRQ
    u8 mask = inb(port);
    mask |= (1 << irq);
    outb(port, mask);
}

bool PIC::all_irqs_masked() {
    u8 masterMask = inb(MASTER_PIC_DATA);
    u8 slaveMask = inb(SLAVE_PIC_DATA);
    
    // All IRQs are masked if both masks are 0xFF
    return (masterMask == 0xFF) && (slaveMask == 0xFF);
}

u16 PIC::get_irq_mask() {
    u8 masterMask = inb(MASTER_PIC_DATA);
    u8 slaveMask = inb(SLAVE_PIC_DATA);
    
    // Combine masks: low 8 bits = master, high 8 bits = slave
    return (static_cast<u16>(slaveMask) << 8) | masterMask;
}

} // namespace kira::system 