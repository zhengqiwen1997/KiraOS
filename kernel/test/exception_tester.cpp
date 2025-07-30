#include "test/exception_tester.hpp"
#include "core/utils.hpp"
#include "test/test_base.hpp"

namespace kira::test {

using namespace kira::utils;


void ExceptionTester::log_test_start(const char* test_name) {
    printf_info("Testing %s...\n", test_name);
}

void ExceptionTester::log_test_end(const char* test_name) {
    printf_success("%s completed!\n", test_name);
}

void ExceptionTester::test_breakpoint() {
    log_test_start("breakpoint exception");
    asm volatile("int $3");
    log_test_end("breakpoint exception");
}

void ExceptionTester::test_overflow() {
    log_test_start("overflow exception");
    asm volatile("int $4");
    log_test_end("overflow exception");
}

void ExceptionTester::test_bound_range() {
    log_test_start("bound range exception");
    asm volatile("int $5");
    log_test_end("bound range exception");
}

void ExceptionTester::test_fpu_not_available() {
    log_test_start("FPU not available exception");
    asm volatile("int $7");
    log_test_end("FPU not available exception");
}

void ExceptionTester::test_x87_fpu_error() {
    log_test_start("x87 FPU error exception");
    asm volatile("int $16");
    log_test_end("x87 FPU error exception");
}

void ExceptionTester::test_simd_fpu_error() {
    log_test_start("SIMD FPU error exception");
    asm volatile("int $19");
    log_test_end("SIMD FPU error exception");
}

void ExceptionTester::test_division_by_zero() {
    log_test_start("division by zero");
    
    volatile int dividend = 10;
    volatile int divisor = 0;
    
    asm volatile(
        "movl %1, %%eax\n"
        "movl %2, %%ebx\n"
        "divl %%ebx\n"
        : 
        : "m"(dividend), "m"(dividend), "m"(divisor)
        : "eax", "ebx", "edx"
    );
    //asm volatile("int $0");
    log_test_end("division by zero");
}

void ExceptionTester::run_tests() {
    print_section_header("Exception Testing");
    
    //test_division_by_zero();
    test_breakpoint();
    test_overflow();
    test_bound_range();
    test_fpu_not_available();
    test_x87_fpu_error();
    test_simd_fpu_error();
    
    print_section_footer("Exception Testing", 6, 6);
}

void ExceptionTester::run_single_test() {
    print_section_header("Single Exception Test");
    test_division_by_zero();
    print_section_footer("Single Exception Test", 1, 1);
}

} // namespace kira::test 