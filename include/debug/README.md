# SerialDebugger - Kernel Debug Logging

Simple serial port debugging utility for KiraOS kernel development.

## Usage

```cpp
#include "debug/serial_debugger.hpp"

// Initialize once during boot
kira::debug::SerialDebugger::init();

// Output methods
SerialDebugger::print("Hello");           // String
SerialDebugger::println("With newline");  // String + newline
SerialDebugger::print_hex(0x1234);        // Hex: 0x00001234
SerialDebugger::print_dec(42);            // Decimal: 42
```

## Example

```cpp
u32 addr = 0x100000;
SerialDebugger::print("Address: ");
SerialDebugger::print_hex(addr);
SerialDebugger::println("");
```

## Viewing Output

**QEMU**: Add `-serial stdio` to see output in terminal
**Hardware**: COM1 port at 38400 baud, 8N1

## Notes

- Use early in boot process for debugging memory/paging issues
- Not thread-safe
- Slower than VGA output 