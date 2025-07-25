#include "core/types.hpp"
#include "memory/memory_manager.hpp"

// Regular new/delete operators for automatic memory management
// These use the kernel's memory manager for heap-like allocation

using namespace kira::system;

// Regular new operators (automatic allocation)
void* operator new(size_t size) {
    auto& memMgr = MemoryManager::get_instance();
    return memMgr.allocate_physical_page();
}

void* operator new[](size_t size) {
    auto& memMgr = MemoryManager::get_instance();
    return memMgr.allocate_physical_page();
}

// Regular delete operators (automatic deallocation)
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