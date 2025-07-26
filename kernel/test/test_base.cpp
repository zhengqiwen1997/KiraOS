#include "test/test_base.hpp"

namespace kira::test {

void TestBase::print_test_result(const char* testName, bool passed) {
    char msg[256];
    strcpy_s(msg, testName, sizeof(msg));
    if (passed) {
        strcat(msg, ": PASSED");
        console.add_message(msg, VGA_GREEN_ON_BLUE);
    } else {
        strcat(msg, ": FAILED");
        console.add_message(msg, VGA_RED_ON_BLUE);
    }
}

void TestBase::print_section_header(const char* sectionName) {
    char header[256];
    strcpy_s(header, "\n=== ", sizeof(header));
    strcat(header, sectionName);
    strcat(header, " ===\n");
    console.add_message(header, VGA_CYAN_ON_BLUE);
}

void TestBase::print_section_footer(const char* sectionName, u32 passed, u32 total) {
    char footer[256];
    strcpy_s(footer, "=== ", sizeof(footer));
    strcat(footer, sectionName);
    strcat(footer, " Summary: ");
    number_to_decimal(footer + strlen(footer), passed);
    strcat(footer, "/");
    number_to_decimal(footer + strlen(footer), total);
    strcat(footer, " tests passed ===\n");
    
    u16 color = (passed == total) ? VGA_GREEN_ON_BLUE : VGA_RED_ON_BLUE;
    console.add_message(footer, color);
}

void TestBase::print_debug(const char* message) {
    console.add_message(message, VGA_YELLOW_ON_BLUE);
}

void TestBase::print_success(const char* message) {
    console.add_message(message, VGA_GREEN_ON_BLUE);
}

void TestBase::print_error(const char* message) {
    console.add_message(message, VGA_RED_ON_BLUE);
}

void TestBase::print_warning(const char* message) {
    console.add_message(message, VGA_YELLOW_ON_BLUE);
}

void TestBase::print_info(const char* message) {
    console.add_message(message, VGA_CYAN_ON_BLUE);
}

void TestBase::print_numbered_message(const char* prefix, u32 number) {
    char msg[256];
    strcpy_s(msg, prefix, sizeof(msg));
    number_to_decimal(msg + strlen(msg), number);
    console.add_message(msg, VGA_LIGHT_GRAY_ON_BLUE);
}

bool TestBase::assert_condition(bool condition, const char* message) {
    if (!condition) {
        print_error(message);
    }
    return condition;
}

} // namespace kira::test 