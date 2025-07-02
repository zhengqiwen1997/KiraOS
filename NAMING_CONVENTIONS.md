# KiraOS Naming Conventions

This document defines the naming conventions used throughout the KiraOS codebase to ensure consistency and readability.

## Table of Contents

- [General Principles](#general-principles)
- [Variables](#variables)
- [Functions](#functions)
- [Classes and Structs](#classes-and-structs)
- [Constants](#constants)
- [Namespaces](#namespaces)
- [Files and Directories](#files-and-directories)
- [Macros](#macros)
- [Special Cases](#special-cases)

## General Principles

1. **Consistency**: Follow the established patterns throughout the codebase
2. **Readability**: Names should be descriptive and self-documenting
3. **Clarity**: Avoid abbreviations unless they are well-known (e.g., `addr`, `ptr`, `size`)
4. **Industry Standards**: Respect established conventions for external APIs and specifications

## Variables

### Local Variables and Function Parameters

Use **camelCase** for multi-word variables:

```cpp
// ✅ Good
u32 memorySize = 1024;
bool isUserMode = true;
void* physicalAddr = nullptr;
u8 scanCode = keyboard.read();

// ❌ Bad
u32 memory_size = 1024;
bool is_user_mode = true;
void* physical_addr = nullptr;
u8 scan_code = keyboard.read();
```

Use **single words** for simple variables (abbreviated when appropriate):

```cpp
// ✅ Good
u32 size;
u32 addr;
void* ptr;
u32 count;
u8 data;

// ❌ Bad
u32 sz;
u32 address;  // Use 'addr' instead
void* pointer; // Use 'ptr' instead
```

### Member Variables

Follow the same rules as local variables - use **camelCase** for multi-word members:

```cpp
class Process {
private:
    u32 timeSlice;          // ✅ Good
    u32 timeUsed;           // ✅ Good
    bool isUserMode;        // ✅ Good
    u32 kernelStackBase;    // ✅ Good
    
    // ❌ Bad
    u32 time_slice;
    u32 time_used;
    bool is_user_mode;
    u32 kernel_stack_base;
};
```

### Global Variables

Use **camelCase** with descriptive prefixes when needed:

```cpp
// ✅ Good
u32 gMemoryMapAddr = 0;
u32 gMemoryMapCount = 0;
ProcessManager* gProcessManager = nullptr;

// ❌ Bad
u32 g_memory_map_addr = 0;
u32 memory_map_count = 0;
ProcessManager* process_manager = nullptr;
```

## Functions

Use **snake_case** for function names:

```cpp
// ✅ Good
void initialize_system();
bool map_page(u32 virtualAddr, u32 physicalAddr);
u32 get_entry_point(const void* elfData);
void handle_keyboard_input(u8 scanCode);

// ❌ Bad
void initializeSystem();
bool mapPage(u32 virtualAddr, u32 physicalAddr);
u32 getEntryPoint(const void* elfData);
void handleKeyboardInput(u8 scanCode);
```

### Member Functions

Follow the same snake_case convention:

```cpp
class MemoryManager {
public:
    void* allocate_physical_page();     // ✅ Good
    void free_physical_page(void* page); // ✅ Good
    bool map_page(u32 addr, u32 phys);   // ✅ Good
    
    // ❌ Bad
    void* allocatePhysicalPage();
    void freePhysicalPage(void* page);
    bool mapPage(u32 addr, u32 phys);
};
```

## Classes and Structs

Use **PascalCase** for class and struct names:

```cpp
// ✅ Good
class ProcessManager;
class MemoryManager;
struct ProcessContext;
struct MemoryMapEntry;

// ❌ Bad
class process_manager;
class memory_manager;
struct process_context;
struct memory_map_entry;
```

## Constants

Use **UPPER_CASE_WITH_UNDERSCORES** for constants:

```cpp
// ✅ Good
constexpr u32 PAGE_SIZE = 4096;
constexpr u32 MAX_PROCESSES = 16;
constexpr u16 VGA_WHITE_ON_BLUE = 0x1F00;
const u8 KEY_ENTER = 0x1C;

// ❌ Bad
constexpr u32 pageSize = 4096;
constexpr u32 maxProcesses = 16;
constexpr u16 vgaWhiteOnBlue = 0x1F00;
const u8 keyEnter = 0x1C;
```

### Enumeration Values

Use **UPPER_CASE_WITH_UNDERSCORES** for enum values:

```cpp
// ✅ Good
enum class ProcessState {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
};

enum class MemoryType {
    USABLE = 1,
    RESERVED = 2,
    ACPI_RECLAIM = 3,
    ACPI_NVS = 4
};

// ❌ Bad
enum class ProcessState {
    Ready,
    Running,
    Blocked,
    Terminated
};
```

## Namespaces

Use **snake_case** for namespace names:

```cpp
// ✅ Good
namespace kira::system {
namespace kira::display {
namespace kira::memory {
namespace kira::interrupts {

// ❌ Bad
namespace kira::System {
namespace kira::Display {
namespace kira::Memory {
```

## Files and Directories

### Header Files

Use **snake_case** with `.hpp` extension:

```
// ✅ Good
memory_manager.hpp
process_manager.hpp
elf_loader.hpp
virtual_memory.hpp

// ❌ Bad
MemoryManager.hpp
ProcessManager.hpp
ElfLoader.hpp
VirtualMemory.hpp
```

### Source Files

Use **snake_case** with `.cpp` extension:

```
// ✅ Good
memory_manager.cpp
process_manager.cpp
elf_loader.cpp
virtual_memory.cpp

// ❌ Bad
MemoryManager.cpp
ProcessManager.cpp
ElfLoader.cpp
VirtualMemory.cpp
```

### Directory Structure

Use **snake_case** for directory names:

```
// ✅ Good
include/
├── core/
├── memory/
├── arch/x86/
├── interrupts/
├── drivers/
└── display/

kernel/
├── core/
├── memory/
├── arch/x86/
├── interrupts/
├── drivers/
└── display/

// ❌ Bad
include/
├── Core/
├── Memory/
├── Arch/X86/
```

## Macros

Use **UPPER_CASE_WITH_UNDERSCORES** for macros:

```cpp
// ✅ Good
#define KERNEL_STRUCTURES_BASE  0x00200000
#define PAGE_ALIGN(addr) ((addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define ENABLE_INTERRUPT_TESTING

// ❌ Bad
#define kernel_structures_base  0x00200000
#define pageAlign(addr) ((addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define enable_interrupt_testing
```

## Special Cases

### Industry Standard Structures

When implementing industry-standard structures (e.g., ELF, ACPI), **preserve the original naming conventions** to maintain compatibility and recognition:

```cpp
// ✅ Good - ELF Standard Names (Keep as-is)
struct Elf32_Ehdr {
    u8  e_ident[16];        // Keep standard ELF names
    u16 e_type;
    u32 e_entry;
    // ...
};

struct Elf32_Phdr {
    u32 p_type;             // Keep standard ELF names
    u32 p_offset;
    u32 p_vaddr;
    // ...
};

// ❌ Bad - Don't convert standard names
struct Elf32Ehdr {
    u8  eIdent[16];
    u16 eType;
    u32 eEntry;
    // ...
};
```

### Assembly Interface

When interfacing with assembly code, use the naming convention that matches the assembly side:

```cpp
// Assembly interface - match assembly naming
extern "C" void irq_stub_0();
extern "C" void exception_stub_14();
extern "C" void usermode_switch_asm(u32 user_ss, u32 user_esp, u32 user_eflags, u32 user_cs, u32 user_eip);
```

### Hardware Registers and Specifications

For hardware-specific constants and registers, use the official specification names:

```cpp
// ✅ Good - Hardware specification names
const u16 MASTER_PIC_COMMAND = 0x20;
const u16 MASTER_PIC_DATA = 0x21;
const u8 ICW1_INIT = 0x10;
const u8 ICW4_8086 = 0x01;
```

## Examples

### Complete Class Example

```cpp
// file: include/memory/memory_manager.hpp
namespace kira::memory {

class MemoryManager {
private:
    // Member variables - camelCase for multi-word
    MemoryMapEntry* memoryMap;
    u32 memoryMapSize;
    u32* pageDirectory;
    u32* freePageStack;
    u32 freePageCount;
    u32 maxFreePages;
    
    // Constants - UPPER_CASE_WITH_UNDERSCORES
    static constexpr u32 PAGE_SIZE = 4096;
    static constexpr u32 MAX_FREE_PAGES = 1024;

public:
    // Functions - snake_case
    static MemoryManager& get_instance();
    void initialize(const MemoryMapEntry* memoryMap, u32 memoryMapSize);
    void* allocate_physical_page();
    void free_physical_page(void* page);
    bool map_page(void* virtualAddr, void* physicalAddr, bool writable = true);
    void* get_physical_address(void* virtualAddr);
};

} // namespace kira::memory
```

### Complete Function Example

```cpp
// Function with proper variable naming
bool MemoryManager::map_page(void* virtualAddr, void* physicalAddr, bool writable) {
    // Local variables - camelCase for multi-word, single word for simple
    u32 vAddr = reinterpret_cast<u32>(virtualAddr);
    u32 pAddr = reinterpret_cast<u32>(physicalAddr);
    u32 pageIndex = vAddr / PAGE_SIZE;
    u32 tableIndex = pageIndex % PAGE_TABLE_ENTRIES;
    bool isUserPage = (vAddr < KERNEL_SPACE_START);
    
    // Implementation...
    return true;
}
```

## Enforcement

- All new code must follow these conventions
- Code reviews should check for naming convention compliance
- Use automated tools where possible to enforce consistency
- When modifying existing code, update naming to match these conventions

## Rationale

These conventions are designed to:
1. **Maximize readability** - Clear distinction between different identifier types
2. **Maintain consistency** - Uniform patterns throughout the codebase
3. **Respect standards** - Preserve industry-standard naming where appropriate
4. **Enable tooling** - Support automated analysis and refactoring tools
5. **Facilitate collaboration** - Make the codebase accessible to new contributors

---

*This document should be updated as the codebase evolves and new naming patterns emerge.* 