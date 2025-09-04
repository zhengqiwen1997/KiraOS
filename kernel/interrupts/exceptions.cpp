#include "interrupts/exceptions.hpp"
#include "arch/x86/idt.hpp"
#include "display/console.hpp"
#include "core/io.hpp"
#include "core/utils.hpp"
#include "debug/serial_debugger.hpp"
#include "core/process.hpp"
#include "memory/virtual_memory.hpp"
#include "memory/memory_manager.hpp"

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
    IDT::set_interrupt_gate(INT_X87_FPU_ERROR, (void*)exception_stub_16);
    IDT::set_interrupt_gate(INT_SIMD_FPU_ERROR, (void*)exception_stub_19);
}

void Exceptions::default_handler(ExceptionFrame* frame) {
    // Defer all console logging to specific handlers so they can decide behavior
    
    // Handle exceptions based on severity and type
    handle_exception_by_type(frame);
}

void Exceptions::handle_exception_by_type(ExceptionFrame* frame) {
    switch (frame->interruptNumber) {
        // ========== RECOVERABLE EXCEPTIONS ==========
        case INT_BREAKPOINT:  // Interrupt 3 - Debugging breakpoint
            kira::kernel::console.add_message("Breakpoint hit - continuing", VGA_GREEN_ON_BLUE);
            break;
            
        case INT_OVERFLOW:    // Interrupt 4 - Arithmetic overflow
            kira::kernel::console.add_message("Arithmetic overflow - continuing", VGA_YELLOW_ON_BLUE);
            break;
            
        // ========== POTENTIALLY RECOVERABLE (SKIP INSTRUCTION) ==========
        case INT_DIVIDE_ERROR:    // Interrupt 0 - Division by zero
            kira::kernel::console.add_message("Division by zero detected!", VGA_YELLOW_ON_BLUE);
            divide_error_handler(frame);
            break;
            
        case INT_BOUND_RANGE:     // Interrupt 5 - Array bounds exceeded
            kira::kernel::console.add_message("Array bounds exceeded - continuing", VGA_YELLOW_ON_BLUE);
            break;
            
        case INT_DEVICE_NOT_AVAILABLE:  // Interrupt 7 - FPU not available
            kira::kernel::console.add_message("FPU not available - continuing", VGA_YELLOW_ON_BLUE);
            break;
            
        case INT_X87_FPU_ERROR:   // Interrupt 16 - x87 FPU error
            kira::kernel::console.add_message("x87 FPU error - continuing", VGA_YELLOW_ON_BLUE);
           // frame->eip += 2;  // Skip "int $16" instruction (2 bytes: CD 10)
            break;
            
        case INT_SIMD_FPU_ERROR:  // Interrupt 19 - SIMD FPU error
            kira::kernel::console.add_message("SIMD FPU error - continuing", VGA_YELLOW_ON_BLUE);
            // frame->eip += 2;  // Skip "int $19" instruction (2 bytes: CD 13)
            break;
            
        // ========== SERIOUS VIOLATIONS - MUST HALT ==========
        case INT_INVALID_OPCODE:  // Interrupt 6 - Invalid instruction
        {
            kira::kernel::console.add_message("Invalid opcode - system halted", VGA_RED_ON_BLUE);
            halt_system("Invalid Opcode");
            break;
        }

        case INT_GENERAL_PROTECTION:  // Interrupt 13 - Memory/privilege violation
        {
            kira::kernel::console.add_message("CRITICAL: General Protection Fault!", VGA_RED_ON_BLUE);
            general_protection_handler(frame);
            halt_system("General Protection Fault - System integrity compromised");
            break;
        }
            
        case INT_PAGE_FAULT: {        // Interrupt 14 - Invalid memory page
            // Do not print to console here; the handler will decide whether to log/halt
            page_fault_handler(frame); // handler halts on failure; returns on success
            return; // Do not halt here if handler resolved the fault
        }
            
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

    // Check if we're in kernel mode (CPL = 0) or user mode (CPL = 3)
    // We can check this from the CS register in the exception frame
    bool is_kernel_mode = (frame->cs & 0x3) == 0;

    if (is_kernel_mode) {
        // Division by zero in kernel mode - this is a serious bug!
        kira::kernel::console.add_message("CRITICAL: Division by zero in kernel mode!", VGA_RED_ON_BLUE);
        kira::kernel::console.add_message("This indicates a kernel bug and must be fixed.", VGA_RED_ON_BLUE);
        
        // In debug builds, we might want to print more diagnostic info
        char debug_info[256];
        strcpy(debug_info, "Fault location: 0x");
        char hexBuf[16];
        number_to_hex(hexBuf, frame->eip);
        strcat(debug_info, hexBuf);
        kira::kernel::console.add_message(debug_info, VGA_RED_ON_BLUE);
        
        // Kernel mode division by zero should always halt the system
        halt_system("Kernel Mode Division Error - System integrity cannot be guaranteed");
    } else {
        // User mode division by zero - we can handle this more gracefully
        kira::kernel::console.add_message("Division by zero in user mode - terminating process", VGA_YELLOW_ON_BLUE);
        
        // TODO: Once we implement process management, we should:
        // 1. Send SIGFPE to the process
        // 2. If not handled, terminate the process
        // 3. Clean up process resources
        // 4. Schedule next process
        
        // For now, since we don't have full process management:
        halt_system("User process terminated due to division by zero");
    }
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
    
    // Serial-only debug for all faults; console output only if we decide to halt
    kira::debug::SerialDebugger::println("=== PAGE FAULT DEBUG ===");
    kira::debug::SerialDebugger::print("Fault Address (CR2): ");
    kira::debug::SerialDebugger::print_hex(faultAddr);
    kira::debug::SerialDebugger::println("");
    kira::debug::SerialDebugger::print("EIP: ");
    kira::debug::SerialDebugger::print_hex(frame->eip);
    kira::debug::SerialDebugger::println("");
    kira::debug::SerialDebugger::print("Error Code: ");
    kira::debug::SerialDebugger::print_hex(frame->errorCode);
    kira::debug::SerialDebugger::println("");
    
    // Handle Copy-on-Write on user write to present read-only page (error code: P=1, W=1, U=1)
    if ((frame->errorCode & 0x7) == 0x7) {
        auto& pm = kira::system::ProcessManager::get_instance();
        kira::system::Process* cur = pm.get_current_process();
        if (cur && cur->addressSpace) {
            auto& mm = kira::system::MemoryManager::get_instance();
            auto& vm = kira::system::VirtualMemoryManager::get_instance();
            // Faulting page aligned VA and old physical backing
            u32 va = faultAddr & kira::system::PAGE_MASK;
            u32 oldPhys = cur->addressSpace->get_physical_address(va) & kira::system::PAGE_MASK;
            if (oldPhys != 0) {
                // Fast path: if refcount indicates uniquely owned (0 extra refs), make writable
                // Refcount semantics: 0 => single owner; N>0 => shared by N+1 owners
                if (mm.get_page_ref(oldPhys) == 0) {
                    kira::debug::SerialDebugger::print("CoW fast-path (unique) pid=");
                    kira::debug::SerialDebugger::print_hex(cur->pid);
                    kira::debug::SerialDebugger::print(" va="); kira::debug::SerialDebugger::print_hex(va);
                    kira::debug::SerialDebugger::print(" oldPhys="); kira::debug::SerialDebugger::print_hex(oldPhys);
                    kira::debug::SerialDebugger::print(" ref=0");
                    kira::debug::SerialDebugger::println("");
                    cur->addressSpace->set_page_writable(va, true);
                    return;
                }
                void* newPage = mm.allocate_physical_page();
                if (newPage) {
                    // Use a user-space scratch VA unlikely to be mapped (heap base)
                    u32 scratchNew = kira::system::USER_HEAP_START; // 0x40000000
                    cur->addressSpace->unmap_page(scratchNew);
                    if (!cur->addressSpace->map_page(scratchNew, reinterpret_cast<u32>(newPage), true, true)) {
                        mm.free_physical_page(newPage);
                    } else {
                        u32 refBefore = mm.get_page_ref(oldPhys);
                        kira::debug::SerialDebugger::print("CoW copy pid=");
                        kira::debug::SerialDebugger::print_hex(cur->pid);
                        kira::debug::SerialDebugger::print(" va="); kira::debug::SerialDebugger::print_hex(va);
                        kira::debug::SerialDebugger::print(" oldPhys="); kira::debug::SerialDebugger::print_hex(oldPhys);
                        kira::debug::SerialDebugger::print(" newPhys="); kira::debug::SerialDebugger::print_hex(reinterpret_cast<u32>(newPage));
                        // Print refcount in decimal to avoid confusion with addresses
                        char rcBuf[16];
                        number_to_decimal(rcBuf, refBefore);
                        kira::debug::SerialDebugger::print(" refBefore="); kira::debug::SerialDebugger::print(rcBuf);
                        kira::debug::SerialDebugger::println("");
                        vm.switch_address_space(cur->addressSpace);
                        // Copy content from old VA (RO) into the new physical page via scratchNew
                        memcpy(reinterpret_cast<void*>(scratchNew), reinterpret_cast<const void*>(va), kira::system::PAGE_SIZE);
                        // Now remap faulting VA to the new writable page; no second copy needed
                        cur->addressSpace->map_page(va, reinterpret_cast<u32>(newPage), true, true);
                        // Remove scratch mapping and drop old refcount
                        cur->addressSpace->unmap_page(scratchNew);
                        mm.decrement_page_ref(oldPhys);
                        u32 refAfter = mm.get_page_ref(oldPhys);
                        kira::debug::SerialDebugger::print("CoW done pid=");
                        kira::debug::SerialDebugger::print_hex(cur->pid);
                        kira::debug::SerialDebugger::print(" va="); kira::debug::SerialDebugger::print_hex(va);
                        number_to_decimal(rcBuf, refAfter);
                        kira::debug::SerialDebugger::print(" refAfter="); kira::debug::SerialDebugger::print(rcBuf);
                        kira::debug::SerialDebugger::println("");
                        return; // Resume execution
                    }
                }
            }
        }
    }
    // Non-CoW or failed CoW: print to console and halt
    char msg[512];
    format_page_fault_message(msg, faultAddr, frame->errorCode);
    kira::kernel::console.add_message(msg, VGA_RED_ON_BLUE);
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