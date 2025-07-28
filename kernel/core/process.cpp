#include "core/types.hpp"
#include "core/process.hpp"
#include "core/utils.hpp"
#include "core/usermode.hpp"
#include "display/vga.hpp"
#include "display/console.hpp"
#include "arch/x86/tss.hpp"
#include "memory/virtual_memory.hpp"
#include "memory/memory_manager.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::system {

using namespace kira::utils;

// Global variable to control timer-driven scheduling
bool timerSchedulingEnabled = false;
// Global variable to disable scheduling during tests
bool schedulingDisabled = false;

// Static member definitions
u8 ProcessManager::kernelStacks[ProcessManager::MAX_PROCESSES][ProcessManager::STACK_SIZE];
u8 ProcessManager::userStacks[ProcessManager::MAX_PROCESSES][ProcessManager::STACK_SIZE];
static ProcessManager* gProcessManager = nullptr;

void ProcessManager::initialize() {
    if (!gProcessManager) {
        // Use placement new to initialize at a specific memory location
        static u8 managerMemory[sizeof(ProcessManager)];
        gProcessManager = new(managerMemory) ProcessManager();
    }
}

ProcessManager& ProcessManager::get_instance() {
    if (!gProcessManager) {
        initialize();
    }
    return *gProcessManager;
}

ProcessManager::ProcessManager() {
    // Initialize basic fields
    currentProcess = nullptr;
    sleepQueue = nullptr;
    nextPid = 1;
    kira::kernel::console.add_message("Initializing ProcessManager in constructor", kira::display::VGA_YELLOW_ON_BLUE);

    processCount = 0;
    schedulerTicks = 0;
    lastAgingTime = 0;
    nextStackIndex = 0;
    isInIdleState = false;
    
    // Initialize priority queues
    for (u32 i = 0; i <= MAX_PRIORITY; i++) {
        readyQueues[i].head = nullptr;
        readyQueues[i].tail = nullptr;
        readyQueues[i].count = 0;
    }
    
    // Initialize all processes to TERMINATED state
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = ProcessState::TERMINATED;
        processes[i].pid = 0;
        processes[i].isUserMode = false;
        processes[i].addressSpace = nullptr;
        processes[i].next = nullptr;
        processes[i].prev = nullptr;
        processes[i].sleepUntil = 0;
        processes[i].age = 0;
        processes[i].lastRunTime = 0;
    }
}

u32 ProcessManager::create_user_process(ProcessFunction function, const char* name, u32 priority) {
    if (processCount >= MAX_PROCESSES || !function) {
        return 0;
    }
    
    // Find free process slot
    Process* process = nullptr;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == ProcessState::TERMINATED || processes[i].pid == 0) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        return 0;
    }
    
    // Initialize process
    process->pid = nextPid++;
    strcpy_s(process->name, name ? name : "unnamed", sizeof(process->name) - 1);
    process->state = ProcessState::READY;
    process->priority = priority;
    process->timeSlice = DEFAULT_TIME_SLICE;
    process->timeUsed = 0;
    process->creationTime = 0;  // Would be set from timer in real system
    process->totalCpuTime = 0;
    process->isUserMode = true;
    process->hasStarted = false;
    
    process->userFunction = reinterpret_cast<void*>(function);
    process->next = nullptr;
    
    // Allocate stacks
    if (!allocate_process_stacks(process)) {
        process->pid = 0;  // Mark as free
        return 0;
    }
    
    // Create user address space
    auto& vmManager = VirtualMemoryManager::get_instance();
    process->addressSpace = vmManager.create_user_address_space();
    if (!process->addressSpace) {
        process->pid = 0;  // Mark as free
        return 0;
    }
    
    // Map user program code into the address space
    if (!setup_user_program_mapping(process, function)) {
        // Manually call destructor since we used placement new
        process->addressSpace->~AddressSpace();
        process->addressSpace = nullptr;
        process->pid = 0;  // Mark as free
        return 0;
    }
    
    // Initialize context for user mode
    init_user_process_context(process, function);
    
    // Add to ready queue
    add_to_ready_queue(process);
    processCount++;
    
    return process->pid;
}

