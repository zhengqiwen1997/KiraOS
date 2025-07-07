#include "test/exception_tester.hpp"
#include "core/utils.hpp"
#include "display/console.hpp"

// Forward declarations
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::test {

using namespace kira::display;
using namespace kira::utils;

// ScrollableConsole& get_console() {
//     return kira::kernel::console;
// }

void ExceptionTester::log_test_start(const char* test_name) {
    char msg[256];
    utils::strcpy(msg, "Testing ");
    utils::strcat(msg, test_name);
    utils::strcat(msg, "...");
    kira::kernel::console.add_message(msg, VGA_CYAN_ON_BLUE);
}

void ExceptionTester::log_test_end(const char* test_name) {
    char msg[256];
    utils::strcpy(msg, test_name);
    utils::strcat(msg, " completed!");
    kira::kernel::console.add_message(msg, VGA_GREEN_ON_BLUE);
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

void ExceptionTester::run_all_tests() {
    kira::kernel::console.add_message("=== EXCEPTION TESTING ===", VGA_WHITE_ON_BLUE);
    
    //test_division_by_zero();
    test_breakpoint();
    test_overflow();
    test_bound_range();
    test_fpu_not_available();
    test_x87_fpu_error();
    test_simd_fpu_error();
    
    kira::kernel::console.add_message("=== ALL EXCEPTION TESTS PASSED ===", VGA_WHITE_ON_BLUE);
}

void ExceptionTester::run_single_test() {
    kira::kernel::console.add_message("=== SINGLE EXCEPTION TEST ===", VGA_WHITE_ON_BLUE);
    test_division_by_zero();
    kira::kernel::console.add_message("=== SINGLE EXCEPTION TEST PASSED ===", VGA_WHITE_ON_BLUE);
}

} // namespace kira::test 