#pragma once

#include "core/types.hpp"
#include "core/sync.hpp"

// Forward declaration to avoid circular dependency
namespace kira::system {
    class AddressSpace;
}

namespace kira::system {

using namespace kira::sync;

// Special sentinel for "wait any child"
constexpr u32 WAIT_ANY_CHILD = 0xFFFFFFFFu;

// Enhanced process states
enum class ProcessState : u8 {
    READY = 0,          // Ready to run
    RUNNING = 1,        // Currently executing
    BLOCKED = 2,        // Waiting for I/O or event
    SLEEPING = 3,       // Sleeping for a specific time
    ZOMBIE = 4,         // Exited, awaiting parent to reap
    TERMINATED = 5,     // Fully reaped, PCB slot free pending
    WAITING = 6         // Waiting for synchronization primitive
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
    // User heap region [heapStart, heapEnd). Grows upward from USER_HEAP_START
    u32 heapStart;
    u32 heapEnd;
    
    // Process function (embedded user program)
    void* userFunction;         // User mode function to execute
    bool isUserMode;            // True if process runs in user mode
    bool hasStarted;            // True if user mode process has been started
    
    // Enhanced scheduling information
    u32 creationTime;           // When process was created
    u32 totalCpuTime;           // Total CPU time used
    u32 sleepUntil;             // Time when sleep should end
    u32 age;                    // Age for starvation prevention
    u32 lastRunTime;            // Last time process ran
    
    // Queue pointers
    Process* next;              // Next process in queue (for linked list)
    Process* prev;              // Previous process in queue (for doubly-linked list)

    // Saved kernel stack frame ESP captured inside syscall stub when the
    // process enters the kernel via INT 0x80. Used to resume exactly at
    // the instruction following the syscall when the process is scheduled again.
    u32 savedSyscallEsp;

    // Pending system call return value to be returned via resume_from_syscall_stack
    u32 pendingSyscallReturn;

    // Current working directory (user-visible path)
    char currentWorkingDirectory[256];

    // Spawn argument buffer (temporary exec/spawn argument passing)
    char spawnArg[256];

    // Waiting pid for WAIT syscall (0 = none)
    u32 waitingOnPid;
    // If non-zero, WAITID will write exit status here upon wake
    u32 waitStatusUserPtr;
    // Parent process ID (0 if no parent)
    u32 parentPid;

    // Pending completed child info to handle races (ANY waits)
    u32 pendingChildPid;
    i32 pendingChildStatus;

    // Exit status code set at termination (valid when state == TERMINATED)
    i32 exitStatus;

    // Whether this child has already been reported to a waiter (WAIT/WAITID)
    bool hasBeenWaited;
} __attribute__((packed));

// Process function type
typedef void (*ProcessFunction)();

// Priority queue node
struct PriorityQueue {
    Process* head;
    Process* tail;
    u32 count;
    
    PriorityQueue() : head(nullptr), tail(nullptr), count(0) {}
};

/**
 * @brief Enhanced Process Manager - handles process creation, scheduling, and context switching
 */
class ProcessManager {
private:
    static constexpr u32 MAX_PROCESSES = 16;
    static constexpr u32 DEFAULT_TIME_SLICE = 10;  // Timer ticks
    static constexpr u32 STACK_SIZE = 4096;        // 4KB stack per process
    static constexpr u32 MAX_PRIORITY = 10;        // Maximum priority level
    static constexpr u32 AGING_INTERVAL = 100;     // Ticks between aging
    
    Process processes[MAX_PROCESSES];
    PriorityQueue readyQueues[MAX_PRIORITY + 1];   // Priority queues
    Process* sleepQueue;                           // Sleeping processes
    Process* inputWaitQueue;                       // Processes waiting for keyboard input
    Process* currentProcess;                       // Currently running process
    u32 nextPid;                                  // Next available PID
    u32 processCount;                             // Number of active processes
    u32 schedulerTicks;                           // Ticks since last schedule
    u32 lastAgingTime;                            // Last time aging was performed
    u32 lastDisplayedPid;                         // For throttling 'Current:' output
    