u32 ProcessManager::create_process(ProcessFunction function, const char* name, u32 priority) {
    if (processCount >= MAX_PROCESSES || !function) {
        return 0;
    }
    
    // Find free process slot
    Process* process = nullptr;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == ProcessState::TERMINATED || processes[i].pid == 0) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        kira::kernel::console.add_message("no process slot", kira::display::VGA_RED_ON_BLUE);
        return 0;
    }
    
    // Initialize process for kernel mode
    process->pid = nextPid++;
    strcpy_s(process->name, name ? name : "unnamed", sizeof(process->name) - 1);
    process->state = ProcessState::READY;
    process->priority = priority;
    process->timeSlice = DEFAULT_TIME_SLICE;
    process->timeUsed = 0;
    process->creationTime = 0;
    process->totalCpuTime = 0;
    process->isUserMode = false;  // Kernel mode process
    process->hasStarted = false;
    process->addressSpace = nullptr;  // No address space for kernel processes
    process->next = nullptr;
    process->prev = nullptr;
    process->sleepUntil = 0;
    process->age = 0;
    process->lastRunTime = 0;
    
    // Allocate kernel stack
    if (!allocate_process_stacks(process)) {
        process->pid = 0;  // Mark as free
        return 0;
    }
    kira::kernel::console.add_message("init_process_context", kira::display::VGA_GREEN_ON_BLUE);
    // Initialize context for kernel mode
    init_process_context(process, function);
    
    // Add to ready queue
    add_to_ready_queue(process);
    processCount++;
    
    // Debug: Verify the process is still in READY state after adding to queue
    if (process->state != ProcessState::READY) {
        kira::kernel::console.add_message("ERROR: Process state changed after adding to ready queue", kira::display::VGA_RED_ON_BLUE);
    }
    return process->pid;
}

bool ProcessManager::terminate_process(u32 pid) {
    Process* process = get_process(pid);
    if (!process) {
        return false;
    }
    
    // Remove from ready queue if present
    remove_from_ready_queue(process);
    
    // Clean up address space
    if (process->addressSpace) {
        // Manually call destructor since we used placement new
        process->addressSpace->~AddressSpace();
        process->addressSpace = nullptr;
    }
    
    // Mark as terminated
    process->state = ProcessState::TERMINATED;
    processCount--;
    
    // If this was the current process, switch to another
    if (currentProcess == process) {
        currentProcess = nullptr;
        switch_process();
    }
    
    return true;
}

void ProcessManager::schedule() {
    // Don't schedule if scheduling is disabled (for testing)
    if (schedulingDisabled) {
        return;
    }
    
    schedulerTicks++;
    
    // Process sleep queue to wake up expired processes
    process_sleep_queue();
    
    // Perform aging to prevent starvation
    perform_aging();
    
    // Debug: Show scheduler state only when it changes
    if (currentProcess) {
        if (isInIdleState) {
            kira::kernel::console.add_message("DEBUG: Scheduler has current process", kira::display::VGA_GREEN_ON_BLUE);
            isInIdleState = false;
        }
    } else {
        if (!isInIdleState) {
            kira::kernel::console.add_message("DEBUG: Scheduler has no current process", kira::display::VGA_MAGENTA_ON_BLUE);
            isInIdleState = true;
        }
    }
    
    if (currentProcess) {
        currentProcess->timeUsed++;
        currentProcess->totalCpuTime++;
        
        // For user mode processes, check if time slice expired
        if (currentProcess->isUserMode) {
            if (currentProcess->timeUsed >= currentProcess->timeSlice) {
                // Time slice expired - yield to next process
                currentProcess->timeUsed = 0;
                currentProcess->state = ProcessState::READY;
                add_to_ready_queue(currentProcess);
                currentProcess = nullptr;
                switch_process();
            }
            // User mode processes are already running - no need to call them again
            // They will return to kernel via system calls or continue running
        } else {
            // Legacy kernel mode process execution
            ProcessFunction func = (ProcessFunction)currentProcess->context.eip;
            if (func) {
                func(); // Call the process function
            }
            
            // Check if time slice expired
            if (currentProcess->timeUsed >= currentProcess->timeSlice) {
                currentProcess->timeUsed = 0;
                currentProcess->state = ProcessState::READY;
                add_to_ready_queue(currentProcess);
                currentProcess = nullptr;
                switch_process();
            }
        }
    } else {
        // No current process, try to schedule one
        // kira::kernel::console.add_message("DEBUG: NO CURRENT PROCESS, CHECKING READY QUEUE", kira::display::VGA_CYAN_ON_BLUE);
        
        Process* nextProcess = get_next_process();
        if (nextProcess) {
            // kira::kernel::console.add_message("DEBUG: READY QUEUE EXISTS, CALLING SWITCH_PROCESS", kira::display::VGA_GREEN_ON_BLUE);
            switch_process();
        } else {
            // kira::kernel::console.add_message("DEBUG: READY QUEUE IS EMPTY!", kira::display::VGA_RED_ON_BLUE);
        }
    }
}

