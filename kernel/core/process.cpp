#include "core/types.hpp"
#include "core/process.hpp"
#include "core/utils.hpp"
#include "core/usermode.hpp"
#include "display/vga.hpp"
#include "arch/x86/tss.hpp"

namespace kira::system {

using namespace kira::utils;

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
    readyQueue = nullptr;
    currentProcess = nullptr;
    nextPid = 1;
    processCount = 0;
    schedulerTicks = 0;
    nextStackIndex = 0;
    
    // Initialize all processes to TERMINATED state
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = ProcessState::TERMINATED;
        processes[i].pid = 0;
        processes[i].isUserMode = false;
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
    
    // Initialize context for user mode
    init_user_process_context(process, function);
    
    // Add to ready queue
    add_to_ready_queue(process);
    processCount++;
    
    return process->pid;
}

u32 ProcessManager::create_process(ProcessFunction function, const char* name, u32 priority) {
    // Legacy method - create user mode process
    return create_user_process(function, name, priority);
}

bool ProcessManager::terminate_process(u32 pid) {
    Process* process = get_process(pid);
    if (!process) {
        return false;
    }
    
    // Remove from ready queue if present
    remove_from_ready_queue(process);
    
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
    schedulerTicks++;
    
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
        switch_process();
    }
}

Process* ProcessManager::get_process(u32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != ProcessState::TERMINATED) {
            return &processes[i];
        }
    }
    return nullptr;
}

void ProcessManager::display_stats() {
    volatile u16* vgaMem = (volatile u16*)0xB8000;
    
    // Only update line 24 - line 23 is updated immediately by switch_process()
    // Line 24: Scheduler statistics and performance info
    int line24Offset = 24 * 80;
    
    // Clear the line first
    for (int i = 0; i < 80; i++) {
        vgaMem[line24Offset + i] = 0x0720; // Clear with spaces
    }
    
    int pos = 0;
    
    // Show scheduler activity and performance
    const char* schedText = "Scheduler: ";
    for (int i = 0; schedText[i] != '\0'; i++) {
        vgaMem[line24Offset + pos++] = 0x0E00 + schedText[i]; // Yellow
    }
    
    // Show active processes
    vgaMem[line24Offset + pos++] = 0x0F00 + ('0' + processCount); // White
    const char* activeText = " active, ";
    for (int i = 0; activeText[i] != '\0'; i++) {
        vgaMem[line24Offset + pos++] = 0x0E00 + activeText[i]; // Yellow
    }
    
    // Show total scheduler invocations (every 100 ticks for readability)
    const char* totalText = "total: ";
    for (int i = 0; totalText[i] != '\0'; i++) {
        vgaMem[line24Offset + pos++] = 0x0E00 + totalText[i]; // Yellow
    }
    
    u32 totalHundreds = (schedulerTicks / 100) % 1000;
    vgaMem[line24Offset + pos++] = 0x0F00 + ('0' + ((totalHundreds / 100) % 10)); // White
    vgaMem[line24Offset + pos++] = 0x0F00 + ('0' + ((totalHundreds / 10) % 10));  // White
    vgaMem[line24Offset + pos++] = 0x0F00 + ('0' + (totalHundreds % 10));         // White
    vgaMem[line24Offset + pos++] = 0x0E00 + '0'; // Yellow (hundreds indicator)
    vgaMem[line24Offset + pos++] = 0x0E00 + '0'; // Yellow
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
    
    process->next = nullptr;
    
    if (!readyQueue) {
        readyQueue = process;
    } else {
        // Add to end of queue (simple FIFO for now)
        Process* current = readyQueue;
        while (current->next) {
            current = current->next;
        }
        current->next = process;
    }
}

void ProcessManager::remove_from_ready_queue(Process* process) {
    if (!process || !readyQueue) return;
    
    if (readyQueue == process) {
        readyQueue = process->next;
    } else {
        Process* current = readyQueue;
        while (current->next && current->next != process) {
            current = current->next;
        }
        if (current->next) {
            current->next = current->next->next;
        }
    }
    process->next = nullptr;
}