    // Synchronization
    Mutex schedulerMutex;                         // Protects scheduler data structures
    Spinlock contextSwitchLock;                   // Protects context switching
    
    // Static stack allocation - safer than dynamic addresses
    static u8 kernelStacks[MAX_PROCESSES][STACK_SIZE];  // Kernel mode stacks
    static u8 userStacks[MAX_PROCESSES][STACK_SIZE];    // User mode stacks
    u32 nextStackIndex;
    bool isInIdleState;                           // Flag to track if we're in idle state
    bool contextSwitchDeferred;                   // Defer context switch from IRQ while waiting in syscall
    
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
     * @brief Create a new user mode process from an existing address space and entry point (ELF)
     * @param addressSpace Pre-built user address space containing program segments and stack
     * @param entryPoint Virtual entry point address inside the address space
     * @param userStackTop Virtual user stack top (ESP on first entry)
     * @param name Process name
     * @param priority Process priority (0 = highest)
     * @return Process ID or 0 if failed
     */
    u32 create_user_process_from_elf(AddressSpace* addressSpace, u32 entryPoint, u32 userStackTop, const char* name, u32 priority = 5);
    
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
    /** Find a terminated child of parentPid, or nullptr if none */
    Process* find_terminated_child(u32 parentPid);
    /** True if parentPid currently has any children (any state) */
    bool has_child(u32 parentPid) const;
    
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
     * @brief Terminate current process with an exit status
     * @param status Exit status to record for parent waiters
     */
    void terminate_current_process_with_status(i32 status);
    
    /**
     * @brief Sleep current process for specified ticks
     */
    void sleep_current_process(u32 ticks);
    
    /**
     * @brief Enable timer-driven scheduling
     */
    static void enable_timer_scheduling();
    
    /**
     * @brief Disable scheduling (for testing)
     */
    static void disable_scheduling();
    
    /**
     * @brief Enable scheduling (for testing)
     */
    static void enable_scheduling();
    
    /**
     * @brief Check if scheduling is disabled
     */
    static bool is_scheduling_disabled();

    // Defer/resume context switches (used by syscall sleep/yield paths)
    void set_defer_context_switch(bool defer) { contextSwitchDeferred = defer; }
    bool is_context_switch_deferred() const { return contextSwitchDeferred; }
    
    /**
     * @brief Wake up a blocked process (make it ready to run)
     * @param process Process to wake up
     */
    void wake_up_process(Process* process);
    
    /**
     * @brief Block current process (waiting for event)
     */
    void block_current_process();

    // Block current process waiting for keyboard input
    void block_current_process_for_input();
    // Deliver one input character to a waiting process (if any). Returns true if delivered
    bool deliver_input_char(char ch);

    /** Reap a terminated child process: free its PCB slot (pid->0) after parent collected status */
    void reap_child(Process* child);
    
    /**
     * @brief Set process priority
     * @param pid Process ID
     * @param priority New priority
     * @return true if successful
     */
    bool set_process_priority(u32 pid, u32 priority);
    
    /**
     * @brief Get process priority
     * @param pid Process ID
     * @return Priority or 0xFFFFFFFF if not found
     */
    u32 get_process_priority(u32 pid) const;

    // Minimal fork: duplicate PCB, copy stacks, share address space (refcount TBD)
    u32 fork_current_process();
private:
    /**
     * @brief Add process to ready queue
     */
    void add_to_ready_queue(Process* process);
    
    /**
     * @brief Remove process from ready queue
     * @param process Process to remove
     */
    void remove_from_ready_queue(Process* process);
    
    /**
     * @brief Add process to sleep queue
     * @param process Process to add
     * @param sleepTicks Number of ticks to sleep
     */
    void add_to_sleep_queue(Process* process, u32 sleepTicks);
    
    /**
     * @brief Process sleep queue and wake up expired processes
     */
    void process_sleep_queue();
    
    /**
     * @brief Perform aging to prevent starvation
     */
    void perform_aging();
    
    /**
     * @brief Get next process from priority queues
     * @return Next process to run or nullptr
     */
    Process* get_next_process();
    
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