Process* ProcessManager::get_process(u32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return nullptr;
}

void ProcessManager::display_stats() {
    // Convert to console message instead of direct VGA access
    char statsMsg[80];
    u32 totalHundreds = (schedulerTicks / 100) % 1000;
    
    // Format: "Scheduler: X active, total: YYY00"
    int pos = 0;
    const char* schedText = "Scheduler: ";
    for (int i = 0; schedText[i] != '\0'; i++) {
        statsMsg[pos++] = schedText[i];
    }
    
    statsMsg[pos++] = '0' + processCount;
    
    const char* activeText = " active, total: ";
    for (int i = 0; activeText[i] != '\0'; i++) {
        statsMsg[pos++] = activeText[i];
    }
    
    statsMsg[pos++] = '0' + ((totalHundreds / 100) % 10);
    statsMsg[pos++] = '0' + ((totalHundreds / 10) % 10);
    statsMsg[pos++] = '0' + (totalHundreds % 10);
    statsMsg[pos++] = '0';
    statsMsg[pos++] = '0';
    statsMsg[pos] = '\0';
    
    kira::kernel::console.add_message(statsMsg, kira::display::VGA_YELLOW_ON_BLUE);
}

void ProcessManager::yield() {
    if (currentProcess) {
        currentProcess->timeUsed = 0; // Reset time slice
        currentProcess->state = ProcessState::READY;
        add_to_ready_queue(currentProcess);
        currentProcess = nullptr;
        switch_process();
    }
}

void ProcessManager::add_to_ready_queue(Process* process) {
    if (!process) return;
    
    // Debug: Check process state before adding to queue
    if (process->state != ProcessState::READY) {
        kira::kernel::console.add_message("WARNING: Process not in READY state when adding to queue", kira::display::VGA_YELLOW_ON_BLUE);
    }
    
    // Ensure priority is within bounds
    u32 priority = process->priority;
    if (priority > MAX_PRIORITY) {
        priority = MAX_PRIORITY;
    }
    
    PriorityQueue& queue = readyQueues[priority];
    
    process->next = nullptr;
    process->prev = nullptr;
    
    if (!queue.head) {
        // Empty queue
        queue.head = process;
        queue.tail = process;
    } else {
        // Add to end of queue (FIFO within priority level)
        process->prev = queue.tail;
        queue.tail->next = process;
        queue.tail = process;
    }
    
    queue.count++;
    
    // Debug: Check process state after adding to queue
    if (process->state != ProcessState::READY) {
        kira::kernel::console.add_message("ERROR: Process state changed during add_to_ready_queue", kira::display::VGA_RED_ON_BLUE);
    }
}

Process* ProcessManager::get_next_process() {
    // Find highest priority non-empty queue
    for (u32 priority = 0; priority <= MAX_PRIORITY; priority++) {
        PriorityQueue& queue = readyQueues[priority];
        if (queue.head) {
            return queue.head; // Return head of highest priority queue
        }
    }
    return nullptr; // No ready processes
}

