#pragma once
#include "display/console.hpp"
#include "core/utils.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::test {

using namespace kira::display;
using namespace kira::utils;
using kira::kernel::console;

/**
 * @brief Base class for all test suites
 * Provides common functionality like result reporting and test organization
 */
class TestBase {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~TestBase() = default;
    
    /**
     * @brief Run all tests in this test suite
     * @return true if all tests passed
     */
    static bool run_tests();

protected:
    /**
     * @brief Print test result with consistent formatting
     * @param testName Name of the test
     * @param passed Whether the test passed
     */
    static void print_test_result(const char* testName, bool passed);
    
    /**
     * @brief Print test section header
     * @param sectionName Name of the test section
     */
    static void print_section_header(const char* sectionName);
    
    /**
     * @brief Print test section footer with summary
     * @param sectionName Name of the test section
     * @param passed Number of tests that passed
     * @param total Total number of tests
     */
    static void print_section_footer(const char* sectionName, u32 passed, u32 total);
    
    /**
     * @brief Print debug message with consistent formatting
     * @param message Debug message to print
     */
    static void print_debug(const char* message);
    
    /**
     * @brief Print success message with consistent formatting
     * @param message Success message to print
     */
    static void print_success(const char* message);
    
    /**
     * @brief Print error message with consistent formatting
     * @param message Error message to print
     */
    static void print_error(const char* message);
    
    /**
     * @brief Print warning message with consistent formatting
     * @param message Warning message to print
     */
    static void print_warning(const char* message);
    
    /**
     * @brief Print info message with consistent formatting
     * @param message Info message to print
     */
    static void print_info(const char* message);

    /**
     * @brief Assert a condition and print result
     * @param condition Condition to check
     * @param message Message to print if condition fails
     * @return true if condition is true, false otherwise
     */
    static bool assert_condition(bool condition, const char* message);
    
    /**
     * @brief Assert equality and print result
     * @param expected Expected value
     * @param actual Actual value
     * @param message Message to print if values don't match
     * @return true if values match, false otherwise
     */
    template<typename T>
    static bool assert_equal(T expected, T actual, const char* message) {
        bool result = (expected == actual);
        if (!result) {
            k_printf_colored(VGA_RED_ON_BLUE, "%s Expected: %u Actual: %u\n", 
                           message, static_cast<u32>(expected), static_cast<u32>(actual));
        }
        return result;
    }
    
    /**
     * @brief Assert not equal and print result
     * @param expected Expected value (should not match)
     * @param actual Actual value
     * @param message Message to print if values match
     * @return true if values don't match, false otherwise
     */
    template<typename T>
    static bool assert_not_equal(T expected, T actual, const char* message) {
        bool result = (expected != actual);
        if (!result) {
            k_printf_colored(VGA_RED_ON_BLUE, "%s Both values are: %u\n", 
                           message, static_cast<u32>(expected));
        }
        return result;
    }
};

// Printf-style convenience macros for test output
#define printf_debug(format, ...) \
    kira::utils::k_printf_colored(kira::display::VGA_YELLOW_ON_BLUE, format, ##__VA_ARGS__)

#define printf_success(format, ...) \
    kira::utils::k_printf_colored(kira::display::VGA_GREEN_ON_BLUE, format, ##__VA_ARGS__)

#define printf_error(format, ...) \
    kira::utils::k_printf_colored(kira::display::VGA_RED_ON_BLUE, format, ##__VA_ARGS__)

#define printf_warning(format, ...) \
    kira::utils::k_printf_colored(kira::display::VGA_YELLOW_ON_BLUE, format, ##__VA_ARGS__)

#define printf_info(format, ...) \
    kira::utils::k_printf_colored(kira::display::VGA_CYAN_ON_BLUE, format, ##__VA_ARGS__)

} // namespace kira::test 