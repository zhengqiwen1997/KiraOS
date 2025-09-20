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
// Local helpers to keep fork logic concise and avoid scattered zeroing
static inline void reset_pcb(Process* pcb) {
    // Zero the PCB slot before reuse; mark as TERMINATED until fully initialized
    memset(pcb, 0, sizeof(*pcb));
    pcb->state = ProcessState::TERMINATED;
    // Initialize fd table to -1 (unused)
    for (u32 i = 0; i < MAX_FDS; i++) {
        pcb->fdTable[i] = -1;
    }
}

// File-scope constants
namespace {
    constexpr u32 DEFAULT_EFLAGS = (1u << 9) | (1u << 1); // IF=1, reserved bit 1=1
    // Clone windows (tunable)
    constexpr u32 CLONE_TEXT_WINDOW_BYTES = 8 * 1024 * 1024;   // 8MB text/data
    constexpr u32 CLONE_STACK_WINDOW_BYTES = 256 * 1024;       // 256KB stack top
    constexpr u32 COPY_CHUNK = 256;                            // chunked copy size
}

static inline void copy_fixed_string(char* dst, const char* src, u32 capacity) {
    if (!dst || !src || capacity == 0) return;
    u32 i = 0;
    for (; i + 1 < capacity && src[i] != '\0'; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static inline void clone_basic_metadata(Process* child, const Process* parent) {
    // Name and scheduling basics
    strcpy_s(child->name, parent->name, sizeof(child->name) - 1);
    child->priority = parent->priority;
    child->timeSlice = parent->timeSlice;
    // User-mode flags
    child->isUserMode = true;
    // Copy shell-visible context
    copy_fixed_string(child->currentWorkingDirectory, parent->currentWorkingDirectory, sizeof(child->currentWorkingDirectory));
    copy_fixed_string(child->spawnArg, parent->spawnArg, sizeof(child->spawnArg));
}


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
        processes[i].parentPid = 0;
        processes[i].exitStatus = 0;
        processes[i].waitStatusUserPtr = 0;
        processes[i].pendingChildPid = 0;
        processes[i].pendingChildStatus = 0;
        processes[i].hasBeenWaited = false;
        for (u32 fd = 0; fd < MAX_FDS; fd++) { processes[i].fdTable[fd] = -1; processes[i].fdFlags[fd] = 0; }
    }
}
bool ProcessManager::is_pid_in_use(u32 pid) const {
    if (pid == 0) return true; // reserved
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        const Process& p = processes[i];
        if (p.pid == pid && p.state != ProcessState::TERMINATED) {
            return true;
        }
    }
    return false;
}

