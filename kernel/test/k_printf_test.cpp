#include "test/k_printf_test.hpp"
#include "core/utils.hpp"
#include "display/vga.hpp"

namespace kira::test {

using namespace kira::utils;
using namespace kira::display;

bool KPrintfTest::run_tests() {
    print_section_header("k_printf Tests");
    
    u32 passedTests = 0;
    u32 totalTests = 3;
    
    if (test_basic_formatting()) passedTests++;
    if (test_colored_output()) passedTests++;
    if (test_edge_cases()) passedTests++;
    
    print_section_footer("k_printf Tests", passedTests, totalTests);
    return (passedTests == totalTests);
}

bool KPrintfTest::test_basic_formatting() {
    print_info("Testing basic k_printf formatting...");
    
    // Test basic string output
    k_printf("k_printf basic test: Hello, KiraOS!\n");  // Each k_printf call = new line
    
    // Test integer formatting
    k_printf("Decimal: %d, Unsigned: %u\n", -42, 42u);
    
    // Test hexadecimal formatting  
    k_printf("Hex (lowercase): %x, Hex (uppercase): %X\n", 0xDEAD, 0xBEEF);
    
    // Test string formatting
    k_printf("String: %s\n", "KiraOS");
    
    // Test character formatting
    k_printf("Character: %c\n", 'K');
    
    // Test pointer formatting
    void* testPtr = reinterpret_cast<void*>(0x12345678);
    k_printf("Pointer: %p\n", testPtr);
    
    // Test literal % sign
    k_printf("Percentage: 100%%\n");
    
    // Test printf-style newline behavior
    k_printf("Line start ");                // No newline - stays on same line
    k_printf("+ continued ");               // Continues on same line
    k_printf("+ end\n");                    // \n advances to next line
    k_printf("Multi\nline\ntest\n");        // Multiple lines with proper breaks
    k_printf("No newline at end");          // Stays on current line
    
    print_success("Basic formatting tests completed");
    return true;
}

bool KPrintfTest::test_colored_output() {
    print_info("Testing k_printf colored output...");
    
    // Test colored output
    k_printf_colored(VGA_GREEN_ON_BLUE, "This is GREEN text!\n");
    k_printf_colored(VGA_RED_ON_BLUE, "This is RED text!\n");
    k_printf_colored(VGA_YELLOW_ON_BLUE, "This is YELLOW text!\n");
    k_printf_colored(VGA_CYAN_ON_BLUE, "This is CYAN text!\n");
    
    print_success("Colored output tests completed");
    return true;
}

bool KPrintfTest::test_edge_cases() {
    print_info("Testing k_printf edge cases...");
    
    // Test null string
    k_printf("Null string test: %s\n", (const char*)nullptr);
    
    // Test zero values
    k_printf("Zero values: %d, %u, %x\n", 0, 0u, 0u);
    
    // Test large numbers
    k_printf("Large number: %u, %x\n", 0xFFFFFFFF, 0xFFFFFFFF);
    
    // Test mixed formatting
    void* testPtr = reinterpret_cast<void*>(0xABCDEF12);
    k_printf("Mixed: %s has %d processes at %p with status %x\n", 
             "KiraOS", 5, testPtr, 0xA5);
    
    // Test empty format
    k_printf("Empty format test\n");
    
    print_success("Edge case tests completed");
    return true;
}

} // namespace kira::test