#pragma once
#include "test/test_base.hpp"

namespace kira::test {

/**
 * @brief Test suite for synchronization primitives
 */
class SyncTest : public TestBase {
public:
    /**
     * @brief Run all synchronization tests
     * @return true if all tests passed
     */
    static bool run_tests();

private:
    /**
     * @brief Test basic spinlock operations
     * @return true if test passed
     */
    static bool test_spinlock_basic();
    
    /**
     * @brief Test basic mutex operations
     * @return true if test passed
     */
    static bool test_mutex_basic();
    
    /**
     * @brief Test basic semaphore operations
     * @return true if test passed
     */
    static bool test_semaphore_basic();
    
    /**
     * @brief Test RAII lock guards
     * @return true if test passed
     */
    static bool test_lock_guard();
    

};

} // namespace kira::test 