void ProcessManager::remove_from_ready_queue(Process* process) {
    if (!process) {
        return;
    }
    
    // Find which queue the process is in
    u32 priority = process->priority;
    if (priority > MAX_PRIORITY) {
        priority = MAX_PRIORITY;
    }
    
    PriorityQueue& queue = readyQueues[priority];
    
    if (!queue.head) {
        return; // Process not in this queue
    }
    
    if (queue.head == process) {
        // Process is at head of queue
        queue.head = process->next;
        if (queue.head) {
            queue.head->prev = nullptr;
        } else {
            // Queue is now empty
            queue.tail = nullptr;
        }
    } else if (queue.tail == process) {
        // Process is at tail of queue
        queue.tail = process->prev;
        queue.tail->next = nullptr;
    } else {
        // Process is in middle of queue
        process->prev->next = process->next;
        process->next->prev = process->prev;
    }
    
    process->next = nullptr;
    process->prev = nullptr;
    queue.count--;
}

void ProcessManager::switch_process() {
    Process* nextProcess = get_next_process();
    if (nextProcess) {
        // Get next process from priority queue
        currentProcess = nextProcess;
        remove_from_ready_queue(currentProcess);
        currentProcess->state = ProcessState::RUNNING;
        currentProcess->timeUsed = 0;
        currentProcess->lastRunTime = schedulerTicks;
        
        // Immediately update the display to show the new current process
        display_current_process_only();
        
        // If this is a user mode process, switch to user mode
        if (currentProcess->isUserMode) {
            // Switch to the process's address space
            if (currentProcess->addressSpace) {
                auto& vmManager = VirtualMemoryManager::get_instance();
                vmManager.switch_address_space(currentProcess->addressSpace);
            }
            
            // Check if this is the first time running this user process
            if (!currentProcess->hasStarted) {
                currentProcess->hasStarted = true;
                
                // Debug: Use console system to show we're about to switch to user mode
                kira::kernel::console.add_message("ABOUT TO SWITCH TO USER MODE", kira::display::VGA_YELLOW_ON_BLUE);
                
                // Update TSS with this process's kernel stack
                TSSManager::set_kernel_stack(currentProcess->context.kernelEsp);
                
                // Switch to user mode and execute the user function
                // Use the virtual address from the process context
                UserMode::switch_to_user_mode(
                    reinterpret_cast<void*>(currentProcess->context.eip),
                    currentProcess->context.userEsp
                );
                
                // If we get here, something went wrong
                currentProcess->state = ProcessState::TERMINATED;
            } else {
                // Process has already started - this means it yielded and is resuming
                
                // Process has already started - this means it yielded and is resuming
                // For now, just call the function again (this is a simplified approach)
                // In a real OS, we would restore the saved context
                typedef void (*UserFunction)();
                UserFunction userFunc = (UserFunction)currentProcess->userFunction;
                userFunc();
            }
        } else {
            // Kernel mode process - execute the function directly
            if (!currentProcess->hasStarted) {
                currentProcess->hasStarted = true;
                
                // For kernel mode processes, we execute the function directly
                // The function will run in kernel mode and return when done
                ProcessFunction func = (ProcessFunction)currentProcess->context.eip;
                if (func) {
                    func(); // Execute the kernel mode function
                }
                
                // After function completes, terminate the process
                currentProcess->state = ProcessState::TERMINATED;
                processCount--;
                currentProcess = nullptr;
                
                // Try to schedule the next process
                switch_process();
            }
        }
    } else {
        // Ready queue is empty - no more processes to run
        
        // No more processes to run - enter kernel idle state
        kira::kernel::console.add_message("All processes terminated - entering kernel idle state", kira::display::VGA_YELLOW_ON_BLUE);
        
        // Clear current process
        currentProcess = nullptr;
        
        // Enter kernel idle loop - this allows console scrolling to work
        while (true) {
            // Enable interrupts so keyboard/timer still work
            asm volatile("sti");
            // Halt until next interrupt
            asm volatile("hlt");
        }
    }
}

