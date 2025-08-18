#include "user_utils.hpp"
#include "libkira.hpp"

namespace kira::usermode::util {

void build_absolute_path(const char* cwd, const char* inputPath, char* outPath, u32 outSize) {
    if (!outPath || outSize == 0) return;

    char combined[256];
    u32 pos = 0; combined[0] = '\0';

    if (inputPath && inputPath[0] == '/') {
        for (u32 i = 0; inputPath[i] != '\0' && pos < sizeof(combined) - 1; i++) combined[pos++] = inputPath[i];
    } else {
        if (cwd) { for (u32 i = 0; cwd[i] != '\0' && pos < sizeof(combined) - 1; i++) combined[pos++] = cwd[i]; }
        if (pos == 0 || combined[0] != '/') { if (pos < sizeof(combined) - 1) combined[pos++] = '/'; }
        else if (!(pos == 1 && combined[0] == '/')) { if (combined[pos - 1] != '/' && pos < sizeof(combined) - 1) combined[pos++] = '/'; }
        if (inputPath) { for (u32 i = 0; inputPath[i] != '\0' && pos < sizeof(combined) - 1; i++) combined[pos++] = inputPath[i]; }
    }
    combined[pos < sizeof(combined) ? pos : (sizeof(combined) - 1)] = '\0';

    const u32 MAX_COMPONENTS = 32; const u32 MAX_COMP_LEN = 64;
    char components[MAX_COMPONENTS][MAX_COMP_LEN]; u32 compCount = 0;
    u32 i = 0;
    while (combined[i] != '\0') {
        while (combined[i] == '/') { i++; }
        if (combined[i] == '\0') break;
        char segment[MAX_COMP_LEN]; u32 segLen = 0;
        while (combined[i] != '\0' && combined[i] != '/' && segLen < MAX_COMP_LEN - 1) segment[segLen++] = combined[i++];
        segment[segLen] = '\0';
        if (segment[0] == '\0' || (segment[0] == '.' && segment[1] == '\0')) {
        } else if (segment[0] == '.' && segment[1] == '.' && segment[2] == '\0') {
            if (compCount > 0) compCount--;
        } else {
            if (compCount < MAX_COMPONENTS) {
                u32 c = 0; while (segment[c] != '\0' && c < MAX_COMP_LEN - 1) { components[compCount][c] = segment[c]; c++; }
                components[compCount][c] = '\0'; compCount++;
            }
        }
    }

    u32 outPos = 0; if (outPos < outSize - 1) outPath[outPos++] = '/';
    if (compCount == 0) { outPath[outPos < outSize ? outPos : (outSize - 1)] = '\0'; return; }
    for (u32 idx = 0; idx < compCount; idx++) {
        u32 c = 0; while (components[idx][c] != '\0' && outPos < outSize - 1) outPath[outPos++] = components[idx][c++];
        if (idx + 1 < compCount && outPos < outSize - 1) outPath[outPos++] = '/';
    }
    outPath[outPos < outSize ? outPos : (outSize - 1)] = '\0';
}

bool directory_exists(const char* absPath) {
    if (!absPath || absPath[0] != '/') return false;
    kira::usermode::FileSystem::DirectoryEntry entry;
    i32 rd = kira::usermode::UserAPI::readdir(absPath, 0, &entry);
    if (rd == 0) return true;
    if (rd == -7) return false;
    if (rd == -5) {
        i32 fd = kira::usermode::UserAPI::open(absPath, static_cast<u32>(kira::usermode::FileSystem::OpenFlags::READ_ONLY));
        if (fd >= 0) { kira::usermode::UserAPI::close(fd); return true; }
        return false;
    }
    return false;
}

bool string_equals(const char* s1, const char* s2) {
    if (!s1 || !s2) return false;
    u32 i = 0; while (i < 32) { if (s1[i] != s2[i]) return false; if (s1[i] == '\0') return true; i++; }
    return false;
}

} // namespace kira::usermode::util


