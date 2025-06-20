#pragma once

#include "types.hpp"

// Forward declarations
namespace kira::display {
    class VGADisplay;
}

namespace kira::system {

// Exception frame structure (pushed by CPU and our handler)
struct ExceptionFrame {
    // Pushed by our handler
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;  // General purpose registers
    u32 interrupt_number;                         // Interrupt number
    u32 error_code;                              // Error code (if applicable)
    
    // Pushed by CPU
    u32 eip;                                     // Instruction pointer
    u32 cs;                                      // Code segment
    u32 eflags;                                  // Flags register
    u32 user_esp;                                // User stack pointer (if privilege change)
    u32 user_ss;                                 // User stack segment (if privilege change)
} __attribute__((packed));

/**
 * @brief Exception handler function type
 * @param frame Pointer to exception frame with register state
 */
using ExceptionHandler = void (*)(ExceptionFrame* frame);

/**
 * @brief Exception management class
 * 
 * Provides basic exception handling capabilities with simple error reporting.
 */
class Exceptions {
public:
    /**
     * @brief Initialize exception handlers
     * Sets up basic handlers for common CPU exceptions
     */
    static void initialize();
    
    /**
     * @brief Default exception handler
     * Displays exception information and halts the system
     * @param frame Exception frame with register state
     */
    static void default_handler(ExceptionFrame* frame);
    
    /**
     * @brief Division by zero exception handler
     * @param frame Exception frame
     */
    static void divide_error_handler(ExceptionFrame* frame);
    
    /**
     * @brief General protection fault handler
     * @param frame Exception frame
     */
    static void general_protection_handler(ExceptionFrame* frame);
    
    /**
     * @brief Page fault handler
     * @param frame Exception frame
     */
    static void page_fault_handler(ExceptionFrame* frame);
    
    /**
     * @brief Get exception name from number
     * @param exception_number Exception number (0-31)
     * @return Human-readable exception name
     */
    static const char* get_exception_name(u32 exception_number);

private:
    /**
     * @brief Handle exception based on its type and severity
     * @param frame Exception frame with register state
     * @param vga VGA display reference for output
     */
    static void handle_exception_by_type(ExceptionFrame* frame, kira::display::VGADisplay& vga);
    
    /**
     * @brief Halt the system with a reason
     * @param reason Reason for halting the system
     */
    static void halt_system(const char* reason);
};

// External assembly stubs (will be implemented in assembly)
extern "C" {
    void exception_stub_0();   // Division error
    void exception_stub_1();   // Debug
    void exception_stub_2();   // NMI
    void exception_stub_3();   // Breakpoint
    void exception_stub_4();   // Overflow
    void exception_stub_5();   // Bound range
    void exception_stub_6();   // Invalid opcode
    void exception_stub_7();   // Device not available
    void exception_stub_8();   // Double fault
    void exception_stub_10();  // Invalid TSS
    void exception_stub_11();  // Segment not present
    void exception_stub_12();  // Stack fault
    void exception_stub_13();  // General protection
    void exception_stub_14();  // Page fault
}

} // namespace kira::system 