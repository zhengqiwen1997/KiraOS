#pragma once

#include "test/test_base.hpp"

namespace kira::test {

class ExceptionTester : public TestBase {
public:
    void run_tests();
    void run_single_test();
    
private:
    void log_test_start(const char* test_name);
    void log_test_end(const char* test_name);
    
    // Individual test functions
    void test_breakpoint();
    void test_overflow();
    void test_bound_range();
    void test_fpu_not_available();
    void test_x87_fpu_error();
    void test_simd_fpu_error();
    void test_division_by_zero();
    
    // Helper functions
    void enable_sse();
};

} // namespace kira::test 