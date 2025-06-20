#include "system/pic.hpp"
#include "system/io.hpp"

namespace kira::system {

void PIC::initialize() {
    // Save current IRQ masks
    u8 master_mask = inb(MASTER_PIC_DATA);
    u8 slave_mask = inb(SLAVE_PIC_DATA);
    
    // Remap PIC to interrupts 32-47
    remap(IRQ_BASE, IRQ_BASE + 8);
    
    // Enable timer (IRQ 0), keyboard (IRQ 1), and cascade (IRQ 2)
    // Mask all others initially
    outb(MASTER_PIC_DATA, 0xF8);  // Enable IRQ 0,1,2 - mask others (11111000)
    outb(SLAVE_PIC_DATA, 0xFF);   // Mask all slave IRQs initially
}

void PIC::remap(u8 master_offset, u8 slave_offset) {
    // Start initialization sequence (ICW1)
    outb(MASTER_PIC_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(SLAVE_PIC_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // Set interrupt vector offsets (ICW2)
    outb(MASTER_PIC_DATA, master_offset);  // Master PIC offset
    io_wait();
    outb(SLAVE_PIC_DATA, slave_offset);    // Slave PIC offset
    io_wait();
    
    // Set up cascade connection (ICW3)
    outb(MASTER_PIC_DATA, 4);  // Tell master PIC that slave is at IRQ2 (0000 0100)
    io_wait();
    outb(SLAVE_PIC_DATA, 2);   // Tell slave PIC its cascade identity (0000 0010)
    io_wait();
    
    // Set mode (ICW4)
    outb(MASTER_PIC_DATA, ICW4_8086);  // 8086 mode
    io_wait();
    outb(SLAVE_PIC_DATA, ICW4_8086);   // 8086 mode
    io_wait();
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
    u8 master_mask = inb(MASTER_PIC_DATA);
    u8 slave_mask = inb(SLAVE_PIC_DATA);
    
    // All IRQs are masked if both masks are 0xFF
    return (master_mask == 0xFF) && (slave_mask == 0xFF);
}

u16 PIC::get_irq_mask() {
    u8 master_mask = inb(MASTER_PIC_DATA);
    u8 slave_mask = inb(SLAVE_PIC_DATA);
    
    // Combine masks: low 8 bits = master, high 8 bits = slave
    return (static_cast<u16>(slave_mask) << 8) | master_mask;
}

} // namespace kira::system 