void ProcessManager::display_current_process_only() {
    // Convert to console message instead of direct VGA access
    char processMsg[80];
    int pos = 0;
    
    const char* currentText = "Current: ";
    for (int i = 0; currentText[i] != '\0'; i++) {
        processMsg[pos++] = currentText[i];
    }
    
    if (currentProcess) {
        // Display current process name
        for (int i = 0; currentProcess->name[i] != '\0' && i < 15; i++) {
            processMsg[pos++] = currentProcess->name[i];
        }
        
        // Display PID
        const char* pidText = " (PID:";
        for (int i = 0; pidText[i] != '\0'; i++) {
            processMsg[pos++] = pidText[i];
        }
        
        // Display PID (handle up to 2 digits)
        u32 pid = currentProcess->pid;
        if (pid >= 10) {
            processMsg[pos++] = '0' + (pid / 10); // Tens digit
            processMsg[pos++] = '0' + (pid % 10); // Ones digit
        } else {
            processMsg[pos++] = '0' + pid; // Single digit
        }
        
        // Show user mode indicator
        if (currentProcess->isUserMode) {
            processMsg[pos++] = ',';
            processMsg[pos++] = ' ';
            const char* userText = "U3";
            for (int i = 0; userText[i] != '\0'; i++) {
                processMsg[pos++] = userText[i];
            }
        }
        
        // Closing parenthesis
        processMsg[pos++] = ')';
    } else {
        const char* idleText = "IDLE";
        for (int i = 0; idleText[i] != '\0'; i++) {
            processMsg[pos++] = idleText[i];
        }
    }
    
    processMsg[pos] = '\0';
    kira::kernel::console.add_message(processMsg, kira::display::VGA_CYAN_ON_BLUE);
}

void ProcessManager::init_user_process_context(Process* process, ProcessFunction function) {
    // Initialize all registers to 0
    process->context.eax = 0;
    process->context.ebx = 0;
    process->context.ecx = 0;
    process->context.edx = 0;
    process->context.esi = 0;
    process->context.edi = 0;
    process->context.ebp = 0;
    
    // Set up user mode stack pointer - NOTE: userEsp is already set correctly by setup_user_program_mapping
    // process->context.userEsp = UserMode::setup_user_stack(
    //     process->userStackBase, 
    //     process->userStackSize
    // );  // DON'T overwrite the user space virtual address!
    
    // Set up kernel mode stack pointer  
    process->context.kernelEsp = process->kernelStackBase + process->kernelStackSize - 4;
    
    // Set entry point - NOTE: EIP is already set correctly by setup_user_program_mapping
    // process->context.eip = (u32)function;  // DON'T overwrite the user space address!
    
    // Set default flags (interrupts enabled)
    process->context.eflags = 0x202;
    
    // Set user mode segment registers
    process->context.ds = 0x23; // User data segment
    process->context.es = 0x23;
    process->context.fs = 0x23;
    process->context.gs = 0x23;
}

void ProcessManager::init_process_context(Process* process, ProcessFunction function) {
    // Legacy kernel mode context initialization
    process->context.eax = 0;
    process->context.ebx = 0;
    process->context.ecx = 0;
    process->context.edx = 0;
    process->context.esi = 0;
    process->context.edi = 0;
    process->context.ebp = 0;
    
    // Set up stack pointer (grows downward)
    process->context.kernelEsp = process->kernelStackBase + process->kernelStackSize - 4;
    
    // Set entry point
    process->context.eip = (u32)function;
    
    // Set default flags (interrupts enabled)
    process->context.eflags = 0x202;
    
    // Set kernel segment registers
    process->context.ds = 0x10; // Kernel data segment
    process->context.es = 0x10;
    process->context.fs = 0x10;
    process->context.gs = 0x10;
}

bool ProcessManager::allocate_process_stacks(Process* process) {
    if (nextStackIndex >= MAX_PROCESSES) {
        return false; // Out of stack space
    }
    
    // Allocate kernel stack
    process->kernelStackBase = (u32)&kernelStacks[nextStackIndex][0];
    process->kernelStackSize = STACK_SIZE;
    
    // Allocate user stack
    process->userStackBase = (u32)&userStacks[nextStackIndex][0];
    process->userStackSize = STACK_SIZE;
    
    nextStackIndex++;
    return true;
}

