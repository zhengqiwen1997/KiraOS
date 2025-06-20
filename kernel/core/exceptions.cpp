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
    
    // Display exception info in top-right corner
    vga.print_string(0, 60, "EXC!", VGA_WHITE_ON_BLUE);
    vga.print_decimal(1, 60, frame->interrupt_number, VGA_WHITE_ON_BLUE);
    
    // Handle exceptions based on severity and type
    handle_exception_by_type(frame, vga);
}

void Exceptions::handle_exception_by_type(ExceptionFrame* frame, VGADisplay& vga) {
    switch (frame->interrupt_number) {
        // ========== RECOVERABLE EXCEPTIONS ==========
        case INT_BREAKPOINT:  // Interrupt 3 - Debugging breakpoint
            vga.print_string(2, 60, "BP", VGA_GREEN_ON_BLUE);
            // For testing: increment EIP to skip the int $3 instruction
            frame->eip += 2;  // Skip "int $3" instruction (2 bytes: CD 03)
            break;
            
        case INT_OVERFLOW:    // Interrupt 4 - Arithmetic overflow
            vga.print_string(2, 60, "OF", VGA_GREEN_ON_BLUE);
            frame->eip += 2;  // Skip "int $4" instruction (2 bytes: CD 04)
            break;
            
        // ========== POTENTIALLY RECOVERABLE (SKIP INSTRUCTION) ==========
        case INT_DIVIDE_ERROR:    // Interrupt 0 - Division by zero
            vga.print_string(2, 60, "DIV", VGA_YELLOW_ON_BLUE);
            //frame->eip += 2;  // Skip "int $0" instruction (2 bytes: CD 00)
            break;
            
        case INT_INVALID_OPCODE:  // Interrupt 6 - Invalid instruction
            vga.print_string(2, 60, "UD", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $6" instruction (2 bytes: CD 06)
            break;
            
        case INT_BOUND_RANGE:     // Interrupt 5 - Array bounds exceeded
            vga.print_string(2, 60, "BR", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $5" instruction (2 bytes: CD 05)
            break;
            
        case INT_DEVICE_NOT_AVAILABLE:  // Interrupt 7 - FPU not available
            vga.print_string(2, 60, "NM", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $7" instruction (2 bytes: CD 07)
            break;
            
        case INT_X87_FPU_ERROR:   // Interrupt 16 - x87 FPU error
            vga.print_string(2, 60, "MF", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $16" instruction (2 bytes: CD 10)
            break;
            
        case INT_SIMD_FPU_ERROR:  // Interrupt 19 - SIMD FPU error
            vga.print_string(2, 60, "XM", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $19" instruction (2 bytes: CD 13)
            break;
            
        // ========== SERIOUS VIOLATIONS - MUST HALT ==========
        case INT_GENERAL_PROTECTION:  // Interrupt 13 - Memory/privilege violation
            vga.print_string(2, 60, "GPF", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("General Protection Fault - System integrity compromised");
            break;
            
        case INT_PAGE_FAULT:          // Interrupt 14 - Invalid memory page
            vga.print_string(2, 60, "PF", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("Page Fault - Invalid memory access");
            break;
            
        case INT_STACK_FAULT:         // Interrupt 12 - Stack segment fault
            vga.print_string(2, 60, "SF", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("Stack Fault - Stack segment violation");
            break;
            
        case INT_SEGMENT_NOT_PRESENT: // Interrupt 11 - Segment not present
            vga.print_string(2, 60, "NP", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("Segment Not Present - Invalid segment access");
            break;
            
        case INT_INVALID_TSS:         // Interrupt 10 - Invalid TSS
            vga.print_string(2, 60, "TS", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("Invalid TSS - Task state segment error");
            break;
            
        case INT_ALIGNMENT_CHECK:     // Interrupt 17 - Alignment check
            vga.print_string(2, 60, "AC", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("Alignment Check - Unaligned memory access");
            break;
            
        // ========== CRITICAL SYSTEM ERRORS - IMMEDIATE HALT ==========
        case INT_DOUBLE_FAULT:        // Interrupt 8 - Critical system error
            vga.print_string(2, 60, "DF", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("Double Fault - Critical system failure");
            break;
            
        case INT_MACHINE_CHECK:       // Interrupt 18 - Hardware error
            vga.print_string(2, 60, "MC", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("Machine Check - Hardware failure detected");
            break;
            
        // ========== SPECIAL CASES ==========
        case INT_DEBUG:               // Interrupt 1 - Debug exception
            vga.print_string(2, 60, "DB", VGA_CYAN_ON_BLUE);
            // Don't modify EIP - debugger should handle this
            break;
            
        case INT_NMI:                 // Interrupt 2 - Non-maskable interrupt
            vga.print_string(2, 60, "NMI", VGA_MAGENTA_ON_BLUE);
            // NMI is usually hardware-related, just acknowledge
            break;
            
        case INT_VIRTUALIZATION_ERROR:    // Interrupt 20 - Virtualization error
            vga.print_string(2, 60, "VE", VGA_YELLOW_ON_BLUE);
            halt_system("Virtualization Error - VM operation failed");
            break;
            
        case INT_CONTROL_PROTECTION_ERROR: // Interrupt 21 - Control flow protection
            vga.print_string(2, 60, "CP", VGA_RED_ON_BLUE);
            vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
            halt_system("Control Protection Error - ROP/JOP attack detected");
            break;
            
        // ========== RESERVED/UNKNOWN EXCEPTIONS ==========
        default:
            if (frame->interrupt_number <= 31) {
                // Reserved CPU exception
                vga.print_string(2, 60, "RSV", VGA_RED_ON_BLUE);
                vga.print_string(3, 60, "HALT", VGA_RED_ON_BLUE);
                halt_system("Reserved Exception - Unknown CPU exception");
            } else {
                // Hardware interrupt or software interrupt
                vga.print_string(2, 60, "UNK", VGA_YELLOW_ON_BLUE);
                halt_system("Unknown Interrupt - Unhandled interrupt");
            }
            break;
    }
}

void Exceptions::halt_system(const char* reason) {
    // Log the halt reason (could be expanded for logging systems)
    // For now, just halt the system
    halt();
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
        case 9: return "Coprocessor Segment Overrun";
        case 10: return "Invalid TSS";
        case 11: return "Segment Not Present";
        case 12: return "Stack Fault";
        case 13: return "General Protection Fault";
        case 14: return "Page Fault";
        case 16: return "x87 FPU Error";
        case 17: return "Alignment Check";
        case 18: return "Machine Check";
        case 19: return "SIMD FPU Error";
        case 20: return "Virtualization Error";
        case 21: return "Control Protection Error";
        default: 
            if (exception_number <= 31) {
                return "Reserved Exception";
            } else {
                return "Hardware/Software Interrupt";
            }
    }
}

} // namespace kira::system 