u32 ProcessManager::allocate_pid() {
    // Try up to MAX_PID times to find a free one
    for (u32 attempts = 0; attempts < MAX_PID; attempts++) {
        if (nextPid == 0 || nextPid > MAX_PID) nextPid = 1; // wrap but skip 0
        u32 candidate = nextPid++;
        if (!is_pid_in_use(candidate)) {
            return candidate;
        }
    }
    // PID space exhausted
    return 0;
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
    process->pid = allocate_pid();
    if (process->pid == 0) {
        // No available PID
        return 0;
    }
    strcpy_s(process->name, name ? name : "unnamed", sizeof(process->name) - 1);
    process->state = ProcessState::READY;
    process->priority = priority;
    process->timeSlice = DEFAULT_TIME_SLICE;
    process->timeUsed = 0;
    process->creationTime = 0;  // Would be set from timer in real system
    process->totalCpuTime = 0;
    process->isUserMode = true;
    process->hasStarted = false;
    // Setup standard fds: 0,1,2
    process->fdTable[0] = -100; // STDIN sentinel
    process->fdTable[1] = -101; // STDOUT sentinel
    process->fdTable[2] = -102; // STDERR sentinel
    process->fdFlags[0] = 0; process->fdFlags[1] = 0; process->fdFlags[2] = 0;
    
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
    process->pid = allocate_pid();
    if (process->pid == 0) {
        return 0;
    }
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
    // Setup standard fds
    process->fdTable[0] = -100;
    process->fdTable[1] = -101;
    process->fdTable[2] = -102;
    process->fdFlags[0] = 0; process->fdFlags[1] = 0; process->fdFlags[2] = 0;
    
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
    process->pid = allocate_pid();
    if (process->pid == 0) {
        kira::kernel::console.add_message("no free pid", kira::display::VGA_RED_ON_BLUE);
        return 0;
    }
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
    
    // Set exit status for killed process and notify parent if waiting
    u32 terminatedPid = process->pid;
    process->exitStatus = -1; // default status for kill

    // Reparent this process's children to init (pid=1)
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0 && processes[i].parentPid == terminatedPid) {
            processes[i].parentPid = 1; // init
        }
    }

    // Become zombie; actual resource free on reap
    process->state = ProcessState::ZOMBIE;
    processCount--;

    // Wake any waiting parent: explicit pid or ANY-child (waitingOnPid==0 and parentPid matches)
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        Process* p = &processes[i];
        if (p->pid == 0) continue;
        bool isExplicitWait = (p->waitingOnPid == terminatedPid);
        bool isAnyChildWait = (p->waitingOnPid == 0 && p->pid == process->parentPid);
        if (!(isExplicitWait || isAnyChildWait)) continue;

        // Default WAIT: return exit status in EAX
        p->pendingSyscallReturn = static_cast<u32>(process->exitStatus);

        // WAITID: write status to user ptr and return child pid
        if (p->waitStatusUserPtr != 0) {
            auto& vm = VirtualMemoryManager::get_instance();
            AddressSpace* original = vm.get_current_address_space();
            if (p->addressSpace) { vm.switch_address_space(p->addressSpace); }
            i32* dst = reinterpret_cast<i32*>(p->waitStatusUserPtr);
            *dst = process->exitStatus;
            if (original) { vm.switch_address_space(original); }
            p->pendingSyscallReturn = static_cast<u32>(terminatedPid);
            p->waitStatusUserPtr = 0;
        }

        if (p->state == ProcessState::BLOCKED) {
            p->waitingOnPid = 0;
            p->state = ProcessState::READY;
            add_to_ready_queue(p);
        }
    }
    
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

