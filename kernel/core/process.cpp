#include "core/types.hpp"
#include "core/process.hpp"
#include "core/utils.hpp"
#include "core/usermode.hpp"
#include "display/vga.hpp"
#include "arch/x86/tss.hpp"

namespace kira::system {

using namespace kira::system::utils;

// Static member definitions
u8 ProcessManager::kernel_stacks[ProcessManager::MAX_PROCESSES][ProcessManager::STACK_SIZE];
u8 ProcessManager::user_stacks[ProcessManager::MAX_PROCESSES][ProcessManager::STACK_SIZE];
static ProcessManager* g_process_manager = nullptr;

void ProcessManager::initialize() {
    if (!g_process_manager) {
        // Use placement new to initialize at a specific memory location
        static u8 manager_memory[sizeof(ProcessManager)];
        g_process_manager = new(manager_memory) ProcessManager();
    }
}

ProcessManager& ProcessManager::get_instance() {
    if (!g_process_manager) {
        initialize();
    }
    return *g_process_manager;
}

ProcessManager::ProcessManager() {
    // Initialize basic fields
    ready_queue = nullptr;
    current_process = nullptr;
    next_pid = 1;
    process_count = 0;
    scheduler_ticks = 0;
    next_stack_index = 0;
    
    // Initialize all processes to TERMINATED state
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = ProcessState::TERMINATED;
        processes[i].pid = 0;
        processes[i].is_user_mode = false;
    }
}

u32 ProcessManager::create_user_process(ProcessFunction function, const char* name, u32 priority) {
    // Find available process slot
    u32 process_index = MAX_PROCESSES;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == ProcessState::TERMINATED) {
            process_index = i;
            break;
        }
    }
    
    if (process_index == MAX_PROCESSES) {
        return 0; // No available slots
    }
    
    Process* process = &processes[process_index];
    
    // Allocate stacks
    if (!allocate_process_stacks(process)) {
        return 0; // Stack allocation failed
    }
    
    // Initialize process
    process->pid = next_pid++;
    strcpy_s(process->name, name, sizeof(process->name));
    process->state = ProcessState::READY;
    process->priority = priority;
    process->time_slice = DEFAULT_TIME_SLICE;
    process->time_used = 0;
    process->creation_time = scheduler_ticks;
    process->total_cpu_time = 0;
    process->next = nullptr;
    process->user_function = (void*)function;
    process->is_user_mode = true;
    process->has_started = false;  // User process hasn't started yet
    
    // Initialize user mode context
    init_user_process_context(process, function);
    
    // Add to ready queue
    add_to_ready_queue(process);
    process_count++;
    
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
    process_count--;
    
    // If this was the current process, switch to another
    if (current_process == process) {
        current_process = nullptr;
        switch_process();
    }
    
    return true;
}

