#pragma once

// Standard size_t definition for placement new
typedef unsigned long size_t;

namespace kira::system {

using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using i8  = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using uintptr_t = u32;
using intptr_t = i32;

} // namespace kira::system

// Placement new operator for kernel
inline void* operator new(size_t, void* ptr) {
    return ptr;
}

// Placement new array operator
inline void* operator new[](size_t, void* ptr) {
    return ptr;
} 