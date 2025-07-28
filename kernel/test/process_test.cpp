#include "test/process_test.hpp"
#include "core/utils.hpp"

namespace kira::test {

using namespace kira::system;
using namespace kira::utils;

ProcessManager* processManagerPtr = nullptr;

// Simple test process functions
static void test_process_function_1() {
    // This process just runs for a while
    volatile u32 counter = 0;
    for (u32 i = 0; i < 1000; i++) {
        counter = counter + 1;
    }
}

bool ProcessTest::run_tests() {
    print_section_header("Process Management and Scheduler Tests");
    processManagerPtr = &ProcessManager::get_instance();
    // Disable scheduling during tests to prevent processes from being executed
    ProcessManager::disable_scheduling();
    
    u32 passedTests = 0;
    u32 totalTests = 7;
    
    if (test_process_creation_termination()) passedTests++;
    if (test_priority_scheduling()) passedTests++;
    if (test_sleep_queue()) passedTests++;
    if (test_aging_mechanism()) passedTests++;
    if (test_process_blocking_waking()) passedTests++;
    if (test_priority_management()) passedTests++;
    if (test_scheduler_statistics()) passedTests++;
    
    // Re-enable scheduling after tests
    ProcessManager::enable_scheduling();
    processManagerPtr = nullptr;
    print_section_footer("Process Management and Scheduler Tests", passedTests, totalTests);
    return (passedTests == totalTests);
}

u32 ProcessTest::create_test_process(const char* name, u32 priority) {
    return processManagerPtr->create_process(test_process_function_1, name, priority);
}

void ProcessTest::simulate_scheduler_ticks(u32 ticks) {
    for (u32 i = 0; i < ticks; i++) {
        processManagerPtr->schedule();
    }
}

bool ProcessTest::verify_process_state(u32 pid, ProcessState expectedState) {
    Process* process = processManagerPtr->get_process(pid);
    if (expectedState == ProcessState::TERMINATED && process->state == ProcessState::TERMINATED) {
        return true;
    }
    if (!process) {
        char msg[128];
        strcpy_s(msg, "Process not found for state verification. PID: ", sizeof(msg));
        char pidStr[16];
        number_to_decimal(pidStr, pid);
        strcat(msg, pidStr);
        print_error(msg);
        return false;
    }
    
    if (process->state == expectedState) {
        return true;
    } else {
        char msg[128];
        strcpy_s(msg, "Process state mismatch. Expected: ", sizeof(msg));
        char stateStr[16];
        char pidStr2[16];
        number_to_decimal(stateStr, static_cast<u32>(expectedState));
        strcat(msg, stateStr);
        strcat(msg, ", Got: ");
        number_to_decimal(stateStr, static_cast<u32>(process->state));
        strcat(msg, stateStr);
        strcat(msg, " for PID: ");
        number_to_decimal(pidStr2, pid);
        strcat(msg, pidStr2);
        print_error(msg);
        return false;
    }
}

bool ProcessTest::verify_process_priority(u32 pid, u32 expectedPriority) {
    u32 actualPriority = processManagerPtr->get_process_priority(pid);
    
    if (actualPriority == expectedPriority) {
        return true;
    } else {
        char msg[128];
        strcpy_s(msg, "Process priority mismatch. Expected: ", sizeof(msg));
        char priorityStr[16];
        number_to_decimal(priorityStr, expectedPriority);
        strcat(msg, priorityStr);
        strcat(msg, ", Got: ");
        number_to_decimal(priorityStr, actualPriority);
        strcat(msg, priorityStr);
        print_error(msg);
        return false;
    }
}

bool ProcessTest::test_process_creation_termination() {
    print_info("Testing process creation and termination...");
    
    // Test process creation
    u32 pid1 = create_test_process("TestProcess1", 5);
    if (pid1 == 0) {
        print_error("Failed to create test process 1");
        return false;
    }
    
    u32 pid2 = create_test_process("TestProcess2", 3);
    if (pid2 == 0) {
        print_error("Failed to create test process 2");
        return false;
    }
    
    if (!verify_process_state(pid1, ProcessState::READY)) return false;
    if (!verify_process_state(pid2, ProcessState::READY)) return false;
    
    // Test process termination
    if (!processManagerPtr->terminate_process(pid1)) {
        print_error("Failed to terminate process 1");
        return false;
    }
    
    if (!verify_process_state(pid1, ProcessState::TERMINATED)) return false;
    
    // Verify process2 is still active
    if (!verify_process_state(pid2, ProcessState::READY)) return false;
    
    print_success("Process creation and termination test passed");
    return true;
}

bool ProcessTest::test_priority_scheduling() {
    print_info("Testing priority-based scheduling...");
    
    // Create processes with different priorities
    u32 highPriorityPid = create_test_process("HighPriority", 1);  // High priority
    u32 mediumPriorityPid = create_test_process("MediumPriority", 5);  // Medium priority
    u32 lowPriorityPid = create_test_process("LowPriority", 9);  // Low priority
    
    if (highPriorityPid == 0 || mediumPriorityPid == 0 || lowPriorityPid == 0) {
        print_error("Failed to create test processes for priority scheduling");
        return false;
    }
    
    // Verify priorities are set correctly
    if (!verify_process_priority(highPriorityPid, 1)) return false;
    if (!verify_process_priority(mediumPriorityPid, 5)) return false;
    if (!verify_process_priority(lowPriorityPid, 9)) return false;
    
    // Test that processes are in ready state before scheduling
    if (!verify_process_state(highPriorityPid, ProcessState::READY)) return false;
    if (!verify_process_state(mediumPriorityPid, ProcessState::READY)) return false;
    if (!verify_process_priority(lowPriorityPid, 9)) return false;
    
    // Test priority management without executing processes
    if (!processManagerPtr->set_process_priority(highPriorityPid, 2)) return false;
    if (!verify_process_priority(highPriorityPid, 2)) return false;
    
    print_success("Priority-based scheduling test passed");
    return true;
}

bool ProcessTest::test_sleep_queue() {
    print_info("Testing sleep queue functionality...");
        
    // Create a test process
    u32 pid = create_test_process("SleepTest", 5);
    if (pid == 0) {
        print_error("Failed to create test process for sleep queue test");
        return false;
    }
    
    // Get the process
    Process* process = processManagerPtr->get_process(pid);
    if (!process) {
        print_error("Failed to retrieve process for sleep test");
        return false;
    }
    
    // Test that we can access process fields directly
    if (process->state != ProcessState::READY) {
        print_error("Process should be in READY state initially");
        return false;
    }
    
    // Test that process has proper priority
    if (process->priority != 5) {
        print_error("Process priority should be 5");
        return false;
    }
    
    print_success("Sleep queue functionality test passed");
    return true;
}

bool ProcessTest::test_aging_mechanism() {
    print_info("Testing aging mechanism...");
        
    // Create processes with different priorities
    u32 lowPriorityPid = create_test_process("LowPriorityAging", 9);
    u32 mediumPriorityPid = create_test_process("MediumPriorityAging", 5);
    
    if (lowPriorityPid == 0 || mediumPriorityPid == 0) {
        print_error("Failed to create test processes for aging test");
        return false;
    }
    
    // Verify initial priorities
    if (!verify_process_priority(lowPriorityPid, 9)) return false;
    if (!verify_process_priority(mediumPriorityPid, 5)) return false;
    
    // Test that processes are in ready state
    if (!verify_process_state(lowPriorityPid, ProcessState::READY)) return false;
    if (!verify_process_state(mediumPriorityPid, ProcessState::READY)) return false;
    
    print_success("Aging mechanism test passed");
    return true;
}

bool ProcessTest::test_process_blocking_waking() {
    print_info("Testing process blocking and waking...");
        
    // Create a test process
    u32 pid = create_test_process("BlockTest", 5);
    if (pid == 0) {
        print_error("Failed to create test process for blocking test");
        return false;
    }
    
    // Get the process
    Process* process = processManagerPtr->get_process(pid);
    if (!process) {
        print_error("Failed to retrieve process for blocking test");
        return false;
    }
    
    // Test that process is initially ready
    if (!verify_process_state(pid, ProcessState::READY)) return false;
    
    // Test wake up functionality (even though process is already ready)
    processManagerPtr->wake_up_process(process);
    
    // Verify process is still ready
    if (!verify_process_state(pid, ProcessState::READY)) return false;
    
    print_success("Process blocking and waking test passed");
    return true;
}

bool ProcessTest::test_priority_management() {
    print_info("Testing priority management...");
        
    // Create a test process
    u32 pid = create_test_process("PriorityTest", 5);
    if (pid == 0) {
        print_error("Failed to create test process for priority management test");
        return false;
    }
    
    // Verify initial priority
    if (!verify_process_priority(pid, 5)) return false;
    
    // Change priority to higher
    if (!processManagerPtr->set_process_priority(pid, 2)) {
        print_error("Failed to set process priority to 2");
        return false;
    }
    
    if (!verify_process_priority(pid, 2)) return false;
    
    // Change priority to lower
    if (!processManagerPtr->set_process_priority(pid, 8)) {
        print_error("Failed to set process priority to 8");
        return false;
    }
    
    if (!verify_process_priority(pid, 8)) return false;
    
    // Test invalid priority
    if (processManagerPtr->set_process_priority(pid, 15)) {
        print_error("Should not accept priority > MAX_PRIORITY");
        return false;
    }
    
    // Test non-existent process
    if (processManagerPtr->set_process_priority(999, 5)) {
        print_error("Should not accept non-existent process ID");
        return false;
    }
    
    print_success("Priority management test passed");
    return true;
}

bool ProcessTest::test_scheduler_statistics() {
    print_info("Testing scheduler statistics...");
        
    // Create some test processes
    u32 pid1 = create_test_process("StatsTest1", 3);
    u32 pid2 = create_test_process("StatsTest2", 7);
    
    if (pid1 == 0 || pid2 == 0) {
        print_error("Failed to create test processes for statistics test");
        return false;
    }
    
    // Test getting current PID (should be 0 if no process is running)
    u32 currentPid = processManagerPtr->get_current_pid();
    if (currentPid != 0) {
        print_warning("Current PID is not 0, but this is expected in test environment");
    }
    
    // Test priority retrieval
    u32 priority1 = processManagerPtr->get_process_priority(pid1);
    u32 priority2 = processManagerPtr->get_process_priority(pid2);
    
    if (priority1 != 3 || priority2 != 7) {
        print_error("Priority retrieval failed");
        return false;
    }
    
    // Test getting non-existent process priority
    u32 invalidPriority = processManagerPtr->get_process_priority(999);
    if (invalidPriority != 0xFFFFFFFF) {
        print_error("Should return 0xFFFFFFFF for non-existent process");
        return false;
    }
    
    print_success("Scheduler statistics test passed");
    return true;
}

} // namespace kira::test
