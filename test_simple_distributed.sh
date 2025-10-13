#!/bin/bash

# Simple test demonstrating the distributed system 

echo "=== Hydra Distributed Microkernel Demo ==="
echo

# Node 1 on VNC display :1 (for interaction)
echo "Starting Node 1 (Primary) with VNC on localhost:5901..."
qemu-system-riscv64 \
    -machine virt \
    -cpu rv64 \
    -m 256M \
    -kernel kernel.elf \
    -nographic \
    -vnc :1 \
    -monitor telnet:localhost:4444,server,nowait \
    -netdev user,id=net0,restrict=off,hostfwd=tcp::8080-:80 \
    -device virtio-net-device,netdev=net0 \
    > node1_simple.log 2>&1 &

NODE1_PID=$!
sleep 2

echo "Node 1 started (PID: $NODE1_PID)"
echo "  - VNC: localhost:5901 (for keyboard interaction)"
echo "  - Monitor: telnet localhost 4444"
echo "  - Log: node1_simple.log"
echo

echo "=== Commands you can type in the VNC console ==="
echo "  d = Show distributed system status"
echo "  m = Migrate raytracer process"
echo "  h = Send heartbeat to all nodes"
echo "  f = Find nodes on network"
echo "  n = Send network test packet"
echo "  s = Show network statistics"
echo

echo "=== Demo Instructions ==="
echo "1. Connect to VNC: vncviewer localhost:5901"
echo "2. Type 'd' to see distributed system status"
echo "3. Type 'f' to discover other nodes"
echo "4. Type 'm' to attempt process migration"
echo

echo "Press Enter to see logs (or Ctrl+C to stop)..."
read

echo "=== Node 1 Startup Log ==="
cat node1_simple.log

echo
echo "=== Live Log (Ctrl+C to stop) ==="
tail -f node1_simple.log &
TAIL_PID=$!

# Wait for user to stop
trap "kill $NODE1_PID $TAIL_PID 2>/dev/null; exit 0" INT TERM

wait