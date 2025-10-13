#!/bin/bash

# Simple test to demonstrate the logging system working in ./logs

echo "=== Hydra Microkernel Logging System Test ==="
echo ""

cd /home/sahan/Hydra-Microkernal

# Clean up old logs
rm -f logs/node*.log logs/node*.pid logs/*.log

# Setup logs directory
./log_manager.sh setup

echo "1. Starting single node to test logging..."
timeout 5 qemu-system-riscv64 \
    -machine virt -cpu rv64 -m 256M \
    -kernel kernel.elf \
    -nographic -no-reboot \
    > logs/test_kernel.log 2>&1 &

NODE_PID=$!
echo "   Node started with PID $NODE_PID"

# Wait for kernel to initialize
sleep 3

# Kill the test node
kill $NODE_PID 2>/dev/null

echo ""
echo "2. Checking generated logs..."

if [ -f "logs/test_kernel.log" ]; then
    size=$(stat -c%s "logs/test_kernel.log" 2>/dev/null || echo "0")
    echo "   Kernel log generated: $(($size / 1024))KB"
    
    echo ""
    echo "3. Log content sample (first 10 lines):"
    head -n 10 logs/test_kernel.log | sed 's/^/   /'
    
    echo ""
    echo "4. Looking for structured log entries..."
    if grep -q "INFO\|ERROR\|WARN\|DEBUG" logs/test_kernel.log; then
        echo "   ✓ Found structured log entries"
        grep "INFO\|ERROR\|WARN\|DEBUG" logs/test_kernel.log | head -n 3 | sed 's/^/   /'
    else
        echo "   ⚠ No structured log entries found (using basic printk)"
    fi
    
else
    echo "   ❌ No kernel log generated"
fi

echo ""
echo "5. Final log directory status:"
./log_manager.sh status

echo ""
echo "=== Test completed ==="
echo "Logs are saved in: ./logs/"
echo "Use './log_manager.sh help' for log management options"