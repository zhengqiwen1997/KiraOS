#pragma once

#include "core/types.hpp"

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
    
    // Stack pointer for kernel stack
    u32 kernel_esp;
} __attribute__((packed));

// Process Control Block (PCB)
struct Process {
    u32 pid;                    // Process ID
    char name[32];              // Process name
    ProcessState state;         // Current state
    u32 priority;               // Process priority (0 = highest)
    u32 time_slice;             // Time quantum for scheduling
    u32 time_used;              // Time used in current slice
    
    ProcessContext context;     // Saved CPU state
    
    // Memory information
    u32 stack_base;             // Base of process stack
    u32 stack_size;             // Size of stack
    
    // Scheduling information
    u32 creation_time;          // When process was created
    u32 total_cpu_time;         // Total CPU time used
    
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
    Process* ready_queue;           // Ready processes queue
    Process* current_process;       // Currently running process
    u32 next_pid;                  // Next available PID
    u32 process_count;             // Number of active processes
    u32 scheduler_ticks;           // Ticks since last schedule
    
    // Static stack allocation - safer than dynamic addresses
    static u8 process_stacks[MAX_PROCESSES][STACK_SIZE];
    u32 next_stack_index;
    
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
     * @brief Create a new process
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
    Process* get_current_process() const { return current_process; }
    
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
     * @brief Initialize process context
     */
    void init_process_context(Process* process, ProcessFunction function);
    
    /**
     * @brief Allocate stack for process
     */
    u32 allocate_stack();
    

};

} // namespace kira::system 