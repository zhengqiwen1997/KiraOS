#include "test/test_base.hpp"

namespace kira::test {

void TestBase::print_test_result(const char* testName, bool passed) {
    if (passed) {
        k_printf_colored(VGA_GREEN_ON_BLUE, "%s: PASSED\n", testName);
    } else {
        k_printf_colored(VGA_RED_ON_BLUE, "%s: FAILED\n", testName);
    }
}

void TestBase::print_section_header(const char* sectionName) {
    k_printf_colored(VGA_CYAN_ON_BLUE, "\n=== %s ===\n", sectionName);
}

void TestBase::print_section_footer(const char* sectionName, u32 passed, u32 total) {
    u16 color = (passed == total) ? VGA_GREEN_ON_BLUE : VGA_RED_ON_BLUE;
    k_printf_colored(color, "=== %s Summary: %u/%u tests passed ===\n", 
                     sectionName, passed, total);
}

void TestBase::print_debug(const char* message) {
    k_printf_colored(VGA_YELLOW_ON_BLUE, "%s\n", message);
}

void TestBase::print_success(const char* message) {
    k_printf_colored(VGA_GREEN_ON_BLUE, "%s\n", message);
}

void TestBase::print_error(const char* message) {
    k_printf_colored(VGA_RED_ON_BLUE, "%s\n", message);
}

void TestBase::print_warning(const char* message) {
    k_printf_colored(VGA_YELLOW_ON_BLUE, "%s\n", message);
}

void TestBase::print_info(const char* message) {
    k_printf_colored(VGA_CYAN_ON_BLUE, "%s\n", message);
}

bool TestBase::assert_condition(bool condition, const char* message) {
    if (!condition) {
        print_error(message);
    }
    return condition;
}

} // namespace kira::test 