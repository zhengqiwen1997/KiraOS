#pragma once

// Test configuration - uncomment to enable interrupt testing
#define ENABLE_EXCEPTION_TESTING  // DISABLED FOR DEBUGGING
// #define ENABLE_SINGLE_EXCEPTION_TEST  // Enable single exception test only

namespace kira::kernel {

class ExceptionTester {
private:
    static void log_test_start(const char* test_name);
    static void log_test_end(const char* test_name);
    static void enable_sse();

public:
    static void test_breakpoint();
    static void test_overflow();
    static void test_bound_range();
    static void test_fpu_not_available();
    static void test_x87_fpu_error();
    static void test_simd_fpu_error();
    static void test_division_by_zero();
    
    static void run_all_tests();
    static void run_single_test();
};

} // namespace kira::kernel 