bool ProcessManager::setup_user_program_mapping(Process* process, ProcessFunction function) {
    if (!process->addressSpace) {
        return false;
    }
    
    auto& memoryManager = MemoryManager::get_instance();
    
    // Map user program code - for embedded functions, we need to map the kernel code pages
    // where the function resides into user space
    u32 functionAddr = reinterpret_cast<u32>(function);
    u32 functionPage = functionAddr & PAGE_MASK;
    
    // Map multiple pages for the user program to handle code that spans page boundaries
    u32 userTextAddr = USER_TEXT_START;
    u32 numPagesToMap = 4; // Map 4 pages (16KB) to handle typical user program size
    
    for (u32 i = 0; i < numPagesToMap; i++) {
        u32 userVirtAddr = userTextAddr + (i * PAGE_SIZE);
        u32 kernelPhysAddr = functionPage + (i * PAGE_SIZE);
        
        if (!process->addressSpace->map_page(userVirtAddr, kernelPhysAddr, false, true)) {
            return false;
        }
    }
    
    // Map VGA buffer for user programs that need direct VGA access
    u32 vgaBufferAddr = 0xB8000;
    if (!process->addressSpace->map_page(vgaBufferAddr, vgaBufferAddr, true, true)) {
        return false;
    }
    
    // Map additional kernel pages that might contain string literals and data
    // Map a range of kernel pages around the function to include data/rodata sections
    u32 kernelStart = 0x100000; // 1MB - typical kernel start
    u32 kernelEnd = 0x400000;   // 4MB - should cover most kernel data
    
    for (u32 addr = kernelStart; addr < kernelEnd; addr += PAGE_SIZE) {
        // Map kernel pages to user space (writable for static variables)
        // This allows user programs to access string literals and static data in kernel memory
        process->addressSpace->map_page(addr, addr, true, true);
    }
    
    // Map console object and its buffers (needed for console.add_message calls)
    // The console is a global object in kernel memory, so we need to map it
    u32 consoleAddr = reinterpret_cast<u32>(&kira::kernel::console);
    u32 consolePage = consoleAddr & PAGE_MASK;
    
    // Map the page containing the console object
    if (!process->addressSpace->map_page(consolePage, consolePage, true, true)) {
        return false;
    }
    
    // Map additional pages around console in case it spans multiple pages
    // Console has large internal buffers that might span multiple pages
    for (u32 offset = 0; offset < 0x10000; offset += PAGE_SIZE) { // Map 64KB around console
        u32 addr = consolePage + offset;
        process->addressSpace->map_page(addr, addr, true, true);
    }
    
    // Map user stack into user address space
    u32 userStackPages = (process->userStackSize + PAGE_SIZE - 1) / PAGE_SIZE;
    u32 userStackPhysBase = process->userStackBase & PAGE_MASK;
    u32 userStackVirtTop = USER_STACK_TOP;
    u32 userStackVirtBase = userStackVirtTop - (userStackPages * PAGE_SIZE);
    
    for (u32 i = 0; i < userStackPages; i++) {
        u32 virtAddr = userStackVirtBase + (i * PAGE_SIZE);
        u32 physAddr = userStackPhysBase + (i * PAGE_SIZE);
        
        if (!process->addressSpace->map_page(virtAddr, physAddr, true, true)) {
            return false;
        }
    }
    
    // Update process context to use virtual addresses
    u32 functionOffset = functionAddr - functionPage;
    process->context.eip = userTextAddr + functionOffset;
    process->context.userEsp = userStackVirtTop - 16; // Leave some space at top
    
    return true;
}

u32 ProcessManager::get_current_pid() const {
    return currentProcess ? currentProcess->pid : 0;
}

void ProcessManager::terminate_current_process() {
    if (currentProcess) {
        currentProcess->state = ProcessState::TERMINATED;
        processCount--;
        currentProcess = nullptr;
        switch_process();
    }
}



