#include "core/process.hpp"
#include "display/vga.hpp"

namespace kira::system {

// Global instance storage
static ProcessManager g_process_manager_instance;
static ProcessManager* g_process_manager = nullptr;

// Static stack allocation
u8 ProcessManager::process_stacks[MAX_PROCESSES][STACK_SIZE];

ProcessManager::ProcessManager() 
    : ready_queue(nullptr)
    , current_process(nullptr)
    , next_pid(1)
    , process_count(0)
    , scheduler_ticks(0)
    , next_stack_index(0)
{
    // Initialize all processes as terminated (only set non-default values)
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = ProcessState::TERMINATED;
        // All other fields are automatically zero-initialized
    }
}

void ProcessManager::initialize() {
    // Manually call constructor to ensure initialization
    g_process_manager_instance.~ProcessManager(); // Destroy any existing state
    // Reconstruct in place
    g_process_manager_instance.ready_queue = nullptr;
    g_process_manager_instance.current_process = nullptr;
    g_process_manager_instance.next_pid = 1;
    g_process_manager_instance.process_count = 0;
    g_process_manager_instance.scheduler_ticks = 0;
    g_process_manager_instance.next_stack_index = 0;
    
    // Initialize all processes as terminated (only set non-default values)
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        g_process_manager_instance.processes[i].state = ProcessState::TERMINATED;
        // All other fields are automatically zero-initialized
    }
    
    g_process_manager = &g_process_manager_instance;
}

ProcessManager& ProcessManager::get_instance() {
    return *g_process_manager;
}

u32 ProcessManager::create_process(ProcessFunction function, const char* name, u32 priority) {
    if (!function || process_count >= MAX_PROCESSES) {
        return 0; // Failed
    }
    
    // Find free process slot
    Process* process = nullptr;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == ProcessState::TERMINATED) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        return 0; // No free slots
    }
    
    // Initialize process
    process->pid = next_pid++;
    safe_strcpy(process->name, name, sizeof(process->name) - 1);
    process->state = ProcessState::READY;
    process->priority = priority;
    process->time_slice = DEFAULT_TIME_SLICE;
    process->time_used = 0;
    process->creation_time = 0;
    process->total_cpu_time = 0;
    process->next = nullptr;
    
    // Allocate stack
    process->stack_base = allocate_stack();
    process->stack_size = STACK_SIZE;
    
    if (process->stack_base == 0) {
        return 0; // Stack allocation failed
    }
    
    // Initialize context
    init_process_context(process, function);
    
    // Add to ready queue
    add_to_ready_queue(process);
    process_count++;
    
    return process->pid;
}

bool ProcessManager::terminate_process(u32 pid) {
    Process* process = get_process(pid);
    if (!process || process->state == ProcessState::TERMINATED) {
        return false;
    }
    
    // Remove from ready queue if present
    if (process->state == ProcessState::READY) {
        remove_from_ready_queue(process);
    }
    
    // Mark as terminated
    process->state = ProcessState::TERMINATED;
    process_count--;
    
    // If this was the current process, schedule next
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
        
        // Execute the current process function
        ProcessFunction func = (ProcessFunction)current_process->context.eip;
        if (func) {
            func(); // Call the process function
        }
        
        // Check if time slice expired
        if (current_process->time_used >= current_process->time_slice) {
            // Reset time slice and switch process
            current_process->time_used = 0;
            current_process->state = ProcessState::READY;
            add_to_ready_queue(current_process);
            current_process = nullptr;
            switch_process();
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
    
    // Only update line 22 - line 21 is updated immediately by switch_process()
    // Line 22: Scheduler statistics and performance info
    int line22_offset = 22 * 80;
    
    // Clear the line first
    for (int i = 0; i < 80; i++) {
        vga_mem[line22_offset + i] = 0x0720; // Clear with spaces
    }
    
    int pos = 0;
    
    // Show scheduler activity and performance
    const char* sched_text = "Scheduler: ";
    for (int i = 0; sched_text[i] != '\0'; i++) {
        vga_mem[line22_offset + pos++] = 0x0E00 + sched_text[i]; // Yellow
    }
    
    // Show active processes
    vga_mem[line22_offset + pos++] = 0x0F00 + ('0' + process_count); // White
    const char* active_text = " active, ";
    for (int i = 0; active_text[i] != '\0'; i++) {
        vga_mem[line22_offset + pos++] = 0x0E00 + active_text[i]; // Yellow
    }
    
    // Show total scheduler invocations (every 100 ticks for readability)
    const char* total_text = "total: ";
    for (int i = 0; total_text[i] != '\0'; i++) {
        vga_mem[line22_offset + pos++] = 0x0E00 + total_text[i]; // Yellow
    }
    
    u32 total_hundreds = (scheduler_ticks / 100) % 1000;
    vga_mem[line22_offset + pos++] = 0x0F00 + ('0' + ((total_hundreds / 100) % 10)); // White
    vga_mem[line22_offset + pos++] = 0x0F00 + ('0' + ((total_hundreds / 10) % 10));  // White
    vga_mem[line22_offset + pos++] = 0x0F00 + ('0' + (total_hundreds % 10));         // White
    vga_mem[line22_offset + pos++] = 0x0E00 + '0'; // Yellow (hundreds indicator)
    vga_mem[line22_offset + pos++] = 0x0E00 + '0'; // Yellow
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
    }
}

