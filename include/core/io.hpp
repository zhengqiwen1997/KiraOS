#pragma once

#include "core/types.hpp"

namespace kira::system {

// Port I/O functions
inline void outb(u16 port, u8 value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline u8 inb(u16 port) {
    u8 result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

inline void outw(u16 port, u16 value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

inline u16 inw(u16 port) {
    u16 result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

inline void outl(u16 port, u32 value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

inline u32 inl(u16 port) {
    u32 result;
    asm volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Memory-mapped I/O functions
inline void mmio_write8(void* addr, u8 value) {
    *(volatile u8*)addr = value;
}

inline u8 mmio_read8(void* addr) {
    return *(volatile u8*)addr;
}

inline void mmio_write16(void* addr, u16 value) {
    *(volatile u16*)addr = value;
}

inline u16 mmio_read16(void* addr) {
    return *(volatile u16*)addr;
}

inline void mmio_write32(void* addr, u32 value) {
    *(volatile u32*)addr = value;
}

inline u32 mmio_read32(void* addr) {
    return *(volatile u32*)addr;
}

// CPU control functions
inline void halt() {
    asm volatile("hlt");
}

inline void cli() {
    asm volatile("cli");
}

inline void sti() {
    asm volatile("sti");
}

inline void pause() {
    asm volatile("pause");
}

// Memory barrier
inline void memory_barrier() {
    asm volatile("" : : : "memory");
}

// I/O wait function - provides a small delay for slow I/O devices
inline void io_wait() {
    // Use port 0x80 (unused diagnostic port) for a small delay
    outb(0x80, 0);
}

} // namespace kira::system 