void ProcessManager::schedule() {
    scheduler_ticks++;
    
    if (current_process) {
        current_process->time_used++;
        current_process->total_cpu_time++;
        
        // For user mode processes, check if time slice expired
        if (current_process->is_user_mode) {
            if (current_process->time_used >= current_process->time_slice) {
                // Time slice expired - yield to next process
                current_process->time_used = 0;
                current_process->state = ProcessState::READY;
                add_to_ready_queue(current_process);
                current_process = nullptr;
                switch_process();
            }
            // User mode processes are already running - no need to call them again
            // They will return to kernel via system calls or continue running
        } else {
            // Legacy kernel mode process execution
            ProcessFunction func = (ProcessFunction)current_process->context.eip;
            if (func) {
                func(); // Call the process function
            }
            
            // Check if time slice expired
            if (current_process->time_used >= current_process->time_slice) {
                current_process->time_used = 0;
                current_process->state = ProcessState::READY;
                add_to_ready_queue(current_process);
                current_process = nullptr;
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
    volatile u16* vga_mem = (volatile u16*)0xB8000;
    
    // Only update line 24 - line 23 is updated immediately by switch_process()
    // Line 24: Scheduler statistics and performance info
    int line24_offset = 24 * 80;
    
    // Clear the line first
    for (int i = 0; i < 80; i++) {
        vga_mem[line24_offset + i] = 0x0720; // Clear with spaces
    }
    
    int pos = 0;
    
    // Show scheduler activity and performance
    const char* sched_text = "Scheduler: ";
    for (int i = 0; sched_text[i] != '\0'; i++) {
        vga_mem[line24_offset + pos++] = 0x0E00 + sched_text[i]; // Yellow
    }
    
    // Show active processes
    vga_mem[line24_offset + pos++] = 0x0F00 + ('0' + process_count); // White
    const char* active_text = " active, ";
    for (int i = 0; active_text[i] != '\0'; i++) {
        vga_mem[line24_offset + pos++] = 0x0E00 + active_text[i]; // Yellow
    }
    
    // Show total scheduler invocations (every 100 ticks for readability)
    const char* total_text = "total: ";
    for (int i = 0; total_text[i] != '\0'; i++) {
        vga_mem[line24_offset + pos++] = 0x0E00 + total_text[i]; // Yellow
    }
    
    u32 total_hundreds = (scheduler_ticks / 100) % 1000;
    vga_mem[line24_offset + pos++] = 0x0F00 + ('0' + ((total_hundreds / 100) % 10)); // White
    vga_mem[line24_offset + pos++] = 0x0F00 + ('0' + ((total_hundreds / 10) % 10));  // White
    vga_mem[line24_offset + pos++] = 0x0F00 + ('0' + (total_hundreds % 10));         // White
    vga_mem[line24_offset + pos++] = 0x0E00 + '0'; // Yellow (hundreds indicator)
    vga_mem[line24_offset + pos++] = 0x0E00 + '0'; // Yellow
}

void ProcessManager::yield() {
    if (current_process) {
        current_process->time_used = 0; // Reset time slice
        current_process->state = ProcessState::READY;
        add_to_ready_queue(current_process);
        current_process = nullptr;
        switch_process();
    }
}

void ProcessManager::add_to_ready_queue(Process* process) {
    if (!process) return;
    
    process->next = nullptr;
    
    if (!ready_queue) {
        ready_queue = process;
    } else {
        // Add to end of queue (simple FIFO for now)
        Process* current = ready_queue;
        while (current->next) {
            current = current->next;
        }
        current->next = process;
    }
}

void ProcessManager::remove_from_ready_queue(Process* process) {
    if (!process || !ready_queue) return;
    
    if (ready_queue == process) {
        ready_queue = process->next;
    } else {
        Process* current = ready_queue;
        while (current->next && current->next != process) {
            current = current->next;
        }
        if (current->next == process) {
            current->next = process->next;
        }
    }
    process->next = nullptr;
}

void ProcessManager::switch_process() {
    if (ready_queue) {
        // Get next process from ready queue
        current_process = ready_queue;
        ready_queue = ready_queue->next;
        current_process->next = nullptr;
        current_process->state = ProcessState::RUNNING;
        current_process->time_used = 0;
        
        // Immediately update the display to show the new current process
        display_current_process_only();
        
        // If this is a user mode process, switch to user mode
        if (current_process->is_user_mode) {
            // Check if this is the first time running this user process
            if (!current_process->has_started) {
                current_process->has_started = true;
                
                // Update TSS with this process's kernel stack
                TSSManager::set_kernel_stack(current_process->context.kernel_esp);
                
                // Switch to user mode and execute the user function
                // This should NEVER return - user program returns via system calls
                UserMode::switch_to_user_mode(
                    current_process->user_function,
                    current_process->context.user_esp
                );
                
                // If we get here, something went wrong
                kira::display::VGADisplay vga;
                vga.print_string(19, 0, "ERROR: User mode returned!", kira::display::VGA_RED_ON_BLUE);
                current_process->state = ProcessState::TERMINATED;
            } else {
                // Process has already started - this means it yielded and is resuming
                // For now, just call the function again (this is a simplified approach)
                // In a real OS, we would restore the saved context
                typedef void (*UserFunction)();
                UserFunction user_func = (UserFunction)current_process->user_function;
                user_func();
            }
        }
    }
}

void ProcessManager::display_current_process_only() {
    volatile u16* vga_mem = (volatile u16*)0xB8000;
    
    // Line 23: Current process info
    int line23_offset = 23 * 80;
    
    // Clear the line first to prevent leftover characters
    for (int i = 0; i < 80; i++) {
        vga_mem[line23_offset + i] = 0x0720; // Clear with spaces
    }
    
    const char* current_text = "Current: ";
    int pos = 0;
    
    for (int i = 0; current_text[i] != '\0'; i++) {
        vga_mem[line23_offset + pos++] = 0x0B00 + current_text[i]; // Cyan
    }
    
    if (current_process) {
        // Display current process name
        for (int i = 0; current_process->name[i] != '\0' && i < 15; i++) {
            vga_mem[line23_offset + pos++] = 0x0A00 + current_process->name[i]; // Bright green
        }
        
        // Display PID
        const char* pid_text = " (PID:";
        for (int i = 0; pid_text[i] != '\0'; i++) {
            vga_mem[line23_offset + pos++] = 0x0B00 + pid_text[i]; // Cyan
        }
        
        // Display PID (handle up to 2 digits)
        u32 pid = current_process->pid;
        if (pid >= 10) {
            vga_mem[line23_offset + pos++] = 0x0F00 + ('0' + (pid / 10)); // Tens digit
            vga_mem[line23_offset + pos++] = 0x0F00 + ('0' + (pid % 10)); // Ones digit
        } else {
            vga_mem[line23_offset + pos++] = 0x0F00 + ('0' + pid); // Single digit
        }
        
        // Show user mode indicator
        if (current_process->is_user_mode) {
            vga_mem[line23_offset + pos++] = 0x0B00 + ','; // Cyan
            vga_mem[line23_offset + pos++] = 0x0B00 + ' '; // Cyan
            const char* user_text = "U3";
            for (int i = 0; user_text[i] != '\0'; i++) {
                vga_mem[line23_offset + pos++] = 0x0E00 + user_text[i]; // Yellow
            }
        }
        
        // Closing parenthesis
        vga_mem[line23_offset + pos++] = 0x0B00 + ')'; // Cyan
    } else {
        const char* idle_text = "IDLE";
        for (int i = 0; idle_text[i] != '\0'; i++) {
            vga_mem[line23_offset + pos++] = 0x0800 + idle_text[i]; // Dark gray
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
    process->context.user_esp = UserMode::setup_user_stack(
        process->user_stack_base, 
        process->user_stack_size
    );
    
    // Set up kernel mode stack pointer  
    process->context.kernel_esp = process->kernel_stack_base + process->kernel_stack_size - 4;
    
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
    process->context.kernel_esp = process->kernel_stack_base + process->kernel_stack_size - 4;
    
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
    if (next_stack_index >= MAX_PROCESSES) {
        return false; // Out of stack space
    }
    
    // Allocate kernel stack
    process->kernel_stack_base = (u32)&kernel_stacks[next_stack_index][0];
    process->kernel_stack_size = STACK_SIZE;
    
    // Allocate user stack
    process->user_stack_base = (u32)&user_stacks[next_stack_index][0];
    process->user_stack_size = STACK_SIZE;
    
    next_stack_index++;
    return true;
}

u32 ProcessManager::get_current_pid() const {
    return current_process ? current_process->pid : 0;
}

void ProcessManager::terminate_current_process() {
    if (current_process) {
        current_process->state = ProcessState::TERMINATED;
        process_count--;
        current_process = nullptr;
        switch_process();
    }
}

void ProcessManager::sleep_current_process(u32 ticks) {
    if (current_process) {
        current_process->state = ProcessState::BLOCKED;
        // For now, just yield - we'd need a proper sleep queue for real implementation
        yield();
    }
}

} // namespace kira::system 