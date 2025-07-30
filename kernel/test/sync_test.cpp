#include "test/sync_test.hpp"
#include "core/sync.hpp"
#include "core/process.hpp"

namespace kira::test {

using namespace kira::sync;
using namespace kira::system;

// Global test variables
static Spinlock g_testSpinlock;
static Mutex g_testMutex;
static Semaphore* g_testSemaphore = nullptr; // Will be initialized in run_tests
static u32 g_sharedCounter = 0;

bool SyncTest::run_tests() {
    print_section_header("Synchronization Tests");
    
    // Initialize semaphore
    g_testSemaphore = new Semaphore(2);
    
    u32 passedTests = 0;
    u32 totalTests = 4;
    
    if (test_spinlock_basic()) passedTests++;
    if (test_mutex_basic()) passedTests++;
    if (test_semaphore_basic()) passedTests++;
    if (test_lock_guard()) passedTests++;
    
    // Clean up
    delete g_testSemaphore;
    g_testSemaphore = nullptr;
    
    print_section_footer("Synchronization Tests", passedTests, totalTests);
    return (passedTests == totalTests);
}

bool SyncTest::test_spinlock_basic() {
    bool testPassed = true;
    
    print_info("Testing basic spinlock operations...");
    
    // Test 1: Basic lock/unlock
    g_testSpinlock.lock();
    print_debug("Spinlock acquired");
    g_testSpinlock.unlock();
    print_debug("Spinlock released");
    
    // Test 2: try_lock when available
    if (g_testSpinlock.try_lock()) {
        print_success("try_lock succeeded when lock available");
        g_testSpinlock.unlock();
    } else {
        print_error("try_lock failed when lock should be available");
        testPassed = false;
    }
    
    // Test 3: Nested lock guard usage
    {
        SpinlockGuard guard(g_testSpinlock);
        print_debug("Inside spinlock guard scope");
        
        // Try to acquire lock - should fail since we already have it
        if (!g_testSpinlock.try_lock()) {
            print_success("try_lock correctly failed when lock held");
        } else {
            print_error("try_lock incorrectly succeeded when lock held");
            testPassed = false;
            g_testSpinlock.unlock(); // Clean up
        }
    } // Lock should be released here
    
    // Test 4: Lock should be available again
    if (g_testSpinlock.try_lock()) {
        print_success("Lock correctly available after guard scope");
        g_testSpinlock.unlock();
    } else {
        print_error("Lock incorrectly unavailable after guard scope");
        testPassed = false;
    }
    
    print_test_result("Spinlock Basic Operations", testPassed);
    return testPassed;
}

bool SyncTest::test_mutex_basic() {
    bool testPassed = true;
    
    print_info("Testing basic mutex operations...");
    
    // Test 1: Basic lock/unlock
    g_testMutex.lock();
    print_debug("Mutex acquired");
    g_testMutex.unlock();
    print_debug("Mutex released");
    
    // Test 2: try_lock when available
    if (g_testMutex.try_lock()) {
        print_success("Mutex try_lock succeeded when available");
        g_testMutex.unlock();
    } else {
        print_error("Mutex try_lock failed when should be available");
        testPassed = false;
    }
    
    // Test 3: RAII guard usage
    {
        MutexGuard guard(g_testMutex);
        print_debug("Inside mutex guard scope");
        
        // Try to acquire mutex - should fail
        if (!g_testMutex.try_lock()) {
            print_success("Mutex try_lock correctly failed when held");
        } else {
            print_error("Mutex try_lock incorrectly succeeded when held");
            testPassed = false;
            g_testMutex.unlock(); // Clean up
        }
    } // Mutex should be released here
    
    // Test 4: Mutex should be available again
    if (g_testMutex.try_lock()) {
        print_success("Mutex correctly available after guard scope");
        g_testMutex.unlock();
    } else {
        print_error("Mutex incorrectly unavailable after guard scope");
        testPassed = false;
    }
    
    print_test_result("Mutex Basic Operations", testPassed);
    return testPassed;
}

bool SyncTest::test_semaphore_basic() {
    bool testPassed = true;
    
    print_info("Testing basic semaphore operations...");
    
    // Debug: Print actual count
    printf_info("Semaphore count: %u\n", g_testSemaphore->get_count());
    
    // Test 1: Initial count should be 2
    if (g_testSemaphore->get_count() == 2) {
        print_success("Semaphore initial count correct");
    } else {
        print_error("Semaphore initial count incorrect");
        testPassed = false;
    }
    
    // Test 2: try_wait should succeed twice
    print_debug("About to try first try_wait...");
    bool firstResult = g_testSemaphore->try_wait();
    printf_info("First try_wait result: %u\n", firstResult);
    
    if (firstResult) {
        print_success("First try_wait succeeded");
        print_debug("About to try second try_wait...");
        bool secondResult = g_testSemaphore->try_wait();
        printf_info("Second try_wait result: %u\n", secondResult);
        
        if (secondResult) {
            print_success("Second try_wait succeeded");
            
            // Test 3: Third try_wait should fail (count = 0)
            if (!g_testSemaphore->try_wait()) {
                print_success("Third try_wait correctly failed");
            } else {
                print_error("Third try_wait incorrectly succeeded");
                testPassed = false;
            }
        } else {
            print_error("Second try_wait failed");
            testPassed = false;
        }
    } else {
        print_error("First try_wait failed");
        testPassed = false;
    }
    
    // Test 4: Signal should restore resources
    g_testSemaphore->signal();
    g_testSemaphore->signal();
    
    if (g_testSemaphore->get_count() == 2) {
        print_success("Semaphore count restored after signals");
    } else {
        print_error("Semaphore count not restored properly");
        testPassed = false;
    }
    
    print_test_result("Semaphore Basic Operations", testPassed);
    return testPassed;
}

bool SyncTest::test_lock_guard() {
    bool testPassed = true;
    
    print_info("Testing RAII lock guards...");
    
    // Test RAII behavior with scope
    {
        SpinlockGuard guard(g_testSpinlock);
        print_debug("Lock acquired via RAII guard");
        
        // Test that lock is held
        if (!g_testSpinlock.try_lock()) {
            print_success("Lock correctly held by guard");
        } else {
            print_error("Lock incorrectly available while guarded");
            testPassed = false;
            g_testSpinlock.unlock(); // Clean up
        }
    } // Guard destructor should release lock here
    
    // Test that lock is released after scope
    if (g_testSpinlock.try_lock()) {
        print_success("Lock correctly released after guard scope");
        g_testSpinlock.unlock();
    } else {
        print_error("Lock incorrectly held after guard scope");
        testPassed = false;
    }
    
    print_test_result("RAII Lock Guards", testPassed);
    return testPassed;
}

} // namespace kira::test 