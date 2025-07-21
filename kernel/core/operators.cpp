#include "core/types.hpp"
#include "memory/memory_manager.hpp"

// C++ runtime support for freestanding environment

using namespace kira::system;

// Regular new operators (use our memory manager)
void* operator new(size_t size) {
    auto& memMgr = MemoryManager::get_instance();
    return memMgr.allocate_physical_page();
}

void* operator new[](size_t size) {
    auto& memMgr = MemoryManager::get_instance();
    return memMgr.allocate_physical_page();
}

// Delete operators (use our memory manager)
void operator delete(void* ptr) noexcept {
    if (ptr) {
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(ptr);
    }
}

void operator delete[](void* ptr) noexcept {
    if (ptr) {
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(ptr);
    }
}

// Sized delete operators (C++14)
void operator delete(void* ptr, size_t size) noexcept {
    if (ptr) {
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(ptr);
    }
}

void operator delete[](void* ptr, size_t size) noexcept {
    if (ptr) {
        auto& memMgr = MemoryManager::get_instance();
        memMgr.free_physical_page(ptr);
    }
}

// Placement delete operators (should not be called)
void operator delete(void* ptr, void* place) noexcept {
    // Placement delete - should not actually free memory
    // This is called if placement new constructor throws
}

void operator delete[](void* ptr, void* place) noexcept {
    // Placement delete - should not actually free memory
}

// Pure virtual function call handler
extern "C" void __cxa_pure_virtual() {
    // Pure virtual function called - this is a bug
    asm volatile("cli; hlt" : : : "memory");
}

// Global constructor/destructor support
extern "C" void __cxa_atexit() {
    // Not needed in kernel environment
}

extern "C" void __dso_handle() {
    // Not needed in kernel environment
} 