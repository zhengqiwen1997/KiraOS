#include "system/exceptions.hpp"
#include "system/idt.hpp"
#include "display/vga_display.hpp"
#include "system/io.hpp"

namespace kira::system {

using namespace kira::display;

void Exceptions::initialize() {
    // Set up basic exception handlers
    IDT::set_interrupt_gate(INT_DIVIDE_ERROR, (void*)exception_stub_0);
    IDT::set_interrupt_gate(INT_DEBUG, (void*)exception_stub_1);
    IDT::set_interrupt_gate(INT_NMI, (void*)exception_stub_2);
    IDT::set_trap_gate(INT_BREAKPOINT, (void*)exception_stub_3);  // Trap gate for debugging
    IDT::set_trap_gate(INT_OVERFLOW, (void*)exception_stub_4);
    IDT::set_interrupt_gate(INT_BOUND_RANGE, (void*)exception_stub_5);
    IDT::set_interrupt_gate(INT_INVALID_OPCODE, (void*)exception_stub_6);
    IDT::set_interrupt_gate(INT_DEVICE_NOT_AVAILABLE, (void*)exception_stub_7);
    IDT::set_interrupt_gate(INT_DOUBLE_FAULT, (void*)exception_stub_8);
    IDT::set_interrupt_gate(INT_INVALID_TSS, (void*)exception_stub_10);
    IDT::set_interrupt_gate(INT_SEGMENT_NOT_PRESENT, (void*)exception_stub_11);
    IDT::set_interrupt_gate(INT_STACK_FAULT, (void*)exception_stub_12);
    IDT::set_interrupt_gate(INT_GENERAL_PROTECTION, (void*)exception_stub_13);
    IDT::set_interrupt_gate(INT_PAGE_FAULT, (void*)exception_stub_14);
}

void Exceptions::default_handler(ExceptionFrame* frame) {
    VGADisplay vga;
    
    // Clear screen and show error
    vga.clear_screen(VGA_RED_ON_BLUE);
    
    vga.print_string(0, 0, "*** KERNEL PANIC - UNHANDLED EXCEPTION ***", VGA_WHITE_ON_BLUE);
    vga.print_string(2, 0, "Exception: ", VGA_YELLOW_ON_BLUE);
    vga.print_string(2, 11, get_exception_name(frame->interrupt_number), VGA_WHITE_ON_BLUE);
    
    vga.print_string(3, 0, "Number: ", VGA_YELLOW_ON_BLUE);
    vga.print_decimal(3, 8, frame->interrupt_number, VGA_WHITE_ON_BLUE);
    
    vga.print_string(4, 0, "Error Code: ", VGA_YELLOW_ON_BLUE);
    vga.print_hex(4, 12, frame->error_code, VGA_WHITE_ON_BLUE);
    
    vga.print_string(6, 0, "Registers:", VGA_YELLOW_ON_BLUE);
    vga.print_string(7, 0, "EAX: ", VGA_CYAN_ON_BLUE);
    vga.print_hex(7, 5, frame->eax, VGA_WHITE_ON_BLUE);
    vga.print_string(7, 20, "EBX: ", VGA_CYAN_ON_BLUE);
    vga.print_hex(7, 25, frame->ebx, VGA_WHITE_ON_BLUE);
    
    vga.print_string(8, 0, "ECX: ", VGA_CYAN_ON_BLUE);
    vga.print_hex(8, 5, frame->ecx, VGA_WHITE_ON_BLUE);
    vga.print_string(8, 20, "EDX: ", VGA_CYAN_ON_BLUE);
    vga.print_hex(8, 25, frame->edx, VGA_WHITE_ON_BLUE);
    
    vga.print_string(10, 0, "EIP: ", VGA_YELLOW_ON_BLUE);
    vga.print_hex(10, 5, frame->eip, VGA_WHITE_ON_BLUE);
    vga.print_string(10, 20, "CS: ", VGA_YELLOW_ON_BLUE);
    vga.print_hex(10, 24, frame->cs, VGA_WHITE_ON_BLUE);
    
    vga.print_string(11, 0, "EFLAGS: ", VGA_YELLOW_ON_BLUE);
    vga.print_hex(11, 8, frame->eflags, VGA_WHITE_ON_BLUE);
    
    vga.print_string(23, 0, "System halted. Please restart.", VGA_LIGHT_GRAY_ON_BLUE);
    
    // Disable interrupts and halt
    cli();
    while (true) {
        halt();
    }
}

void Exceptions::divide_error_handler(ExceptionFrame* frame) {
    VGADisplay vga;
    vga.clear_screen(VGA_RED_ON_BLUE);
    vga.print_string(0, 0, "*** DIVISION BY ZERO ERROR ***", VGA_WHITE_ON_BLUE);
    vga.print_string(2, 0, "A division by zero occurred at:", VGA_YELLOW_ON_BLUE);
    vga.print_string(3, 0, "EIP: ", VGA_CYAN_ON_BLUE);
    vga.print_hex(3, 5, frame->eip, VGA_WHITE_ON_BLUE);
    
    default_handler(frame);
}

void Exceptions::general_protection_handler(ExceptionFrame* frame) {
    VGADisplay vga;
    vga.clear_screen(VGA_RED_ON_BLUE);
    vga.print_string(0, 0, "*** GENERAL PROTECTION FAULT ***", VGA_WHITE_ON_BLUE);
    vga.print_string(2, 0, "Invalid memory access or privilege violation", VGA_YELLOW_ON_BLUE);
    
    default_handler(frame);
}

void Exceptions::page_fault_handler(ExceptionFrame* frame) {
    VGADisplay vga;
    vga.clear_screen(VGA_RED_ON_BLUE);
    vga.print_string(0, 0, "*** PAGE FAULT ***", VGA_WHITE_ON_BLUE);
    vga.print_string(2, 0, "Invalid memory page access", VGA_YELLOW_ON_BLUE);
    
    // Get the faulting address from CR2 register
    u32 fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    vga.print_string(3, 0, "Fault Address: ", VGA_CYAN_ON_BLUE);
    vga.print_hex(3, 15, fault_addr, VGA_WHITE_ON_BLUE);
    
    default_handler(frame);
}

const char* Exceptions::get_exception_name(u32 exception_number) {
    switch (exception_number) {
        case 0: return "Division Error";
        case 1: return "Debug";
        case 2: return "Non-Maskable Interrupt";
        case 3: return "Breakpoint";
        case 4: return "Overflow";
        case 5: return "Bound Range Exceeded";
        case 6: return "Invalid Opcode";
        case 7: return "Device Not Available";
        case 8: return "Double Fault";
        case 10: return "Invalid TSS";
        case 11: return "Segment Not Present";
        case 12: return "Stack Fault";
        case 13: return "General Protection Fault";
        case 14: return "Page Fault";
        default: return "Unknown Exception";
    }
}

} // namespace kira::system 