u32 ProcessManager::fork_current_process() {
    Process* parent = currentProcess;
    if (!parent) return 0;

    // --- Constants to avoid magic numbers ---
    constexpr u16 USER_CS = 0x1B;   // Ring 3 code selector
    constexpr u16 USER_DS = 0x23;   // Ring 3 data selector
    constexpr u32 SEG_PUSHA_SIZE = 48; // [GS,FS,ES,DS] + pusha (8 regs)
    constexpr u32 IRET_SIZE = 20;       // EIP,CS,EFLAGS,USERESP,SS
    constexpr u32 FRAME_SIZE = SEG_PUSHA_SIZE + IRET_SIZE;
    constexpr u32 PUSHA_EAX_OFFSET = 44;  // Offset of EAX within pusha area
    constexpr u32 IRET_EIP_OFF = 0;
    constexpr u32 IRET_CS_OFF = 4;
    constexpr u32 IRET_EFLAGS_OFF = 8;
    constexpr u32 IRET_USERESP_OFF = 12;
    constexpr u32 IRET_SS_OFF = 16;
    constexpr u32 COPY_CHUNK = 256;      // Small chunk to limit kernel stack usage
    constexpr u32 CLONE_TEXT_WINDOW_BYTES = 8 * 1024 * 1024;   // 8MB text/data window
    constexpr u32 CLONE_STACK_WINDOW_BYTES = 256 * 1024;       // 256KB of top-of-stack

    auto align_down = [](u32 value, u32 align) -> u32 { return value & ~(align - 1); };
    auto align_up   = [](u32 value, u32 align) -> u32 { return (value + align - 1) & ~(align - 1); };

    // Find a free PCB slot
    Process* child = nullptr;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == ProcessState::TERMINATED || processes[i].pid == 0) {
            child = &processes[i];
            break;
        }
    }
    if (!child) return 0;

    // Initialize child metadata (reset slot, then fill)
    reset_pcb(child);
    child->pid = nextPid++;
    clone_basic_metadata(child, parent);
    child->state = ProcessState::READY;
    child->creationTime = parent->creationTime;
    child->userFunction = parent->userFunction;
    child->hasStarted = true; // will resume from saved frame
    child->parentPid = parent->pid;
    // Inherit fd table by shallow copy
    for (u32 fd = 0; fd < MAX_FDS; fd++) child->fdTable[fd] = parent->fdTable[fd];
    for (u32 fd = 0; fd < MAX_FDS; fd++) child->fdFlags[fd] = parent->fdFlags[fd];

    // Allocate kernel/user stacks backing memory
    if (!allocate_process_stacks(child)) {
        child->pid = 0;
        return 0;
    }
    child->context = {}; // zero register context
    child->context.kernelEsp = child->kernelStackBase + child->kernelStackSize - 4;
    child->context.eflags = DEFAULT_EFLAGS;
    child->context.ds = USER_DS; child->context.es = USER_DS; child->context.fs = USER_DS; child->context.gs = USER_DS;

    // Create a new user address space for the child and clone parent's mappings in relevant windows
    child->addressSpace = nullptr;
    if (parent->addressSpace) {
        auto& vm = VirtualMemoryManager::get_instance();
        AddressSpace* childAS = vm.create_user_address_space();
        if (!childAS) { child->pid = 0; return 0; }

        // Do not map kernel identity/VGA/console into child user space (hardening)
        u32 consoleAddr = reinterpret_cast<u32>(&kira::kernel::console);
        u32 consolePage = consoleAddr & PAGE_MASK;

        // PTE-based CoW over full user space, excluding shared regions
        const u32 EXCL_ID_START = KERNEL_CODE_START; // 1MB
        const u32 EXCL_ID_END   = 0x00800000;        // 8MB
        const u32 EXCL_VGA_PAGE = VGA_BUFFER_ADDR & PAGE_MASK;
        const u32 EXCL_CONS_START = consolePage;
        const u32 EXCL_CONS_END   = consolePage + 0x10000; // 64KB console span

        // Limit CoW to: user text window we established and the heap range; plus last 16KB of stack
        const u32 TEXT_START = USER_TEXT_START - PAGE_SIZE; // we mapped one preceding page
        const u32 TEXT_END   = USER_TEXT_START + (64 * PAGE_SIZE);
        const u32 HEAP_START = parent->heapStart ? parent->heapStart : USER_HEAP_START;
        const u32 HEAP_END   = parent->heapEnd ? parent->heapEnd : USER_HEAP_START;
        const u32 STACK_TOP  = USER_STACK_TOP;
        const u32 STACK_SIZE_LIMIT = 64 * 1024; // clone top 64KB of stack to avoid unmapped faults
        const u32 STACK_BASE = STACK_TOP - STACK_SIZE_LIMIT;

        auto clone_range_cow = [&](u32 start, u32 end) {
            for (u32 va = start & PAGE_MASK; va < end; va += PAGE_SIZE) {
                if (!parent->addressSpace->is_mapped(va)) continue;
                if ((va >= EXCL_ID_START && va < EXCL_ID_END) ||
                    va == EXCL_VGA_PAGE ||
                    (va >= EXCL_CONS_START && va < EXCL_CONS_END)) {
                    continue;
                }
                u32 srcPhys = parent->addressSpace->get_physical_address(va) & PAGE_MASK;
                if (srcPhys == 0) continue;
                childAS->map_page(va, srcPhys, false, true);
                parent->addressSpace->set_page_writable(va, false);
                MemoryManager::get_instance().increment_page_ref(srcPhys);
            }
        };

        clone_range_cow(TEXT_START, TEXT_END);
        if (HEAP_END > HEAP_START) clone_range_cow(HEAP_START, HEAP_END);
        clone_range_cow(STACK_BASE, STACK_TOP);
 
        child->addressSpace = childAS;
    }

    // Inherit heap bounds
    child->heapStart = parent->heapStart;
    child->heapEnd = parent->heapEnd;

    // Synthesize a saved syscall frame for the child so it resumes after the INT 0x80 with EAX=0
    if (parent->savedSyscallEsp != 0) {

        // Place frame at top of child's kernel stack
        u32 childTop = child->kernelStackBase + child->kernelStackSize;
        u32 frameEsp = childTop - FRAME_SIZE;

        // Copy parent's saved frame bytes (segments+pusha+iret) verbatim
        const u8* parentFrame = reinterpret_cast<const u8*>(parent->savedSyscallEsp);
        u8* childFrame = reinterpret_cast<u8*>(frameEsp);
        for (u32 i = 0; i < FRAME_SIZE; i++) {
            childFrame[i] = parentFrame[i];
        }
        // Overwrite EAX in pusha to 0 for the child return value
        *reinterpret_cast<u32*>(frameEsp + PUSHA_EAX_OFFSET) = 0;
        // Ensure user selectors in iret frame (defensive)
        *reinterpret_cast<u32*>(frameEsp + SEG_PUSHA_SIZE + IRET_CS_OFF) = USER_CS; // CS
        *reinterpret_cast<u32*>(frameEsp + SEG_PUSHA_SIZE + IRET_SS_OFF) = USER_DS; // SS

        child->savedSyscallEsp = frameEsp;
        child->context.kernelEsp = childTop - 4; // ESP0 for TSS
        child->pendingSyscallReturn = 0;
        // Also set context fields for fallback first-time entry path to parent's values
        auto& vmm_rd = VirtualMemoryManager::get_instance();
        AddressSpace* originalAS = vmm_rd.get_current_address_space();
        vmm_rd.switch_address_space(parent->addressSpace);
        u32 parentEip = *reinterpret_cast<u32*>(parent->savedSyscallEsp + SEG_PUSHA_SIZE + IRET_EIP_OFF);
        u32 parentUserEsp = *reinterpret_cast<u32*>(parent->savedSyscallEsp + SEG_PUSHA_SIZE + IRET_USERESP_OFF);
        if (originalAS) vmm_rd.switch_address_space(originalAS);
        child->context.eip = parentEip;
        child->context.userEsp = parentUserEsp;
        child->context.eax = 0;
    } else {
        // Fallback: start as first-time user entry (less precise return semantics)
        child->hasStarted = false;
        child->context.eip = parent->context.eip;
        child->context.userEsp = parent->context.userEsp;
        child->context.eax = 0;
    }

    // Add to ready queue
    add_to_ready_queue(child);
    processCount++;
    return child->pid;
}

