#!/bin/bash

# Pakaya Microkernel QEMU Runner Script
# Usage: ./run_qemu.sh [options]

set -e

KERNEL="kernel.elf"
MACHINE="virt"
MEMORY="128M"
CPU_COUNT="1"

# Default QEMU options
QEMU_OPTS="-machine $MACHINE -m $MEMORY -smp $CPU_COUNT -nographic"

# Parse command line arguments
DEBUG=false
NETWORK=false
MONITOR=false
HELP=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            DEBUG=true
            shift
            ;;
        -n|--network)
            NETWORK=true
            shift
            ;;
        -m|--monitor)
            MONITOR=true
            shift
            ;;
        -h|--help)
            HELP=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Show help
if [ "$HELP" = true ]; then
    echo "Pakaya Microkernel QEMU Runner"
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -d, --debug     Enable GDB server (listen on port 1234)"
    echo "  -n, --network   Enable network (virtio-net)"
    echo "  -m, --monitor   Enable QEMU monitor console"
    echo "  -h, --help      Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0              # Normal run"
    echo "  $0 -d           # Debug mode"
    echo "  $0 -n           # With network"
    echo "  $0 -d -n        # Debug with network"
    echo ""
    echo "In debug mode, connect GDB with:"
    echo "  riscv64-unknown-elf-gdb kernel.elf"
    echo "  (gdb) target remote localhost:1234"
    echo "  (gdb) continue"
    exit 0
fi

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo "Error: $KERNEL not found. Please build the kernel first with 'make'"
    exit 1
fi

# Add debug options
if [ "$DEBUG" = true ]; then
    QEMU_OPTS="$QEMU_OPTS -s -S"
    echo "Debug mode enabled - GDB server listening on port 1234"
    echo "Connect with: riscv64-unknown-elf-gdb $KERNEL"
    echo "Then: (gdb) target remote localhost:1234"
    echo ""
fi

# Add network options
if [ "$NETWORK" = true ]; then
    QEMU_OPTS="$QEMU_OPTS -netdev user,id=net0 -device virtio-net-device,netdev=net0"
    echo "Network enabled (virtio-net)"
fi

# Add monitor console
if [ "$MONITOR" = true ]; then
    QEMU_OPTS="$QEMU_OPTS -monitor stdio"
    echo "QEMU monitor enabled on stdio"
    echo "Use Ctrl+A, C to switch between console and monitor"
fi

echo "Starting QEMU with options: $QEMU_OPTS"
echo "Kernel: $KERNEL"
echo ""
echo "To exit QEMU: Ctrl+A, X"
echo "=================================================================================="

# Run QEMU
exec qemu-system-riscv64 $QEMU_OPTS -kernel $KERNEL
