#pragma once

namespace kira::system {

using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using i8  = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using size_t = u32;  // Use 32-bit size_t for our 32-bit kernel
using uintptr_t = u32;
using intptr_t = i32;

} // namespace kira::system 