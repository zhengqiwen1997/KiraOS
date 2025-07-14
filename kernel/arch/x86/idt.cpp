#include "arch/x86/idt.hpp"

namespace kira::system {

// Static member definitions
IDTEntry IDT::idtTable[IDT_SIZE];
IDTDescriptor IDT::idtDescriptor;

void IDT::initialize() {
    // Clear the IDT table
    for (u32 i = 0; i < IDT_SIZE; i++) {
        idtTable[i].offsetLow = 0;
        idtTable[i].selector = 0;
        idtTable[i].reserved = 0;
        idtTable[i].typeAttr = 0;
        idtTable[i].offsetHigh = 0;
    }
    
    // Set up IDT descriptor
    idtDescriptor.limit = (IDT_SIZE * IDT_ENTRY_SIZE) - 1;
    idtDescriptor.base = (u32)&idtTable[0];
    
    // Load the IDT
    load();
}

void IDT::set_handler(u8 interruptNumber, void* handler, u16 selector, u8 typeAttr) {
    if (interruptNumber >= IDT_SIZE) {
        return; // Invalid interrupt number
    }
    
    u32 handlerAddr = (u32)handler;
    
    idtTable[interruptNumber].offsetLow = handlerAddr & 0xFFFF;
    idtTable[interruptNumber].offsetHigh = (handlerAddr >> 16) & 0xFFFF;
    idtTable[interruptNumber].selector = selector;
    idtTable[interruptNumber].reserved = 0;
    idtTable[interruptNumber].typeAttr = typeAttr;
}

void IDT::set_interrupt_gate(u8 interruptNumber, void* handler) {
    u8 typeAttr = IDT_PRESENT | IDT_RING0 | IDT_INTERRUPT_GATE;
    set_handler(interruptNumber, handler, 0x08, typeAttr); // 0x08 = kernel code segment
}

void IDT::set_user_interrupt_gate(u8 interruptNumber, void* handler) {
    u8 typeAttr = IDT_PRESENT | IDT_RING3 | IDT_INTERRUPT_GATE;
    set_handler(interruptNumber, handler, 0x08, typeAttr); // 0x08 = kernel code segment
}

void IDT::set_trap_gate(u8 interruptNumber, void* handler) {
    u8 typeAttr = IDT_PRESENT | IDT_RING0 | IDT_TRAP_GATE;
    set_handler(interruptNumber, handler, 0x08, typeAttr);
}

void IDT::load() {
    // Load IDT using LIDT instruction
    asm volatile("lidt %0" : : "m"(idtDescriptor));
}

const IDTEntry* IDT::get_entry(u8 interruptNumber) {
    if (interruptNumber >= IDT_SIZE) {
        return nullptr;
    }
    return &idtTable[interruptNumber];
}

} // namespace kira::system 