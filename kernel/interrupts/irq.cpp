#include "interrupts/irq.hpp"
#include "interrupts/pic.hpp"
#include "arch/x86/idt.hpp"
#include "core/utils.hpp"
#include "core/io.hpp"
#include "display/vga.hpp"
#include "drivers/keyboard.hpp"
#include "core/process.hpp"
#include "display/console.hpp"

// Forward declaration of console from kernel namespace
namespace kira::kernel {
    extern kira::display::ScrollableConsole console;
}

namespace kira::system {

// IRQ handler table
static IRQHandler irq_handlers[16];

// IRQ statistics
static u32 irq_counts[16] = {0};

// VGA display for output
static kira::display::VGADisplay* vga_display = nullptr;
static kira::display::VGADisplay global_vga_display;

void initialize_irq() {
    // Initialize PIC first
    PIC::initialize();
    
    // Initialize all handlers to default unhandled handler
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = unhandled_irq;
    }
    
    // Register specific handlers
    register_handler(PIC::IRQ_TIMER, timer_handler);
    register_handler(PIC::IRQ_KEYBOARD, keyboard_handler);
    
    // Explicitly enable keyboard IRQ (IRQ1)
    enable_irq(PIC::IRQ_KEYBOARD);
    
    // Install IRQ stubs in IDT (interrupts 32-47)
    IDT::set_interrupt_gate(32, (void*)irq_stub_0);
    IDT::set_interrupt_gate(33, (void*)irq_stub_1);
    IDT::set_interrupt_gate(34, (void*)irq_stub_2);
    IDT::set_interrupt_gate(35, (void*)irq_stub_3);
    IDT::set_interrupt_gate(36, (void*)irq_stub_4);
    IDT::set_interrupt_gate(37, (void*)irq_stub_5);
    IDT::set_interrupt_gate(38, (void*)irq_stub_6);
    IDT::set_interrupt_gate(39, (void*)irq_stub_7);
    IDT::set_interrupt_gate(40, (void*)irq_stub_8);
    IDT::set_interrupt_gate(41, (void*)irq_stub_9);
    IDT::set_interrupt_gate(42, (void*)irq_stub_10);
    IDT::set_interrupt_gate(43, (void*)irq_stub_11);
    IDT::set_interrupt_gate(44, (void*)irq_stub_12);
    IDT::set_interrupt_gate(45, (void*)irq_stub_13);
    IDT::set_interrupt_gate(46, (void*)irq_stub_14);
    IDT::set_interrupt_gate(47, (void*)irq_stub_15);
    
    // Reload IDT
    IDT::load();
    
    // Initialize VGA display for output
    vga_display = &global_vga_display;
    
    // Enable interrupts
    sti();
}

bool register_handler(u8 irqNumber, IRQHandler handler) {
    if (irqNumber >= 16 || !handler) {
        return false;
    }
    
    irq_handlers[irqNumber] = handler;
    return true;
}

bool unregister_handler(u8 irqNumber) {
    if (irqNumber >= 16) {
        return false;
    }
    
    irq_handlers[irqNumber] = nullptr;
    return true;
}

bool enable_irq(u8 irqNumber) {
    if (irqNumber >= 16) {
        return false;
    }
    
    PIC::enable_irq(irqNumber);
    return true;
}

bool disable_irq(u8 irqNumber) {
    if (irqNumber >= 16) {
        return false;
    }
    
    PIC::disable_irq(irqNumber);
    return true;
}

bool is_irq_enabled(u8 irqNumber) {
    if (irqNumber >= 16) {
        return false;
    }
    
    u16 mask = PIC::get_irq_mask();
    return !(mask & (1 << irqNumber));
}

u32 get_irq_count(u8 irqNumber) {
    if (irqNumber >= 16) {
        return 0;
    }
    
    return irq_counts[irqNumber];
}

void default_handler(IRQFrame* frame) {
    // Convert interrupt number to IRQ number
    u8 irqNumber = PIC::interrupt_to_irq(frame->interruptNumber);
    
    // Validate IRQ number
    if (irqNumber >= 16) {
        // Invalid IRQ number - shouldn't happen
        PIC::send_eoi(0);  // Send EOI anyway to be safe
        return;
    }
    
    // Update statistics
    irq_counts[irqNumber]++;
    
    // Call registered handler or default
    if (irq_handlers[irqNumber]) {
        irq_handlers[irqNumber](frame);
    } else {
        unhandled_irq(frame);
    }
    
    // Send End of Interrupt signal
    PIC::send_eoi(irqNumber);
}

void print_statistics() {
    if (!vga_display) return;
    
    vga_display->print_string(0, 0, "IRQ Statistics:", kira::display::VGA_WHITE_ON_BLUE);
    
    for (int i = 0; i < 16; i++) {
        if (irq_counts[i] > 0) {
            vga_display->print_string(i + 1, 0, "IRQ ", kira::display::VGA_CYAN_ON_BLUE);
            vga_display->print_decimal(i + 1, 4, i, kira::display::VGA_CYAN_ON_BLUE);
            vga_display->print_string(i + 1, 6, ": ", kira::display::VGA_CYAN_ON_BLUE);
            vga_display->print_decimal(i + 1, 8, irq_counts[i], kira::display::VGA_WHITE_ON_BLUE);
        }
    }
}

void timer_handler(IRQFrame* frame) {
    // Timer tick - used for process scheduling
    static u32 timerTicks = 0;
    timerTicks++;
    
    // Call process scheduler every timer tick
    auto& pm = ProcessManager::get_instance();
    pm.schedule();
    
    // Timer functionality only - no VGA output to avoid conflicts with console
}

void keyboard_handler(IRQFrame* frame) {
    // Read scan code from keyboard
    u8 scanCode = inb(0x60);
    
    // Update keyboard state using the existing Keyboard class
    Keyboard::handle_key_press(scanCode);
    
    // Forward keyboard input to console (declared in kernel namespace in kernel.cpp)
    if (kira::kernel::console.handle_keyboard_input(scanCode)) {
        // Console handled the key, refresh display
        kira::kernel::console.refresh_display();
    }
}

void unhandled_irq(IRQFrame* frame) {
    u8 irqNumber = PIC::interrupt_to_irq(frame->interruptNumber);
    
    if (vga_display) {
        vga_display->print_string(19, 0, "Unhandled IRQ ", kira::display::VGA_RED_ON_BLUE);
        vga_display->print_decimal(19, 14, irqNumber, kira::display::VGA_RED_ON_BLUE);
    }
}

} // namespace kira::system

// C-style wrapper for assembly to call our C++ default handler
extern "C" void irq_default_handler_wrapper(kira::system::IRQFrame* frame) {
    kira::system::default_handler(frame);
} 