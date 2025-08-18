#pragma once

#include "core/types.hpp"

namespace kira::usermode::util {

using namespace kira::system;

// Build an absolute, normalized path from cwd and an input path.
// Handles absolute and relative inputs, "." and ".." components, and collapses duplicate slashes.
void build_absolute_path(const char* cwd, const char* inputPath, char* outPath, u32 outSize);

// Check whether an absolute path refers to an existing directory (user-mode view)
bool directory_exists(const char* absPath);

// Check if two strings are equal
bool string_equals(const char* s1, const char* s2);

} // namespace kira::usermode::util


