#pragma once
#include "test/test_base.hpp"

namespace kira::test {

/**
 * @brief Test suite for k_printf functionality
 */
class KPrintfTest : public TestBase {
public:
    /**
     * @brief Run all k_printf tests
     * @return true if all tests passed
     */
    static bool run_tests();

private:
    /**
     * @brief Test basic k_printf formatting
     * @return true if test passed
     */
    static bool test_basic_formatting();
    
    /**
     * @brief Test k_printf colored output
     * @return true if test passed
     */
    static bool test_colored_output();
    
    /**
     * @brief Test k_printf edge cases
     * @return true if test passed
     */
    static bool test_edge_cases();
};

} // namespace kira::test