#pragma once

#include "core/types.hpp"

namespace kira::drivers {

using namespace kira::system;

/**
 * @brief Programmable Interval Timer (PIT) Driver
 * 
 * The PIT is responsible for generating timer interrupts at regular intervals.
 * It uses the 8253/8254 timer chip to generate IRQ 0 (timer interrupt).
 */
class Timer {
public:
    // PIT I/O ports
    static constexpr u16 PIT_CHANNEL_0 = 0x40;  // Channel 0 data port (IRQ 0)
    static constexpr u16 PIT_CHANNEL_1 = 0x41;  // Channel 1 data port (RAM refresh)
    static constexpr u16 PIT_CHANNEL_2 = 0x42;  // Channel 2 data port (PC speaker)
    static constexpr u16 PIT_COMMAND   = 0x43;  // Command register
    
    // PIT frequency
    static constexpr u32 PIT_FREQUENCY = 1193182;  // Base frequency in Hz
    
    // Command register bits
    static constexpr u8 PIT_CHANNEL_0_SELECT = 0x00;   // Select channel 0
    static constexpr u8 PIT_ACCESS_LOHI      = 0x30;   // Access mode: lobyte/hibyte
    static constexpr u8 PIT_MODE_SQUARE_WAVE = 0x06;   // Mode 3: Square wave generator
    static constexpr u8 PIT_BINARY_MODE      = 0x00;   // Binary mode (not BCD)
    
public:
    /**
     * @brief Initialize the PIT to generate timer interrupts
     * @param frequency Desired interrupt frequency in Hz (default: 100 Hz = 10ms intervals)
     */
    static void initialize(u32 frequency = 100);
    
    /**
     * @brief Get the current timer frequency
     */
    static u32 get_frequency();
    
    /**
     * @brief Set a new timer frequency
     * @param frequency New frequency in Hz
     */
    static void set_frequency(u32 frequency);

private:
    static u32 currentFrequency;
    
    /**
     * @brief Calculate the divisor for a given frequency
     * @param frequency Desired frequency in Hz
     * @return Divisor value for the PIT
     */
    static u16 calculate_divisor(u32 frequency);
};

} // namespace kira::drivers 