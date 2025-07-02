#include "drivers/timer.hpp"
#include "core/io.hpp"

namespace kira::drivers {

using namespace kira::system;

// Static member definition
u32 Timer::currentFrequency = 0;

void Timer::initialize(u32 frequency) {
    // Calculate the divisor for the desired frequency
    u16 divisor = calculate_divisor(frequency);
    
    // Send command to PIT
    // Channel 0, Access mode: lobyte/hibyte, Mode 3: Square wave, Binary mode
    u8 command = PIT_CHANNEL_0_SELECT | PIT_ACCESS_LOHI | PIT_MODE_SQUARE_WAVE | PIT_BINARY_MODE;
    outb(PIT_COMMAND, command);
    
    // Send the divisor (low byte first, then high byte)
    outb(PIT_CHANNEL_0, divisor & 0xFF);         // Low byte
    outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF); // High byte
    
    // Store the current frequency
    currentFrequency = frequency;
}

u32 Timer::get_frequency() {
    return currentFrequency;
}

void Timer::set_frequency(u32 frequency) {
    initialize(frequency);
}

u16 Timer::calculate_divisor(u32 frequency) {
    // Calculate divisor: PIT_FREQUENCY / desired_frequency
    // Ensure we don't divide by zero and don't exceed 16-bit range
    if (frequency == 0) {
        frequency = 100; // Default to 100 Hz
    }
    
    u32 divisor = PIT_FREQUENCY / frequency;
    
    // Clamp to 16-bit range (1 to 65535)
    if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    } else if (divisor < 1) {
        divisor = 1;
    }
    
    return static_cast<u16>(divisor);
}

} // namespace kira::drivers 