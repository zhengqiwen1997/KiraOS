#pragma once

#include "core/types.hpp"

// Forward declaration to avoid circular dependency
namespace kira::system {
    class AddressSpace;
}

namespace kira::system {

// Process states
enum class ProcessState : u8 {
    READY = 0,      // Ready to run
    RUNNING = 1,    // Currently executing
    BLOCKED = 2,    // Waiting for I/O or event
    TERMINATED = 3  // Finished execution
};

// CPU register state for context switching
struct ProcessContext {
    // General purpose registers
    u32 eax, ebx, ecx, edx;
    u32 esi, edi, esp, ebp;
    
    // Segment registers
    u32 ds, es, fs, gs;
    
    // Program counter and flags
    u32 eip;
    u32 eflags;
    
    // Stack pointers
    u32 kernelEsp;              // Kernel mode stack pointer
    u32 userEsp;                // User mode stack pointer
} __attribute__((packed));

// Process Control Block (PCB)
struct Process {
    u32 pid;                    // Process ID
    char name[32];              // Process name
    ProcessState state;         // Current state
    u32 priority;               // Process priority (0 = highest)
    u32 timeSlice;              // Time quantum for scheduling
    u32 timeUsed;               // Time used in current slice
    
    ProcessContext context;     // Saved CPU state
    
    // Memory information
    u32 kernelStackBase;        // Base of kernel stack
    u32 kernelStackSize;        // Size of kernel stack
    u32 userStackBase;          // Base of user stack  
    u32 userStackSize;          // Size of user stack
    AddressSpace* addressSpace; // Virtual memory address space for this process
    
    // Process function (embedded user program)
    void* userFunction;         // User mode function to execute
    bool isUserMode;            // True if process runs in user mode
    bool hasStarted;            // True if user mode process has been started
    
    // Scheduling information
    u32 creationTime;           // When process was created
    u32 totalCpuTime;           // Total CPU time used
    
    Process* next;              // Next process in queue (for linked list)
} __attribute__((packed));

// Process function type
typedef void (*ProcessFunction)();

/**
 * @brief Process Manager - handles process creation, scheduling, and context switching
 */
class ProcessManager {
private:
    static constexpr u32 MAX_PROCESSES = 16;
    static constexpr u32 DEFAULT_TIME_SLICE = 10;  // Timer ticks
    static constexpr u32 STACK_SIZE = 4096;        // 4KB stack per process
    
    Process processes[MAX_PROCESSES];
    Process* readyQueue;           // Ready processes queue
    Process* currentProcess;       // Currently running process
    u32 nextPid;                  // Next available PID
    u32 processCount;             // Number of active processes
    u32 schedulerTicks;           // Ticks since last schedule
    
    // Static stack allocation - safer than dynamic addresses
    static u8 kernelStacks[MAX_PROCESSES][STACK_SIZE];  // Kernel mode stacks
    static u8 userStacks[MAX_PROCESSES][STACK_SIZE];    // User mode stacks
    u32 nextStackIndex;
    bool isInIdleState;           // Flag to track if we're in idle state
    
public:
    /**
     * @brief Initialize the process manager
     */
    static void initialize();
    
    /**
     * @brief Get the singleton instance
     */
    static ProcessManager& get_instance();
    
    /**
     * @brief Constructor
     */
    ProcessManager();
    
    /**
     * @brief Create a new user mode process
     * @param function Process entry point
     * @param name Process name
     * @param priority Process priority (0 = highest)
     * @return Process ID or 0 if failed
     */
    u32 create_user_process(ProcessFunction function, const char* name, u32 priority = 5);
    
    /**
     * @brief Create a new process (legacy - for compatibility)
     * @param function Process entry point
     * @param name Process name
     * @param priority Process priority (0 = highest)
     * @return Process ID or 0 if failed
     */
    u32 create_process(ProcessFunction function, const char* name, u32 priority = 5);
    
    /**
     * @brief Terminate a process
     * @param pid Process ID to terminate
     * @return true if successful
     */
    bool terminate_process(u32 pid);
    
    /**
     * @brief Schedule next process (called from timer interrupt)
     */
    void schedule();
    
    /**
     * @brief Get current running process
     */
    Process* get_current_process() const { return currentProcess; }
    
    /**
     * @brief Get process by PID
     */
    Process* get_process(u32 pid);
    
    /**
     * @brief Display scheduler statistics
     */
    void display_stats();
    
    /**
     * @brief Display only the current process (for immediate updates)
     */
    void display_current_process_only();
    
    /**
     * @brief Yield CPU to next process (cooperative scheduling)
     */
    void yield();
    
    /**
     * @brief Get current process ID
     */
    u32 get_current_pid() const;
    
    /**
     * @brief Terminate current process
     */
    void terminate_current_process();
    
    /**
     * @brief Sleep current process for specified ticks
     */
    void sleep_current_process(u32 ticks);
    

    
    /**
     * @brief Enable timer-driven scheduling
     */
    static void enable_timer_scheduling();
    
    /**
     * @brief Wake up a blocked process (make it ready to run)
     * @param process Process to wake up
     */
    void wake_up_process(Process* process);

private:
    /**
     * @brief Add process to ready queue
     */
    void add_to_ready_queue(Process* process);
    
    /**
     * @brief Remove process from ready queue
     */
    void remove_from_ready_queue(Process* process);
    
    /**
     * @brief Switch to next ready process
     */
    void switch_process();
    
    /**
     * @brief Initialize process context for user mode
     */
    void init_user_process_context(Process* process, ProcessFunction function);
    
    /**
     * @brief Initialize process context (legacy)
     */
    void init_process_context(Process* process, ProcessFunction function);
    
    /**
     * @brief Allocate stacks for process
     */
    bool allocate_process_stacks(Process* process);
    
    /**
     * @brief Set up memory mapping for user program
     */
    bool setup_user_program_mapping(Process* process, ProcessFunction function);

};

} // namespace kira::system 