void ProcessManager::enable_timer_scheduling() {
    timerSchedulingEnabled = true;
}

void ProcessManager::disable_scheduling() {
    schedulingDisabled = true;
}

void ProcessManager::enable_scheduling() {
    schedulingDisabled = false;
}

bool ProcessManager::is_scheduling_disabled() {
    return schedulingDisabled;
}

void ProcessManager::sleep_current_process(u32 ticks) {
    if (!currentProcess) {
        return;
    }
    
    add_to_sleep_queue(currentProcess, ticks);
    currentProcess = nullptr;
    switch_process();
}

void ProcessManager::wake_up_process(Process* process) {
    if (!process) return;
    
    if (process->state == ProcessState::BLOCKED || process->state == ProcessState::WAITING) {
        process->state = ProcessState::READY;
        add_to_ready_queue(process);
    }
}

void ProcessManager::add_to_sleep_queue(Process* process, u32 sleepTicks) {
    if (!process) return;
    
    process->state = ProcessState::SLEEPING;
    process->sleepUntil = schedulerTicks + sleepTicks;
    
    // Add to sleep queue (sorted by wake time)
    if (!sleepQueue || process->sleepUntil < sleepQueue->sleepUntil) {
        // Insert at head
        process->next = sleepQueue;
        process->prev = nullptr;
        if (sleepQueue) {
            sleepQueue->prev = process;
        }
        sleepQueue = process;
    } else {
        // Insert in middle or at end
        Process* current = sleepQueue;
        while (current->next && current->next->sleepUntil <= process->sleepUntil) {
            current = current->next;
        }
        
        process->next = current->next;
        process->prev = current;
        current->next = process;
        if (process->next) {
            process->next->prev = process;
        }
    }
}

void ProcessManager::process_sleep_queue() {
    Process* current = sleepQueue;
    while (current && current->sleepUntil <= schedulerTicks) {
        Process* next = current->next;
        
        // Remove from sleep queue
        if (current->prev) {
            current->prev->next = current->next;
        } else {
            sleepQueue = current->next;
        }
        if (current->next) {
            current->next->prev = current->prev;
        }
        
        // Wake up the process
        current->state = ProcessState::READY;
        current->next = nullptr;
        current->prev = nullptr;
        add_to_ready_queue(current);
        
        current = next;
    }
}

void ProcessManager::perform_aging() {
    if (schedulerTicks - lastAgingTime < AGING_INTERVAL) {
        return; // Not time for aging yet
    }
    
    lastAgingTime = schedulerTicks;
    
    // Age all processes in ready queues
    for (u32 priority = 1; priority <= MAX_PRIORITY; priority++) {
        PriorityQueue& queue = readyQueues[priority];
        Process* current = queue.head;
        
        while (current) {
            Process* next = current->next;
            current->age++;
            
            // Promote process to higher priority if it's been waiting too long
            if (current->age > 50 && priority > 0) {
                remove_from_ready_queue(current);
                current->priority = priority - 1;
                current->age = 0;
                add_to_ready_queue(current);
            }
            
            current = next;
        }
    }
}

void ProcessManager::block_current_process() {
    if (!currentProcess) return;
    
    currentProcess->state = ProcessState::BLOCKED;
    currentProcess = nullptr;
    switch_process();
}

bool ProcessManager::set_process_priority(u32 pid, u32 priority) {
    Process* process = get_process(pid);
    if (!process) return false;
    
    if (priority > MAX_PRIORITY) {
        return false; // Invalid priority
    }
    
    // If process is in ready queue, remove and re-add with new priority
    if (process->state == ProcessState::READY) {
        remove_from_ready_queue(process);
        process->priority = priority;
        add_to_ready_queue(process);
    } else {
        process->priority = priority;
    }
    
    return true;
}

u32 ProcessManager::get_process_priority(u32 pid) const {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != ProcessState::TERMINATED) {
            return processes[i].priority;
        }
    }
    return 0xFFFFFFFF; // Not found
}

} // namespace kira::system 
