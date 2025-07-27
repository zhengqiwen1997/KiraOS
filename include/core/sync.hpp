#pragma once

#include "core/types.hpp"

// Forward declaration
namespace kira::system {
    struct Process;
}

namespace kira::sync {

using namespace kira::system;

/**
 * @brief Simple spinlock for short critical sections
 * Use for protecting data structures that are accessed briefly
 */
class Spinlock {
private:
    volatile u32 m_locked;  // 0 = unlocked, 1 = locked

public:
    Spinlock() : m_locked(0) {}
    
    /**
     * @brief Acquire the spinlock (busy wait)
     */
    void lock();
    
    /**
     * @brief Release the spinlock
     */
    void unlock();
    
    /**
     * @brief Try to acquire lock without blocking
     * @return true if lock acquired, false otherwise
     */
    bool try_lock();
};

/**
 * @brief Mutex for longer critical sections
 * Blocks the calling thread instead of busy waiting
 */
class Mutex {
private:
    volatile u32 m_locked;       // 0 = unlocked, 1 = locked
    u32 m_owner;                 // PID of owning process
    Process* m_waitQueue;        // Queue of waiting processes

public:
    Mutex() : m_locked(0), m_owner(0), m_waitQueue(nullptr) {}
    
    /**
     * @brief Acquire the mutex (blocks if necessary)
     */
    void lock();
    
    /**
     * @brief Release the mutex
     */
    void unlock();
    
    /**
     * @brief Try to acquire mutex without blocking
     * @return true if mutex acquired, false otherwise
     */
    bool try_lock();

private:
    void add_to_wait_queue(Process* process);
    Process* remove_from_wait_queue();
};

/**
 * @brief Semaphore for counting resources
 * Useful for limiting concurrent access to resources
 */
class Semaphore {
private:
    i32 m_count;                 // Available resource count (protected by m_lock)
    Process* m_waitQueue;        // Queue of waiting processes
    Spinlock m_lock;             // Protect internal state

public:
    explicit Semaphore(i32 initialCount) : m_count(initialCount), m_waitQueue(nullptr) {}
    
    /**
     * @brief Acquire a resource (decrements count)
     */
    void wait();
    
    /**
     * @brief Release a resource (increments count)
     */
    void signal();
    
    /**
     * @brief Try to acquire resource without blocking
     * @return true if resource acquired, false otherwise
     */
    bool try_wait();
    
    /**
     * @brief Get current count
     */
    i32 get_count() const { return m_count; }

private:
    void add_to_wait_queue(Process* process);
    Process* remove_from_wait_queue();
};

/**
 * @brief RAII lock guard for automatic lock management
 */
template<typename LockType>
class LockGuard {
private:
    LockType& m_lock;

public:
    explicit LockGuard(LockType& lock) : m_lock(lock) {
        m_lock.lock();
    }
    
    ~LockGuard() {
        m_lock.unlock();
    }
    
    // Prevent copying
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

// Convenience typedefs
using SpinlockGuard = LockGuard<Spinlock>;
using MutexGuard = LockGuard<Mutex>;

} // namespace kira::sync 