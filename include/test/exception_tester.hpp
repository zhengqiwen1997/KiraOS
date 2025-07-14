#pragma once

namespace kira::test {

class ExceptionTester {
public:
    static void run_all_tests();
    static void run_single_test();
    
private:
    static void log_test_start(const char* test_name);
    static void log_test_end(const char* test_name);
    
    // Individual test functions
    static void test_breakpoint();
    static void test_overflow();
    static void test_bound_range();
    static void test_fpu_not_available();
    static void test_x87_fpu_error();
    static void test_simd_fpu_error();
    static void test_division_by_zero();
    
    // Helper functions
    static void enable_sse();
};

} // namespace kira::test 