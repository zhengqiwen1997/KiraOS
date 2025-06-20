#include "interrupts/irq.hpp"
#include "interrupts/pic.hpp"
#include "arch/x86/idt.hpp"
#include "core/utils.hpp"
#include "core/io.hpp"
#include "display/vga.hpp"

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
        volatile u16* vga_mem = (volatile u16*)0xB8000;
        int line14_offset = 14 * 80;  // Line 14 start
        
        // Write "IRQ" in green
        vga_mem[line14_offset + 0] = 0x2049;  // 'I' green on black
        vga_mem[line14_offset + 1] = 0x2052;  // 'R' green on black
        vga_mem[line14_offset + 2] = 0x2051;  // 'Q' green on black
        
        // Write IRQ number in white
        vga_mem[line14_offset + 3] = 0x0F30 + irq_number;  // IRQ number in white
        
        // Write ":" in green
        vga_mem[line14_offset + 4] = 0x203A;  // ':' green on black
        
        // Write count (simple approach - just show last digit for now)
        u32 count = irq_counts[irq_number];
        vga_mem[line14_offset + 5] = 0x0F30 + (count % 10);  // Last digit in white
        vga_mem[line14_offset + 6] = 0x0F30 + ((count / 10) % 10);  // Second to last digit
        vga_mem[line14_offset + 7] = 0x0F30 + ((count / 100) % 10);  // Third to last digit
        
        // Call registered handler
        if (irq_handlers[irq_number]) {
            irq_handlers[irq_number](frame);
        }
        
        // Send EOI to PIC
        PIC::send_eoi(irq_number);
    } else {
        // Not a hardware interrupt
        volatile u16* vga_mem = (volatile u16*)0xB8000;
        int line14_offset = 14 * 80;
        vga_mem[line14_offset + 20] = 0x0C23;  // '#' in red
        vga_mem[line14_offset + 21] = 0x0F30 + (frame->interrupt_number % 10);
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
    // Timer tick - can be used for scheduling, timekeeping, etc.
    static u32 timer_ticks = 0;
    timer_ticks++;
    
    // Show timer activity using direct VGA memory writes
    volatile u16* vga_mem = (volatile u16*)0xB8000;
    
    // Line 15: Show timer handler activity
    int line15_offset = 15 * 80;
    
    // Write "TIMER:" in green
    vga_mem[line15_offset + 0] = 0x2054;  // 'T'
    vga_mem[line15_offset + 1] = 0x2049;  // 'I'
    vga_mem[line15_offset + 2] = 0x204D;  // 'M'
    vga_mem[line15_offset + 3] = 0x2045;  // 'E'
    vga_mem[line15_offset + 4] = 0x2052;  // 'R'
    vga_mem[line15_offset + 5] = 0x203A;  // ':'
    
    // Write tick count (last 4 digits)
    vga_mem[line15_offset + 6] = 0x0F30 + ((timer_ticks / 1000) % 10);
    vga_mem[line15_offset + 7] = 0x0F30 + ((timer_ticks / 100) % 10);
    vga_mem[line15_offset + 8] = 0x0F30 + ((timer_ticks / 10) % 10);
    vga_mem[line15_offset + 9] = 0x0F30 + (timer_ticks % 10);
    
    // Line 17: Show dots every few ticks
    if ((timer_ticks % 5) == 0) {
        int line17_offset = 17 * 80;
        int dot_position = (timer_ticks / 5) % 70;
        vga_mem[line17_offset + dot_position] = 0x202E;  // '.' in green
    }
}

void keyboard_handler(IRQFrame* frame) {
    // Read scan code from keyboard
    u8 scan_code = inb(0x60);
    
    static u32 kb_count = 0;
    kb_count++;
    
    // Show keyboard activity using direct VGA memory writes
    volatile u16* vga_mem = (volatile u16*)0xB8000;
    
    // Line 16: Show keyboard handler activity
    int line16_offset = 16 * 80;
    
    // Write "KEYBOARD:" in magenta
    vga_mem[line16_offset + 0] = 0x054B;  // 'K'
    vga_mem[line16_offset + 1] = 0x0545;  // 'E'
    vga_mem[line16_offset + 2] = 0x0559;  // 'Y'
    vga_mem[line16_offset + 3] = 0x0542;  // 'B'
    vga_mem[line16_offset + 4] = 0x054F;  // 'O'
    vga_mem[line16_offset + 5] = 0x0541;  // 'A'
    vga_mem[line16_offset + 6] = 0x0552;  // 'R'
    vga_mem[line16_offset + 7] = 0x0544;  // 'D'
    vga_mem[line16_offset + 8] = 0x053A;  // ':'
    
    // Write key count (last 3 digits)
    vga_mem[line16_offset + 9] = 0x0F30 + ((kb_count / 100) % 10);
    vga_mem[line16_offset + 10] = 0x0F30 + ((kb_count / 10) % 10);
    vga_mem[line16_offset + 11] = 0x0F30 + (kb_count % 10);
    
    // Line 18: Show scan codes
    int line18_offset = 18 * 80;
    static u32 key_position = 0;
    u32 col = (key_position % 25) * 3;  // 25 scan codes per line
    
    // Convert scan code to hex and display
    char hex_chars[] = "0123456789ABCDEF";
    vga_mem[line18_offset + col] = 0x0E00 + hex_chars[(scan_code >> 4) & 0xF];  // High nibble in yellow
    vga_mem[line18_offset + col + 1] = 0x0E00 + hex_chars[scan_code & 0xF];     // Low nibble in yellow
    vga_mem[line18_offset + col + 2] = 0x0E20;  // Space in yellow
    
    key_position++;
    
    // Clear line when it gets full
    if ((key_position % 25) == 0) {
        for (int i = 0; i < 75; i++) {
            vga_mem[line18_offset + i] = 0x0720;  // Space in white
        }
        key_position = 0;
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

// Simple C-style interrupt handler for testing
extern "C" void simple_irq_test_handler() {
    // Write directly to VGA memory to test
    volatile kira::system::u16* vga_mem = (volatile kira::system::u16*)0xB8000;
    static kira::system::u32 test_count = 0;
    test_count++;
    
    // Write "!" at position (18, 30) = offset 18*80+30 = 1470
    kira::system::u16 pos = 18 * 80 + 30 + (test_count % 10);
    vga_mem[pos] = 0x4C00 | '!';  // Red background, black text, '!' character
}

// C-style wrapper for our default handler to avoid name mangling issues
extern "C" void irq_default_handler_wrapper(kira::system::IRQFrame* frame) {
    // DEBUG: Write 'D' to show C wrapper was called
    volatile kira::system::u16* vga_mem = (volatile kira::system::u16*)0xB8000;
    // Position (14, 16) = 14*80+16 = 1136 * 2 = 2272 bytes
    vga_mem[1136] = 0x4C44;  // Red background, white text, 'D' character
    
    // Call our actual C++ handler
    kira::system::irq::default_handler(frame);
} 