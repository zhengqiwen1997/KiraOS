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

extern "C" void resume_from_syscall_stack(kira::system::u32 newEsp);

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
    inputWaitQueue = nullptr;
    nextPid = 1;
    kira::kernel::console.add_message("Initializing ProcessManager in constructor", kira::display::VGA_YELLOW_ON_BLUE);

    processCount = 0;
    schedulerTicks = 0;
    lastAgingTime = 0;
    lastDisplayedPid = 0xFFFFFFFF;
    nextStackIndex = 0;
    isInIdleState = false;
    contextSwitchDeferred = false;
    
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
        processes[i].savedSyscallEsp = 0;
        processes[i].pendingSyscallReturn = 0;
        processes[i].currentWorkingDirectory[0] = '/';
        processes[i].currentWorkingDirectory[1] = '\0';
        processes[i].spawnArg[0] = '\0';
        processes[i].waitingOnPid = 0;
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

u32 ProcessManager::create_user_process_from_elf(AddressSpace* addressSpace, u32 entryPoint, u32 userStackTop, const char* name, u32 priority) {
    if (processCount >= MAX_PROCESSES || !addressSpace || entryPoint == 0 || userStackTop == 0) {
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
    if (!process) return 0;
    
    // Initialize process metadata
    process->pid = nextPid++;
    strcpy_s(process->name, name ? name : "elf", sizeof(process->name) - 1);
    process->state = ProcessState::READY;
    process->priority = priority;
    process->timeSlice = DEFAULT_TIME_SLICE;
    process->timeUsed = 0;
    process->creationTime = 0;
    process->totalCpuTime = 0;
    process->isUserMode = true;
    process->hasStarted = false;
    process->userFunction = nullptr;
    process->next = nullptr;
    
    // Allocate kernel/user stacks backing memory (kernel stack for syscalls)
    if (!allocate_process_stacks(process)) {
        process->pid = 0; return 0;
    }
    
    // Attach provided user address space and context
    process->addressSpace = addressSpace;
    process->context.eip = entryPoint;
    // Use the stack top returned by ELF loader directly; do not subtract again
    process->context.userEsp = userStackTop;
    process->context.kernelEsp = process->kernelStackBase + process->kernelStackSize - 4;
    process->context.eflags = 0x202;
    process->context.ds = 0x23; process->context.es = 0x23; process->context.fs = 0x23; process->context.gs = 0x23;
    
    // Ready queue
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
    
    // Debug banners disabled to avoid interfering with user prompts
    isInIdleState = (currentProcess == nullptr);
    
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
                if (!contextSwitchDeferred) {
                    switch_process();
                }
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
                if (!contextSwitchDeferred) {
                    switch_process();
                }
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
    u32 totalHundreds = (schedulerTicks / 100) % 1000;
    
    // Format: "Scheduler: X active, total: YYY00"
    kira::utils::k_printf_colored(kira::display::VGA_YELLOW_ON_BLUE, 
        "Scheduler: %u active, total: %u00\n", processCount, totalHundreds);
}

void ProcessManager::yield() {
    if (currentProcess) {
        currentProcess->timeUsed = 0; // Reset time slice
        currentProcess->state = ProcessState::READY;
        add_to_ready_queue(currentProcess);
        currentProcess = nullptr;
        // Immediately switch to another process from syscall context
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
                // Process previously entered kernel via syscall; resume at saved iret frame
                // Update TSS with this process's kernel stack for nested syscalls
                TSSManager::set_kernel_stack(currentProcess->context.kernelEsp);
                
                // Switch to the process's address space
                if (currentProcess->addressSpace) {
                    auto& vmManager = VirtualMemoryManager::get_instance();
                    vmManager.switch_address_space(currentProcess->addressSpace);
                }
                
                // If we have a saved syscall ESP, resume from there; otherwise, start anew
                if (currentProcess->savedSyscallEsp != 0) {
                    u32 esp_to_resume = currentProcess->savedSyscallEsp;
                    currentProcess->savedSyscallEsp = 0; // clear after use
                    // Use pendingSyscallReturn as the value to be placed in EAX on return
                    u32 eax_ret = currentProcess->pendingSyscallReturn;
                    asm volatile(
                        "push %0; push %1; call resume_from_syscall_stack; add $8, %%esp" :: "r"(eax_ret), "r"(esp_to_resume) : "memory");
                } else {
                    // Fallback: first-time style jump (should not normally hit here)
                    UserMode::switch_to_user_mode(
                        reinterpret_cast<void*>(currentProcess->context.eip),
                        currentProcess->context.userEsp
                    );
                }
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
        // No ready processes at the moment. Do not enter an infinite idle loop here,
        // because this function can be invoked from syscall/IRQ contexts. Simply return
        // to the caller and let the upper layer decide how to idle.
        return;
    }
}

void ProcessManager::display_current_process_only() {
    // Suppress scheduler banners to avoid interfering with user prompts
    return;
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
    char debugMsg[64]; // Declare once for the entire function
    
    // Map user program code - for embedded functions, we need to map the kernel code pages
    // where the function resides into user space
    u32 functionAddr = reinterpret_cast<u32>(function);
    u32 functionPage = functionAddr & PAGE_MASK;
    
    // Map multiple pages for the user program to handle code that spans page boundaries
    u32 userTextAddr = USER_TEXT_START;
    u32 numPagesToMap = 20; // Map 20 pages (80KB) to handle larger user programs like the shell
    
    // Map one extra preceding page to tolerate minor negative offsets into the first page
    if (functionPage >= PAGE_SIZE) {
        process->addressSpace->map_page(userTextAddr - PAGE_SIZE, functionPage - PAGE_SIZE, false, true);
    }
    
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
    // Map a MUCH larger range of kernel pages to include ALL kernel code/data
    u32 kernelStart = 0x100000; // 1MB - typical kernel start  
    u32 kernelEnd = 0x800000;   // 8MB - expanded to cover full kernel with FAT32 code
    
    u32 pagesMapped = 0;
    for (u32 addr = kernelStart; addr < kernelEnd; addr += PAGE_SIZE) {
        // Map kernel pages to user space (writable for static variables)
        // This allows user programs to access string literals and static data in kernel memory
        if (process->addressSpace->map_page(addr, addr, true, true)) {
            pagesMapped++;
        }
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
    
    // Map user stack into user address space, honoring base offset
    u32 userStackBase = process->userStackBase;
    u32 userStackOffset = userStackBase & (PAGE_SIZE - 1);
    u32 userStackPhysBase = userStackBase & PAGE_MASK;
    u32 totalStackBytes = process->userStackSize + userStackOffset;
    u32 userStackPages = (totalStackBytes + PAGE_SIZE - 1) / PAGE_SIZE;
    u32 userStackVirtTop = USER_STACK_TOP;
    u32 userStackVirtBase = userStackVirtTop - (userStackPages * PAGE_SIZE);
    
    for (u32 i = 0; i < userStackPages; i++) {
        u32 virtAddr = userStackVirtBase + (i * PAGE_SIZE);
        u32 physAddr = userStackPhysBase + (i * PAGE_SIZE);
        
        if (!process->addressSpace->map_page(virtAddr, physAddr, true, true)) {
            return false;
        }
    }
    
    // Update process context: execute directly at the kernel function address
    // (kernel code pages were mapped user-accessible above)
    u32 functionOffset = functionAddr - functionPage;
    process->context.eip = functionAddr;
    // Point ESP to the top of the mapped stack region minus the portion used by offset
    process->context.userEsp = userStackVirtBase + userStackOffset + process->userStackSize - 16; // leave 16 bytes
    

    // Check if user shell function is in mapped range
    u32 shellAddr = reinterpret_cast<u32>(function);
    if (shellAddr >= kernelStart && shellAddr < kernelEnd) {
        kira::kernel::console.add_message("[PROCESS] Shell function is in mapped range", kira::display::VGA_GREEN_ON_BLUE);
    } else {
        kira::kernel::console.add_message("[PROCESS] WARNING: Shell function outside mapped range!", kira::display::VGA_RED_ON_BLUE);
        kira::utils::number_to_hex(debugMsg, shellAddr);
        kira::kernel::console.add_message("[PROCESS] Shell addr:", kira::display::VGA_RED_ON_BLUE);
        kira::kernel::console.add_message(debugMsg, kira::display::VGA_WHITE_ON_BLUE);
    }
    
    return true;
}

u32 ProcessManager::get_current_pid() const {
    return currentProcess ? currentProcess->pid : 0;
}

void ProcessManager::terminate_current_process() {
    if (currentProcess) {
        u32 terminatedPid = currentProcess->pid;
        currentProcess->state = ProcessState::TERMINATED;
        processCount--;
        // Wake any parent waiting on this pid
        for (u32 i = 0; i < MAX_PROCESSES; i++) {
            if (processes[i].state == ProcessState::BLOCKED && processes[i].waitingOnPid == terminatedPid) {
                processes[i].waitingOnPid = 0;
                processes[i].state = ProcessState::READY;
                add_to_ready_queue(&processes[i]);
            }
        }
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
    // Switch to a ready process now; if none ready, idle until timer wakes someone
    switch_process();
    // If switch_process() returned, there was no ready process right now
    while (!currentProcess) {
        asm volatile("sti");
        asm volatile("hlt");
        switch_process();
    }
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

void ProcessManager::block_current_process_for_input() {
    if (!currentProcess) return;
    // Move current process to input wait queue (FIFO)
    Process* p = currentProcess;
    p->state = ProcessState::BLOCKED;
    p->next = nullptr;
    p->prev = nullptr;
    if (!inputWaitQueue) {
        inputWaitQueue = p;
    } else {
        Process* tail = inputWaitQueue;
        while (tail->next) tail = tail->next;
        tail->next = p;
        p->prev = tail;
    }
    currentProcess = nullptr;
    switch_process();
}

bool ProcessManager::deliver_input_char(char ch) {
    if (!inputWaitQueue) return false;
    // Wake the head waiter and set its pending return value
    Process* p = inputWaitQueue;
    inputWaitQueue = p->next;
    if (inputWaitQueue) inputWaitQueue->prev = nullptr;
    p->next = p->prev = nullptr;
    p->pendingSyscallReturn = static_cast<u32>(static_cast<u8>(ch));
    p->state = ProcessState::READY;
    add_to_ready_queue(p);
    return true;
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
