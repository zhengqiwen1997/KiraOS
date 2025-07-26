#include "core/sync.hpp"
#include "core/process.hpp"
#include "display/console.hpp"

// Forward declaration to access global console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::sync {

using namespace kira::system;

//=============================================================================
// Spinlock Implementation
//=============================================================================

void Spinlock::lock() {
    // Disable interrupts to prevent deadlock
    asm volatile("cli");
    
    // Busy wait until we can acquire the lock
    while (!try_lock()) {
        // Pause instruction helps with performance on some CPUs
        asm volatile("pause");
    }
}

void Spinlock::unlock() {
    // Release the lock
    m_locked = 0;
    
    // Re-enable interrupts
    asm volatile("sti");
}

bool Spinlock::try_lock() {
    // Atomic test-and-set operation
    u32 result;
    asm volatile(
        "xchgl %0, %1"
        : "=r"(result), "=m"(m_locked)
        : "0"(1)
        : "memory"
    );
    
    // Return true if we acquired the lock (previous value was 0)
    return result == 0;
}

//=============================================================================
// Mutex Implementation
//=============================================================================

void Mutex::lock() {
    auto& pm = ProcessManager::get_instance();
    Process* currentProcess = pm.get_current_process();
    
    if (!currentProcess) {
        // Kernel thread - fall back to spinlock behavior
        while (!try_lock()) {
            asm volatile("pause");
        }
        return;
    }
    
    // Try to acquire lock first
    if (try_lock()) {
        m_owner = currentProcess->pid;
        return;
    }
    
    // Lock is held - add to wait queue and block
    currentProcess->state = ProcessState::BLOCKED;
    add_to_wait_queue(currentProcess);
    
    // Yield to scheduler - we'll be woken up when lock is available
    pm.yield();
    
    // When we wake up, we should have the lock
    m_owner = currentProcess->pid;
}

void Mutex::unlock() {
    auto& pm = ProcessManager::get_instance();
    Process* currentProcess = pm.get_current_process();
    
    // Verify we own the lock
    if (currentProcess && m_owner != currentProcess->pid) {
        kira::kernel::console.add_message("ERROR: Process trying to unlock mutex it doesn't own!", kira::display::VGA_RED_ON_BLUE);
        return;
    }
    
    // Wake up next waiting process
    Process* nextProcess = remove_from_wait_queue();
    if (nextProcess) {
        pm.wake_up_process(nextProcess);
    }
    
    // Release the lock
    m_owner = 0;
    m_locked = 0;
}

bool Mutex::try_lock() {
    // Atomic test-and-set operation
    u32 result;
    asm volatile(
        "xchgl %0, %1"
        : "=r"(result), "=m"(m_locked)
        : "0"(1)
        : "memory"
    );
    
    if (result == 0) {
        // We acquired the lock
        auto& pm = ProcessManager::get_instance();
        Process* currentProcess = pm.get_current_process();
        if (currentProcess) {
            m_owner = currentProcess->pid;
        }
        return true;
    }
    
    return false;
}

void Mutex::add_to_wait_queue(Process* process) {
    if (!process) return;
    
    process->next = nullptr;
    
    if (!m_waitQueue) {
        m_waitQueue = process;
    } else {
        // Add to end of queue (FIFO)
        Process* current = m_waitQueue;
        while (current->next) {
            current = current->next;
        }
        current->next = process;
    }
}

Process* Mutex::remove_from_wait_queue() {
    if (!m_waitQueue) {
        return nullptr;
    }
    
    Process* process = m_waitQueue;
    m_waitQueue = m_waitQueue->next;
    process->next = nullptr;
    
    return process;
}

//=============================================================================
// Semaphore Implementation
//=============================================================================

void Semaphore::wait() {
    auto& pm = ProcessManager::get_instance();
    Process* currentProcess = pm.get_current_process();
    
    if (!currentProcess) {
        // Kernel thread - busy wait (not ideal but safe)
        while (true) {
            {
                SpinlockGuard guard(m_lock);
                if (m_count > 0) {
                    m_count--;
                    return;
                }
            } // Release lock before busy wait
            asm volatile("pause");
        }
    }
    
    // User process - can block
    while (true) {
        {
            SpinlockGuard guard(m_lock);
            if (m_count > 0) {
                // Resource available - take it
                m_count--;
                return;
            }
            
            // No resources available - block
            currentProcess->state = ProcessState::BLOCKED;
            add_to_wait_queue(currentProcess);
        } // Release lock before yielding
        
        // Yield to scheduler
        pm.yield();
        
        // When we wake up, try again
    }
}

void Semaphore::signal() {
    SpinlockGuard guard(m_lock);
    
    // Wake up a waiting process if any
    Process* nextProcess = remove_from_wait_queue();
    if (nextProcess) {
        auto& pm = ProcessManager::get_instance();
        pm.wake_up_process(nextProcess);
    } else {
        // No waiting processes - increment count
        m_count++;
    }
}

bool Semaphore::try_wait() {
    SpinlockGuard guard(m_lock);
    
    if (m_count > 0) {
        m_count--;
        return true;
    }
    
    return false;
}

void Semaphore::add_to_wait_queue(Process* process) {
    if (!process) return;
    
    process->next = nullptr;
    
    if (!m_waitQueue) {
        m_waitQueue = process;
    } else {
        // Add to end of queue (FIFO)
        Process* current = m_waitQueue;
        while (current->next) {
            current = current->next;
        }
        current->next = process;
    }
}

Process* Semaphore::remove_from_wait_queue() {
    if (!m_waitQueue) {
        return nullptr;
    }
    
    Process* process = m_waitQueue;
    m_waitQueue = m_waitQueue->next;
    process->next = nullptr;
    
    return process;
}

} // namespace kira::sync 