void ProcessManager::display_current_process_only() {
    volatile u16* vga_mem = (volatile u16*)0xB8000;
    
    // Line 21: Current process info
    int line21_offset = 21 * 80;
    
    // Clear the line first to prevent leftover characters
    for (int i = 0; i < 80; i++) {
        vga_mem[line21_offset + i] = 0x0720; // Clear with spaces
    }
    
    const char* current_text = "Current: ";
    int pos = 0;
    
    for (int i = 0; current_text[i] != '\0'; i++) {
        vga_mem[line21_offset + pos++] = 0x0B00 + current_text[i]; // Cyan
    }
    
    if (current_process) {
        // Display current process name
        for (int i = 0; current_process->name[i] != '\0' && i < 15; i++) {
            vga_mem[line21_offset + pos++] = 0x0A00 + current_process->name[i]; // Bright green
        }
        
        // Display PID
        const char* pid_text = " (PID:";
        for (int i = 0; pid_text[i] != '\0'; i++) {
            vga_mem[line21_offset + pos++] = 0x0B00 + pid_text[i]; // Cyan
        }
        
        // Display PID (handle up to 2 digits)
        u32 pid = current_process->pid;
        if (pid >= 10) {
            vga_mem[line21_offset + pos++] = 0x0F00 + ('0' + (pid / 10)); // Tens digit
            vga_mem[line21_offset + pos++] = 0x0F00 + ('0' + (pid % 10)); // Ones digit
        } else {
            vga_mem[line21_offset + pos++] = 0x0F00 + ('0' + pid); // Single digit
        }
        
        // Closing parenthesis
        vga_mem[line21_offset + pos++] = 0x0B00 + ')'; // Cyan
    } else {
        const char* idle_text = "IDLE";
        for (int i = 0; idle_text[i] != '\0'; i++) {
            vga_mem[line21_offset + pos++] = 0x0800 + idle_text[i]; // Dark gray
        }
    }
}

void ProcessManager::init_process_context(Process* process, ProcessFunction function) {
    // Initialize all registers to 0
    process->context.eax = 0;
    process->context.ebx = 0;
    process->context.ecx = 0;
    process->context.edx = 0;
    process->context.esi = 0;
    process->context.edi = 0;
    process->context.ebp = 0;
    
    // Set up stack pointer (grows downward)
    process->context.esp = process->stack_base + process->stack_size - 4;
    process->context.kernel_esp = process->context.esp;
    
    // Set entry point
    process->context.eip = (u32)function;
    
    // Set default flags (interrupts enabled)
    process->context.eflags = 0x202;
    
    // Set segment registers (kernel segments for now)
    process->context.ds = 0x10; // Kernel data segment
    process->context.es = 0x10;
    process->context.fs = 0x10;
    process->context.gs = 0x10;
}

u32 ProcessManager::allocate_stack() {
    if (next_stack_index >= MAX_PROCESSES) {
        return 0; // Out of stack space
    }
    
    // Return the address of the stack for this process
    u32 stack_addr = (u32)&process_stacks[next_stack_index][0];
    next_stack_index++;
    
    return stack_addr;
}

void ProcessManager::safe_strcpy(char* dest, const char* src, u32 max_len) {
    u32 i = 0;
    while (i < max_len && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

} // namespace kira::system 