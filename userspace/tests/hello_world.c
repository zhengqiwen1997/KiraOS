// Simple Hello World program for KiraOS ELF loading test
// This will be compiled to ELF and loaded by our ELF loader

// We need to use our system call interface
// For now, let's make a simple program that just loops

void _start() {
    // Simple loop that does some work
    // In a real implementation, this would call system calls
    
    volatile int counter = 0;
    
    // Do some work to show the program is running
    for (int i = 0; i < 1000000; i++) {
        counter++;
        
        // Simple delay
        for (int j = 0; j < 100; j++) {
            asm volatile("nop");
        }
    }
    
    // Infinite loop (since we don't have exit() yet)
    while (1) {
        for (int k = 0; k < 1000000; k++) {
            asm volatile("nop");
        }
    }
} 