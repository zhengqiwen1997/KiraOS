#pragma once

#include "core/process.hpp"
#include "test/test_base.hpp"

namespace kira::test {

/**
 * @brief Process Management and Scheduler Test Suite
 * 
 * Tests the enhanced process management system including:
 * - Priority-based scheduling
 * - Process creation and termination
 * - Sleep queue management
 * - Aging mechanism
 * - Process blocking and waking
 * - Priority management
 */
class ProcessTest : public TestBase {
public:
    /**
     * @brief Run comprehensive process management tests
     * @return true if all tests pass, false otherwise
     */
    bool run_tests();
    
private:
    /**
     * @brief Test basic process creation and termination
     * @return true if successful
     */
    bool test_process_creation_termination();
    
    /**
     * @brief Test priority-based scheduling
     * @return true if successful
     */
    bool test_priority_scheduling();
    
    /**
     * @brief Test sleep queue functionality
     * @return true if successful
     */
    bool test_sleep_queue();
    
    /**
     * @brief Test aging mechanism
     * @return true if successful
     */
    bool test_aging_mechanism();
    
    /**
     * @brief Test process blocking and waking
     * @return true if successful
     */
    bool test_process_blocking_waking();
    
    /**
     * @brief Test priority management
     * @return true if successful
     */
    bool test_priority_management();
    
    /**
     * @brief Test scheduler statistics and display
     * @return true if successful
     */
    bool test_scheduler_statistics();
    
    /**
     * @brief Helper function to create a test process
     * @param name Process name
     * @param priority Process priority
     * @return Process ID or 0 if failed
     */
    u32 create_test_process(const char* name, u32 priority);
    
    /**
     * @brief Helper function to simulate scheduler ticks
     * @param ticks Number of ticks to simulate
     */
    void simulate_scheduler_ticks(u32 ticks);
    
    /**
     * @brief Helper function to verify process state
     * @param pid Process ID
     * @param expectedState Expected process state
     * @return true if state matches
     */
    bool verify_process_state(u32 pid, kira::system::ProcessState expectedState);
    
    /**
     * @brief Helper function to verify process priority
     * @param pid Process ID
     * @param expectedPriority Expected priority
     * @return true if priority matches
     */
    bool verify_process_priority(u32 pid, u32 expectedPriority);
    
    /**
     * @brief Clean up all test processes
     */
    void cleanup_all_test_processes();
};

} // namespace kira::test
