#include "interrupts/exceptions.hpp"
#include "arch/x86/idt.hpp"
#include "display/vga.hpp"
#include "display/console.hpp"
#include "core/io.hpp"
#include "core/utils.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::system {

using namespace kira::display;
using namespace kira::system::utils;

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
    // Log exception to console
    char msg[80];
    const char* exc_name = get_exception_name(frame->interrupt_number);
    
    // Format: "EXCEPTION: [name] (INT xx) at EIP 0xXXXXXXXX"
    format_exception_message(msg, exc_name, frame->interrupt_number, frame->eip);
    
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
    kira::kernel::console.refresh_display();
    
    // Handle exceptions based on severity and type
    handle_exception_by_type(frame);
}

void Exceptions::handle_exception_by_type(ExceptionFrame* frame) {
    switch (frame->interrupt_number) {
        // ========== RECOVERABLE EXCEPTIONS ==========
        case INT_BREAKPOINT:  // Interrupt 3 - Debugging breakpoint
            kira::kernel::console.add_message("Breakpoint hit - continuing", VGA_GREEN_ON_BLUE);
            // For testing: increment EIP to skip the int $3 instruction
            frame->eip += 2;  // Skip "int $3" instruction (2 bytes: CD 03)
            break;
            
        case INT_OVERFLOW:    // Interrupt 4 - Arithmetic overflow
            kira::kernel::console.add_message("Arithmetic overflow - continuing", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $4" instruction (2 bytes: CD 04)
            break;
            
        // ========== POTENTIALLY RECOVERABLE (SKIP INSTRUCTION) ==========
        case INT_DIVIDE_ERROR:    // Interrupt 0 - Division by zero
            kira::kernel::console.add_message("Division by zero detected!", VGA_YELLOW_ON_BLUE);
            divide_error_handler(frame);
            break;
            
        case INT_INVALID_OPCODE:  // Interrupt 6 - Invalid instruction
        {
            kira::kernel::console.add_message("Invalid opcode - system halted", VGA_RED_ON_BLUE);
            halt_system("Invalid Opcode");
            break;
        }
            
        case INT_BOUND_RANGE:     // Interrupt 5 - Array bounds exceeded
            kira::kernel::console.add_message("Array bounds exceeded - continuing", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $5" instruction (2 bytes: CD 05)
            break;
            
        case INT_DEVICE_NOT_AVAILABLE:  // Interrupt 7 - FPU not available
            kira::kernel::console.add_message("FPU not available - continuing", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $7" instruction (2 bytes: CD 07)
            break;
            
        case INT_X87_FPU_ERROR:   // Interrupt 16 - x87 FPU error
            kira::kernel::console.add_message("x87 FPU error - continuing", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $16" instruction (2 bytes: CD 10)
            break;
            
        case INT_SIMD_FPU_ERROR:  // Interrupt 19 - SIMD FPU error
            kira::kernel::console.add_message("SIMD FPU error - continuing", VGA_YELLOW_ON_BLUE);
            frame->eip += 2;  // Skip "int $19" instruction (2 bytes: CD 13)
            break;
            
        // ========== SERIOUS VIOLATIONS - MUST HALT ==========
        case INT_GENERAL_PROTECTION:  // Interrupt 13 - Memory/privilege violation
        {
            kira::kernel::console.add_message("CRITICAL: General Protection Fault!", VGA_RED_ON_BLUE);
            general_protection_handler(frame);
            halt_system("General Protection Fault - System integrity compromised");
            break;
        }
            
        case INT_PAGE_FAULT:          // Interrupt 14 - Invalid memory page
            kira::kernel::console.add_message("CRITICAL: Page Fault!", VGA_RED_ON_BLUE);
            page_fault_handler(frame);
            halt_system("Page Fault - Invalid memory access");
            break;
            
        case INT_STACK_FAULT:         // Interrupt 12 - Stack segment fault
            kira::kernel::console.add_message("CRITICAL: Stack Fault!", VGA_RED_ON_BLUE);
            halt_system("Stack Fault - Stack segment violation");
            break;
            
        case INT_SEGMENT_NOT_PRESENT: // Interrupt 11 - Segment not present
            kira::kernel::console.add_message("CRITICAL: Segment Not Present!", VGA_RED_ON_BLUE);
            halt_system("Segment Not Present - Invalid segment access");
            break;
            
        case INT_INVALID_TSS:         // Interrupt 10 - Invalid TSS
            kira::kernel::console.add_message("CRITICAL: Invalid TSS!", VGA_RED_ON_BLUE);
            halt_system("Invalid TSS - Task state segment error");
            break;
            
        case INT_ALIGNMENT_CHECK:     // Interrupt 17 - Alignment check
            kira::kernel::console.add_message("CRITICAL: Alignment Check!", VGA_RED_ON_BLUE);
            halt_system("Alignment Check - Unaligned memory access");
            break;
            
        // ========== CRITICAL SYSTEM ERRORS - IMMEDIATE HALT ==========
        case INT_DOUBLE_FAULT:        // Interrupt 8 - Critical system error
            kira::kernel::console.add_message("FATAL: Double Fault!", VGA_RED_ON_BLUE);
            halt_system("Double Fault - Critical system failure");
            break;
            
        case INT_MACHINE_CHECK:       // Interrupt 18 - Hardware error
            kira::kernel::console.add_message("FATAL: Machine Check!", VGA_RED_ON_BLUE);
            halt_system("Machine Check - Hardware failure detected");
            break;
            
        // ========== SPECIAL CASES ==========
        case INT_DEBUG:               // Interrupt 1 - Debug exception
            kira::kernel::console.add_message("Debug exception", VGA_CYAN_ON_BLUE);
            // Don't modify EIP - debugger should handle this
            break;
            
        case INT_NMI:                 // Interrupt 2 - Non-maskable interrupt
            kira::kernel::console.add_message("Non-maskable interrupt", VGA_MAGENTA_ON_BLUE);
            // NMI is usually hardware-related, just acknowledge
            break;
            
        case INT_VIRTUALIZATION_ERROR:    // Interrupt 20 - Virtualization error
            kira::kernel::console.add_message("Virtualization error", VGA_YELLOW_ON_BLUE);
            halt_system("Virtualization Error - VM operation failed");
            break;
            
        case INT_CONTROL_PROTECTION_ERROR: // Interrupt 21 - Control flow protection
            kira::kernel::console.add_message("CRITICAL: Control Protection Error!", VGA_RED_ON_BLUE);
            halt_system("Control Protection Error - ROP/JOP attack detected");
            break;
            
        // ========== RESERVED/UNKNOWN EXCEPTIONS ==========
        default:
            if (frame->interrupt_number <= 31) {
                // Reserved CPU exception
                kira::kernel::console.add_message("CRITICAL: Reserved Exception!", VGA_RED_ON_BLUE);
                halt_system("Reserved Exception - Unknown CPU exception");
            } else {
                // Hardware interrupt or software interrupt
                kira::kernel::console.add_message("Unknown interrupt", VGA_YELLOW_ON_BLUE);
                halt_system("Unknown Interrupt - Unhandled interrupt");
            }
            break;
    }
    
    // Refresh console after handling exception
    kira::kernel::console.refresh_display();
}

void Exceptions::halt_system(const char* reason) {
    // Log the halt reason to console
    char msg[80];
    string_copy(msg, "SYSTEM HALT: ");
    string_concat(msg, reason);
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
    kira::kernel::console.add_message("System stopped - manual restart required", VGA_RED_ON_BLUE);
    kira::kernel::console.refresh_display();
    
    // Halt the system
    halt();
}

void Exceptions::divide_error_handler(ExceptionFrame* frame) {
    char msg[80];
    format_eip_message(msg, frame->eip);
    kira::kernel::console.add_message(msg, VGA_YELLOW_ON_BLUE);
    kira::kernel::console.add_message("Attempting to continue...", VGA_YELLOW_ON_BLUE);
    
    // Skip the problematic instruction (this is a simple approach)
    frame->eip += 2;  // Skip typical 2-byte instruction
}

void Exceptions::general_protection_handler(ExceptionFrame* frame) {
    char msg[80];
    format_gpf_message(msg, frame->error_code);
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
    kira::kernel::console.add_message("Memory/privilege violation detected", VGA_RED_ON_BLUE);
}

void Exceptions::page_fault_handler(ExceptionFrame* frame) {
    // Get the faulting address from CR2 register
    u32 fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    char msg[80];
    format_eip_message(msg, frame->eip);
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
    
    format_page_fault_message(msg, fault_addr, frame->error_code);
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
    
    // Decode error code
    const char* access_type = (frame->error_code & 0x2) ? "write" : "read";
    const char* privilege = (frame->error_code & 0x4) ? "user" : "kernel";
    const char* present = (frame->error_code & 0x1) ? "protection" : "not present";
    
    string_copy(msg, "Type: ");
    string_concat(msg, privilege);
    string_concat(msg, " ");
    string_concat(msg, access_type);
    string_concat(msg, " (");
    string_concat(msg, present);
    string_concat(msg, ")");
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
}

// Helper function implementations - now using common utils
void Exceptions::format_exception_message(char* buffer, const char* name, u32 number, u32 eip) {
    string_copy(buffer, "EXCEPTION: ");
    string_concat(buffer, name);
    string_concat(buffer, " (INT ");
    
    char num_buf[16];
    number_to_decimal(num_buf, number);
    string_concat(buffer, num_buf);
    
    string_concat(buffer, ") at EIP 0x");
    
    char hex_buf[16];
    number_to_hex(hex_buf, eip);
    string_concat(buffer, hex_buf);
}

void Exceptions::format_eip_message(char* buffer, u32 eip) {
    string_copy(buffer, "EIP: 0x");
    char hex_buf[16];
    number_to_hex(hex_buf, eip);
    string_concat(buffer, hex_buf);
}

void Exceptions::format_gpf_message(char* buffer, u32 error_code) {
    string_copy(buffer, "GPF Error: 0x");
    char hex_buf[16];
    number_to_hex(hex_buf, error_code);
    string_concat(buffer, hex_buf);
}

void Exceptions::format_page_fault_message(char* buffer, u32 fault_addr, u32 error_code) {
    string_copy(buffer, "Fault address: 0x");
    char hex_buf[16];
    number_to_hex(hex_buf, fault_addr);
    string_concat(buffer, hex_buf);
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