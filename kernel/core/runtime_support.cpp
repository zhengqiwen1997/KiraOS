#include "core/types.hpp"

// C++ runtime support for freestanding environment
// These functions support C++ language features in kernel environment

using namespace kira::system;

// Pure virtual function call handler
extern "C" void __cxa_pure_virtual() {
    // Pure virtual function called - this is a programming bug
    // Halt the system since this should never happen
    asm volatile("cli; hlt" : : : "memory");
}

// Global constructor/destructor support
extern "C" void __cxa_atexit() {
    // atexit() functionality not needed in kernel environment
    // Static destructors are not called in kernel
}

extern "C" void __dso_handle() {
    // Dynamic shared object handle - not needed in kernel environment
} 