Process* ProcessManager::find_terminated_child(u32 parentPid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0 &&
            processes[i].parentPid == parentPid &&
            processes[i].state == ProcessState::ZOMBIE &&
            !processes[i].hasBeenWaited) {
            return &processes[i];
        }
    }
    return nullptr;
}

bool ProcessManager::has_child(u32 parentPid) const {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0 && processes[i].parentPid == parentPid) return true;
    }
    return false;
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
                
                // Sanity: ensure EIP and ESP are mapped in this address space before switching
                if (currentProcess->addressSpace) {
                    AddressSpace* as = currentProcess->addressSpace;
                    u32 eip = currentProcess->context.eip;
                    u32 esp = currentProcess->context.userEsp;
                    if (!as->is_mapped(eip) || !as->is_mapped(esp)) {
                        kira::kernel::console.add_message("ERROR: EIP/ESP not mapped for user process", kira::display::VGA_RED_ON_BLUE);
                        char buf[32];
                        kira::utils::number_to_hex(buf, eip);
                        kira::kernel::console.add_message("EIP:", kira::display::VGA_RED_ON_BLUE);
                        kira::kernel::console.add_message(buf, kira::display::VGA_WHITE_ON_BLUE);
                        kira::utils::number_to_hex(buf, esp);
                        kira::kernel::console.add_message("ESP:", kira::display::VGA_RED_ON_BLUE);
                        kira::kernel::console.add_message(buf, kira::display::VGA_WHITE_ON_BLUE);
                        currentProcess->state = ProcessState::TERMINATED;
                        processCount--;
                        currentProcess = nullptr;
                        switch_process();
                        return;
                    }
                }

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
    // Reuse per-process slot stacks based on PCB index instead of consuming a global index
    u32 index = static_cast<u32>(process - processes);
    if (index >= MAX_PROCESSES) {
        return false;
    }
    // Assign kernel stack
    process->kernelStackBase = (u32)&kernelStacks[index][0];
    process->kernelStackSize = STACK_SIZE;
    // Assign user stack
    process->userStackBase = (u32)&userStacks[index][0];
    process->userStackSize = STACK_SIZE;
    return true;
}

