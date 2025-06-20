#include "arch/x86/idt.hpp"
#include "core/io.hpp"
#include "display/vga.hpp"

namespace kira::system {

// Static member definitions
IDTEntry IDT::idt_table[IDT_SIZE];
IDTDescriptor IDT::idt_descriptor;

void IDT::initialize() {
    // Clear the IDT table
    for (u32 i = 0; i < IDT_SIZE; i++) {
        idt_table[i].offset_low = 0;
        idt_table[i].selector = 0;
        idt_table[i].reserved = 0;
        idt_table[i].type_attr = 0;
        idt_table[i].offset_high = 0;
    }
    
    // Set up IDT descriptor
    idt_descriptor.limit = (IDT_SIZE * IDT_ENTRY_SIZE) - 1;
    idt_descriptor.base = (u32)&idt_table[0];
    
    // Load the IDT
    load();
}

void IDT::set_handler(u8 interrupt_number, void* handler, u16 selector, u8 type_attr) {
    if (interrupt_number >= IDT_SIZE) {
        return; // Invalid interrupt number
    }
    
    u32 handler_addr = (u32)handler;
    
    idt_table[interrupt_number].offset_low = handler_addr & 0xFFFF;
    idt_table[interrupt_number].offset_high = (handler_addr >> 16) & 0xFFFF;
    idt_table[interrupt_number].selector = selector;
    idt_table[interrupt_number].reserved = 0;
    idt_table[interrupt_number].type_attr = type_attr;
}

void IDT::set_interrupt_gate(u8 interrupt_number, void* handler) {
    u8 type_attr = IDT_PRESENT | IDT_RING0 | IDT_INTERRUPT_GATE;
    set_handler(interrupt_number, handler, 0x08, type_attr); // 0x08 = kernel code segment
}

void IDT::set_trap_gate(u8 interrupt_number, void* handler) {
    u8 type_attr = IDT_PRESENT | IDT_RING0 | IDT_TRAP_GATE;
    set_handler(interrupt_number, handler, 0x08, type_attr);
}

void IDT::load() {
    // Load IDT using LIDT instruction
    asm volatile("lidt %0" : : "m"(idt_descriptor));
}

const IDTEntry* IDT::get_entry(u8 interrupt_number) {
    if (interrupt_number >= IDT_SIZE) {
        return nullptr;
    }
    return &idt_table[interrupt_number];
}

} // namespace kira::system 