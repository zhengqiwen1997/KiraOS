#include "interrupts/exceptions.hpp"
#include "arch/x86/idt.hpp"
#include "display/console.hpp"
#include "core/io.hpp"
#include "core/utils.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::system {

using namespace kira::display;
using namespace kira::utils;

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
    const char* exc_name = get_exception_name(frame->interruptNumber);
    
    // Format: "EXCEPTION: [name] (INT xx) at EIP 0xXXXXXXXX"
    format_exception_message(msg, exc_name, frame->interruptNumber, frame->eip);
    
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
    kira::kernel::console.refresh_display();
    
    // Handle exceptions based on severity and type
    handle_exception_by_type(frame);
}

void Exceptions::handle_exception_by_type(ExceptionFrame* frame) {
    switch (frame->interruptNumber) {
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
            if (frame->interruptNumber <= 31) {
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
    strcpy(msg, "SYSTEM HALT: ");
    strcat(msg, reason);
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
    char msg[256];
    format_gpf_message(msg, frame->errorCode);
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
    halt_system("General Protection Fault");
}

void Exceptions::page_fault_handler(ExceptionFrame* frame) {
    u32 faultAddr;
    asm volatile("mov %%cr2, %0" : "=r"(faultAddr));
    
    char msg[512];
    format_page_fault_message(msg, faultAddr, frame->errorCode);
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
    
    // Decode error code for additional info
    const char* accessType = (frame->errorCode & 0x2) ? "write" : "read";
    const char* privilege = (frame->errorCode & 0x4) ? "user" : "kernel";
    const char* present = (frame->errorCode & 0x1) ? "protection" : "not present";
    
    char detail[256];
    strcpy(detail, "Page fault details: ");
    strcat(detail, accessType);
    strcat(detail, " access, ");
    strcat(detail, privilege);
    strcat(detail, " mode, ");
    strcat(detail, present);
    kira::kernel::console.add_message(detail, VGA_YELLOW_ON_BLUE);
    
    halt_system("Page Fault");
}

// Helper function implementations - now using common utils
void Exceptions::format_exception_message(char* buffer, const char* name, u32 number, u32 eip) {
    strcpy(buffer, "EXCEPTION: ");
    strcat(buffer, name);
    strcat(buffer, " (");
    
    char numBuf[16];
    number_to_decimal(numBuf, number);
    strcat(buffer, numBuf);
    strcat(buffer, ") at EIP=0x");
    
    char hexBuf[16];
    number_to_hex(hexBuf, eip);
    strcat(buffer, hexBuf);
}

void Exceptions::format_eip_message(char* buffer, u32 eip) {
    strcpy(buffer, "Instruction pointer: 0x");
    char hexBuf[16];
    number_to_hex(hexBuf, eip);
    strcat(buffer, hexBuf);
}

void Exceptions::format_gpf_message(char* buffer, u32 errorCode) {
    strcpy(buffer, "General Protection Fault - Error Code: 0x");
    char hexBuf[16];
    number_to_hex(hexBuf, errorCode);
    strcat(buffer, hexBuf);
}

void Exceptions::format_page_fault_message(char* buffer, u32 faultAddr, u32 errorCode) {
    strcpy(buffer, "Page Fault at address: 0x");
    char hexBuf[16];
    number_to_hex(hexBuf, faultAddr);
    strcat(buffer, hexBuf);
    strcat(buffer, " (Error: 0x");
    number_to_hex(hexBuf, errorCode);
    strcat(buffer, hexBuf);
    strcat(buffer, ")");
}

const char* Exceptions::get_exception_name(u32 exceptionNumber) {
    switch (exceptionNumber) {
        case 0: return "Divide Error";
        case 1: return "Debug Exception";
        case 2: return "NMI Interrupt";
        case 3: return "Breakpoint";
        case 4: return "Overflow";
        case 5: return "BOUND Range Exceeded";
        case 6: return "Invalid Opcode";
        case 7: return "Device Not Available";
        case 8: return "Double Fault";
        case 9: return "Coprocessor Segment Overrun";
        case 10: return "Invalid TSS";
        case 11: return "Segment Not Present";
        case 12: return "Stack Fault";
        case 13: return "General Protection";
        case 14: return "Page Fault";
        case 16: return "x87 FPU Floating-Point Error";
        case 17: return "Alignment Check";
        case 18: return "Machine Check";
        case 19: return "SIMD Floating-Point Exception";
        case 20: return "Virtualization Exception";
        case 21: return "Control Protection Exception";
        default: return "Unknown Exception";
    }
}

} // namespace kira::system 