bool ProcessManager::setup_user_program_mapping(Process* process, ProcessFunction function) {
    if (!process->addressSpace) {
        return false;
    }
    
    auto& memoryManager = MemoryManager::get_instance();
    char debugMsg[64]; // Declare once for the entire function
    
    // Map user program code - for embedded functions, we will map only the
    // minimal required kernel code pages into user space as read-only text.
    u32 functionAddr = reinterpret_cast<u32>(function);
    u32 functionPage = functionAddr & PAGE_MASK;
    
    // Map multiple pages for the user program to handle code that spans page boundaries
    u32 userTextAddr = USER_TEXT_START;
    u32 numPagesToMap = 64; // map 256KB of contiguous text to cover shell + deps
    
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
    
    // Do not map broad kernel identity pages or console into user space anymore.
    // User programs should communicate via syscalls. VGA direct mapping also disabled.

    // Minimal read-only identity window for kernel .rodata referenced by embedded user code
    // This allows absolute addresses like 0x0011xxxx to be readable from user mode.
    {
        const u32 roStart = KERNEL_CODE_START;            // 1MB
        const u32 roEnd   = KERNEL_CODE_START + 0x200000; // +2MB window (1MB..3MB)
        for (u32 addr = roStart; addr < roEnd; addr += PAGE_SIZE) {
            (void)process->addressSpace->map_page(addr, addr, false, true);
        }
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
    
    // Update process context: execute at the mapped user text address
    u32 functionOffset = functionAddr - functionPage;
    process->context.eip = userTextAddr + functionOffset;
    // Point ESP to the top of the mapped stack region minus the portion used by offset
    process->context.userEsp = userStackVirtBase + userStackOffset + process->userStackSize - 16; // leave 16 bytes
    
    return true;
}

u32 ProcessManager::get_current_pid() const {
    return currentProcess ? currentProcess->pid : 0;
}

void ProcessManager::terminate_current_process() {
    if (currentProcess) {
        terminate_current_process_with_status(0);
    }
}

void ProcessManager::terminate_current_process_with_status(i32 status) {
    if (!currentProcess) return;
    u32 terminatedPid = currentProcess->pid;
    currentProcess->exitStatus = status;
    currentProcess->state = ProcessState::ZOMBIE;
    processCount--;

    // Reparent any children to init (pid=1)
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0 && processes[i].parentPid == terminatedPid) {
            processes[i].parentPid = 1; // init
        }
    }

    // Wake any waiting parent: explicit pid match OR any-child (waitingOnPid==0 and parentPid matches)
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        Process* p = &processes[i];
        if (p->pid == 0) continue;
        bool isExplicitWait = (p->waitingOnPid == terminatedPid);
        bool isAnyChildWait = (p->waitingOnPid == WAIT_ANY_CHILD && p->pid == currentProcess->parentPid);
        if (!(isExplicitWait || isAnyChildWait)) continue;

        // Default: WAIT returns exit status in EAX (if already blocked). If not blocked yet, stash
        if (p->state == ProcessState::BLOCKED) {
            p->pendingSyscallReturn = static_cast<u32>(currentProcess->exitStatus);
        } else {
            p->pendingChildPid = terminatedPid;
            p->pendingChildStatus = currentProcess->exitStatus;
        }

        // If WAITID provided a user status pointer, write status there and return child pid in EAX
        if (p->waitStatusUserPtr != 0) {
            auto& vm = VirtualMemoryManager::get_instance();
            AddressSpace* original = vm.get_current_address_space();
            if (p->addressSpace) { vm.switch_address_space(p->addressSpace); }
            i32* dst = reinterpret_cast<i32*>(p->waitStatusUserPtr);
            *dst = currentProcess->exitStatus;
            if (original) { vm.switch_address_space(original); }
            p->pendingSyscallReturn = static_cast<u32>(terminatedPid);
            p->waitStatusUserPtr = 0;
            currentProcess->hasBeenWaited = true;
        }
        else {
            // Simple WAIT also consumes this child
            currentProcess->hasBeenWaited = true;
        }

        if (p->state == ProcessState::BLOCKED) {
            p->waitingOnPid = 0;
            p->state = ProcessState::READY;
            add_to_ready_queue(p);
        }
    }
    currentProcess = nullptr;
    switch_process();
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
    
    if (process->state == ProcessState::BLOCKED) {
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

void ProcessManager::reap_child(Process* child) {
    if (!child) return;
    // Only reap if it's zombie and already consumed by a waiter
    if (child->state != ProcessState::ZOMBIE) return;
    if (!child->hasBeenWaited) return;
    // Free its address space if somehow still present
    if (child->addressSpace) {
        auto& vm = VirtualMemoryManager::get_instance();
        vm.destroy_user_address_space(child->addressSpace);
        child->addressSpace = nullptr;
    }
    // Mark PCB slot free
    child->pid = 0;
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