void ProcessManager::switch_process() {
    if (readyQueue) {
        // Get next process from ready queue
        currentProcess = readyQueue;
        readyQueue = readyQueue->next;
        currentProcess->next = nullptr;
        currentProcess->state = ProcessState::RUNNING;
        currentProcess->timeUsed = 0;
        
        // Immediately update the display to show the new current process
        display_current_process_only();
        
        // If this is a user mode process, switch to user mode
        if (currentProcess->isUserMode) {
            // Check if this is the first time running this user process
            if (!currentProcess->hasStarted) {
                currentProcess->hasStarted = true;
                
                // Update TSS with this process's kernel stack
                TSSManager::set_kernel_stack(currentProcess->context.kernelEsp);
                
                // Switch to user mode and execute the user function
                // This should NEVER return - user program returns via system calls
                UserMode::switch_to_user_mode(
                    currentProcess->userFunction,
                    currentProcess->context.userEsp
                );
                
                // If we get here, something went wrong
                kira::display::VGADisplay vga;
                vga.print_string(19, 0, "ERROR: User mode returned!", kira::display::VGA_RED_ON_BLUE);
                currentProcess->state = ProcessState::TERMINATED;
            } else {
                // Process has already started - this means it yielded and is resuming
                // For now, just call the function again (this is a simplified approach)
                // In a real OS, we would restore the saved context
                typedef void (*UserFunction)();
                UserFunction userFunc = (UserFunction)currentProcess->userFunction;
                userFunc();
            }
        }
    }
}

void ProcessManager::display_current_process_only() {
    volatile u16* vgaMem = (volatile u16*)0xB8000;
    
    // Line 23: Current process info
    int line23Offset = 23 * 80;
    
    // Clear the line first to prevent leftover characters
    for (int i = 0; i < 80; i++) {
        vgaMem[line23Offset + i] = 0x0720; // Clear with spaces
    }
    
    const char* currentText = "Current: ";
    int pos = 0;
    
    for (int i = 0; currentText[i] != '\0'; i++) {
        vgaMem[line23Offset + pos++] = 0x0B00 + currentText[i]; // Cyan
    }
    
    if (currentProcess) {
        // Display current process name
        for (int i = 0; currentProcess->name[i] != '\0' && i < 15; i++) {
            vgaMem[line23Offset + pos++] = 0x0A00 + currentProcess->name[i]; // Bright green
        }
        
        // Display PID
        const char* pidText = " (PID:";
        for (int i = 0; pidText[i] != '\0'; i++) {
            vgaMem[line23Offset + pos++] = 0x0B00 + pidText[i]; // Cyan
        }
        
        // Display PID (handle up to 2 digits)
        u32 pid = currentProcess->pid;
        if (pid >= 10) {
            vgaMem[line23Offset + pos++] = 0x0F00 + ('0' + (pid / 10)); // Tens digit
            vgaMem[line23Offset + pos++] = 0x0F00 + ('0' + (pid % 10)); // Ones digit
        } else {
            vgaMem[line23Offset + pos++] = 0x0F00 + ('0' + pid); // Single digit
        }
        
        // Show user mode indicator
        if (currentProcess->isUserMode) {
            vgaMem[line23Offset + pos++] = 0x0B00 + ','; // Cyan
            vgaMem[line23Offset + pos++] = 0x0B00 + ' '; // Cyan
            const char* userText = "U3";
            for (int i = 0; userText[i] != '\0'; i++) {
                vgaMem[line23Offset + pos++] = 0x0E00 + userText[i]; // Yellow
            }
        }
        
        // Closing parenthesis
        vgaMem[line23Offset + pos++] = 0x0B00 + ')'; // Cyan
    } else {
        const char* idleText = "IDLE";
        for (int i = 0; idleText[i] != '\0'; i++) {
            vgaMem[line23Offset + pos++] = 0x0800 + idleText[i]; // Dark gray
        }
    }
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
    
    // Set up user mode stack pointer
    process->context.userEsp = UserMode::setup_user_stack(
        process->userStackBase, 
        process->userStackSize
    );
    
    // Set up kernel mode stack pointer  
    process->context.kernelEsp = process->kernelStackBase + process->kernelStackSize - 4;
    
    // Set entry point
    process->context.eip = (u32)function;
    
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

void ProcessManager::sleep_current_process(u32 ticks) {
    if (currentProcess) {
        currentProcess->state = ProcessState::BLOCKED;
        // For now, just yield - we'd need a proper sleep queue for real implementation
        yield();
    }
}

} // namespace kira::system 