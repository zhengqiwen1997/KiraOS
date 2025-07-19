#!/bin/bash

# Create test disk image for ATA driver testing
echo "Creating test disk image..."

# Create a 10MB disk image
dd if=/dev/zero of=test_disk.img bs=1M count=10 2>/dev/null

# Write test data to specific sectors
echo "Writing test data to sectors..."

# Sector 0 (Boot sector) - Write a simple signature
printf "KIRATEST" | dd of=test_disk.img bs=1 seek=0 conv=notrunc 2>/dev/null

# Sector 1 - Write test pattern
for i in {0..511}; do
    printf "\\x$(printf "%02x" $((i % 256)))"
done | dd of=test_disk.img bs=1 seek=512 conv=notrunc 2>/dev/null

# Sector 2 - Write ASCII text
printf "Hello from KiraOS ATA driver test! This is sector 2 data." | dd of=test_disk.img bs=1 seek=1024 conv=notrunc 2>/dev/null

# Sector 3 - Write repeating pattern
for i in {0..511}; do
    printf "\\x$(printf "%02x" $((0xAA)))"
done | dd of=test_disk.img bs=1 seek=1536 conv=notrunc 2>/dev/null

echo "Test disk image created: test_disk.img"
echo "Sector 0: KIRATEST signature"
echo "Sector 1: Incrementing byte pattern (0x00-0xFF)"
echo "Sector 2: ASCII text message"
echo "Sector 3: 0xAA pattern"

# Make it executable
chmod +x create_test_disk.sh 