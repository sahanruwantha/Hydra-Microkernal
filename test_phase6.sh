#!/bin/bash

# Test script for Phase 6 applications
echo "Testing Phase 6 applications..."

# Start QEMU in the background and send commands
(
    sleep 2  # Wait for kernel to boot
    echo "6"  # Trigger Phase 6 demo
    sleep 10  # Wait for demo to complete
    echo "q"  # Try to quit gracefully
) | timeout 30s qemu-system-riscv64 -machine virt -nographic -kernel kernel.elf

echo "Phase 6 test complete"
