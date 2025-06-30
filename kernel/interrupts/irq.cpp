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

namespace kira::system::irq {

// IRQ handler table
static IRQHandler irq_handlers[16];

// IRQ statistics
static u32 irq_counts[16] = {0};

// VGA display for output
static display::VGADisplay* vga_display = nullptr;
static display::VGADisplay global_vga_display;

void initialize() {
    // Initialize PIC first
    PIC::initialize();
    
    // Initialize all handlers to default unhandled handler
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = handlers::unhandled_irq;
    }
    
    // Register specific handlers
    register_handler(PIC::IRQ_TIMER, handlers::timer_handler);
    register_handler(PIC::IRQ_KEYBOARD, handlers::keyboard_handler);
    
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

bool register_handler(u8 irq_number, IRQHandler handler) {
    if (irq_number >= 16 || !handler) {
        return false;
    }
    
    irq_handlers[irq_number] = handler;
    return true;
}

bool unregister_handler(u8 irq_number) {
    if (irq_number >= 16) {
        return false;
    }
    
    irq_handlers[irq_number] = handlers::unhandled_irq;
    return true;
}

bool enable_irq(u8 irq_number) {
    if (irq_number >= 16) {
        return false;
    }
    
    PIC::enable_irq(irq_number);
    return true;
}

bool disable_irq(u8 irq_number) {
    if (irq_number >= 16) {
        return false;
    }
    
    PIC::disable_irq(irq_number);
    return true;
}

bool is_irq_enabled(u8 irq_number) {
    if (irq_number >= 16) {
        return false;
    }
    
    u16 mask = PIC::get_irq_mask();
    return !(mask & (1 << irq_number));
}

u32 get_irq_count(u8 irq_number) {
    if (irq_number >= 16) {
        return 0;
    }
    
    return irq_counts[irq_number];
}

void default_handler(IRQFrame* frame) {
    // Extract IRQ number from interrupt number
    u8 irq_number = PIC::interrupt_to_irq(frame->interrupt_number);
    
    if (irq_number != 0xFF) {
        // Valid hardware interrupt
        irq_counts[irq_number]++;
        
        // Show interrupt activity on line 14 using direct VGA memory writes
        // BUT skip timer (IRQ 0) since timer_handler writes to line 14
        // COMMENTED OUT - IRQ display not needed for now
        /*
        if (irq_number != 0) {  // Don't overwrite timer display
            volatile u16* vga_mem = (volatile u16*)0xB8000;
            int line14_offset = 14 * 80;  // Line 14 start
            
            // Write "IRQ" in green at position 15 (after timer display)
            vga_mem[line14_offset + 15] = 0x2049;  // 'I' green on black
            vga_mem[line14_offset + 16] = 0x2052;  // 'R' green on black
            vga_mem[line14_offset + 17] = 0x2051;  // 'Q' green on black
            
            // Write IRQ number in white
            vga_mem[line14_offset + 18] = 0x0F30 + irq_number;  // IRQ number in white
            
            // Write ":" in green
            vga_mem[line14_offset + 19] = 0x203A;  // ':' green on black
            
            // Write count (simple approach - just show last digit for now)
            u32 count = irq_counts[irq_number];
            vga_mem[line14_offset + 20] = 0x0F30 + (count % 10);  // Last digit in white
            vga_mem[line14_offset + 21] = 0x0F30 + ((count / 10) % 10);  // Second to last digit
            vga_mem[line14_offset + 22] = 0x0F30 + ((count / 100) % 10);  // Third to last digit
        }
        */
        
        // Call registered handler
        if (irq_handlers[irq_number]) {
            irq_handlers[irq_number](frame);
        }
        
        // Send EOI to PIC
        PIC::send_eoi(irq_number);
    } else {
        // Not a hardware interrupt - no VGA output to avoid console conflicts
    }
}

void print_statistics() {
    if (!vga_display) return;
    
    vga_display->print_string(0, 0, "IRQ Statistics:", display::VGA_WHITE_ON_BLUE);
    
    for (int i = 0; i < 16; i++) {
        if (irq_counts[i] > 0) {
            vga_display->print_string(i + 1, 0, "IRQ ", display::VGA_CYAN_ON_BLUE);
            vga_display->print_decimal(i + 1, 4, i, display::VGA_CYAN_ON_BLUE);
            vga_display->print_string(i + 1, 6, ": ", display::VGA_CYAN_ON_BLUE);
            vga_display->print_decimal(i + 1, 8, irq_counts[i], display::VGA_WHITE_ON_BLUE);
        }
    }
}

namespace handlers {

void timer_handler(IRQFrame* frame) {
    // Timer tick - used for process scheduling
    static u32 timer_ticks = 0;
    timer_ticks++;
    
    // Call process scheduler every timer tick
    auto& pm = ProcessManager::get_instance();
    pm.schedule();
    
    // Timer functionality only - no VGA output to avoid conflicts with console
}

void keyboard_handler(IRQFrame* frame) {
    // Read scan code from keyboard
    u8 scan_code = inb(0x60);
    
    // Update keyboard state using the existing Keyboard class
    Keyboard::handle_key_press(scan_code);
    
    // Forward keyboard input to console (declared in kernel namespace in kernel.cpp)
    if (kira::kernel::console.handle_keyboard_input(scan_code)) {
        // Console handled the key, refresh display
        kira::kernel::console.refresh_display();
    }
}

void unhandled_irq(IRQFrame* frame) {
    // Default handler for unregistered IRQs (Line 19)
    u8 irq_number = PIC::interrupt_to_irq(frame->interrupt_number);
    
    if (vga_display) {
        vga_display->print_string(19, 0, "Unhandled IRQ ", display::VGA_RED_ON_BLUE);
        vga_display->print_decimal(19, 14, irq_number, display::VGA_RED_ON_BLUE);
    }
}

} // namespace handlers

} // namespace kira::system::irq

// C-style wrapper for assembly to call our C++ default handler
extern "C" void irq_default_handler_wrapper(kira::system::IRQFrame* frame) {
    kira::system::irq::default_handler(frame);
} 