#!/bin/bash

# Hydra Distributed Microkernel - Dual Node Test Script
# This script launches two QEMU instances with different node configurations
# to test the distributed process migration system

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_IMAGE="$SCRIPT_DIR/kernel.elf"
QEMU_SYSTEM="qemu-system-riscv64"

# Network configuration
NETWORK_BRIDGE="hydra-br0"
TAP1="hydra-tap1"
TAP2="hydra-tap2"
NODE1_IP="10.0.0.2"
NODE2_IP="10.0.0.3"
NETMASK="255.255.255.0"

# QEMU configuration
MEMORY="256M"
CPU_TYPE="rv64"
MACHINE="virt"

# Ports for VNC and monitor (to avoid conflicts)
NODE1_VNC_PORT=5901
NODE2_VNC_PORT=5902
NODE1_MONITOR_PORT=4444
NODE2_MONITOR_PORT=4445

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_banner() {
    echo -e "${BLUE}"
    echo "============================================="
    echo "  Hydra Distributed Microkernel Test Setup  "
    echo "============================================="
    echo -e "${NC}"
}

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    print_status "Checking dependencies..."
    
    if ! command -v $QEMU_SYSTEM &> /dev/null; then
        print_error "QEMU RISC-V system emulator not found: $QEMU_SYSTEM"
        exit 1
    fi
    
    if [ ! -f "$KERNEL_IMAGE" ]; then
        print_error "Kernel image not found: $KERNEL_IMAGE"
        print_status "Please build the kernel first with: make all"
        exit 1
    fi
    
    print_status "Dependencies OK"
}

setup_network() {
    print_status "Setting up network bridge and TAP interfaces..."
    
    # Check if running as root or with sudo
    if [ "$EUID" -ne 0 ]; then
        print_warning "Network setup requires root privileges"
        print_status "You may need to run this script with sudo, or set up networking manually"
        return 0
    fi
    
    # Create bridge if it doesn't exist
    if ! ip link show $NETWORK_BRIDGE &> /dev/null; then
        print_status "Creating network bridge: $NETWORK_BRIDGE"
        ip link add $NETWORK_BRIDGE type bridge
        ip addr add 10.0.0.1/24 dev $NETWORK_BRIDGE
        ip link set $NETWORK_BRIDGE up
    else
        print_status "Network bridge $NETWORK_BRIDGE already exists"
    fi
    
    # Create TAP interfaces
    for tap in $TAP1 $TAP2; do
        if ! ip link show $tap &> /dev/null; then
            print_status "Creating TAP interface: $tap"
            ip tuntap add $tap mode tap user $(whoami)
            ip link set $tap master $NETWORK_BRIDGE
            ip link set $tap up
        else
            print_status "TAP interface $tap already exists"
        fi
    done
    
    print_status "Network setup complete"
    print_status "Bridge: $NETWORK_BRIDGE (10.0.0.1/24)"
    print_status "Node 1: $TAP1 -> $NODE1_IP"
    print_status "Node 2: $TAP2 -> $NODE2_IP"
}

cleanup_network() {
    if [ "$EUID" -eq 0 ]; then
        print_status "Cleaning up network interfaces..."
        
        for tap in $TAP1 $TAP2; do
            if ip link show $tap &> /dev/null; then
                print_status "Removing TAP interface: $tap"
                ip link delete $tap
            fi
        done
        
        if ip link show $NETWORK_BRIDGE &> /dev/null; then
            print_status "Removing network bridge: $NETWORK_BRIDGE"
            ip link delete $NETWORK_BRIDGE
        fi
    fi
}

start_node() {
    local node_id=$1
    local node_type=$2
    local tap_device=$3
    local vnc_port=$4
    local monitor_port=$5
    local ip_addr=$6
    
    # Ensure logs directory exists
    mkdir -p logs
    
    local log_file="logs/node${node_id}.log"
    local pid_file="logs/node${node_id}.pid"
    
    print_status "Starting Node $node_id ($node_type) on $ip_addr..."
    
    # Enhanced QEMU command with socket networking for inter-node communication
    local socket_path="/tmp/hydra_net_${node_id}.sock"
    
    # Create the socket network for node communication
    if [ "$node_id" = "1" ]; then
        # Node 1 creates the socket and listens
        local netdev_config="socket,id=net0,listen=:1234"
    else
        # Node 2 connects to Node 1's socket
        local netdev_config="socket,id=net0,connect=127.0.0.1:1234"
    fi
    
    local qemu_cmd="$QEMU_SYSTEM \
        -machine $MACHINE \
        -cpu $CPU_TYPE \
        -m $MEMORY \
        -kernel $KERNEL_IMAGE \
        -nographic \
        -no-reboot \
        -netdev $netdev_config \
        -device virtio-net-device,netdev=net0,mac=52:54:00:12:34:$(printf '%02d' $node_id) \
        -vnc :$(($vnc_port - 5900)) \
        -monitor tcp:127.0.0.1:$monitor_port,server,nowait"
    
    print_status "QEMU command: $qemu_cmd"
    
    # Start QEMU in background and save PID
    nohup $qemu_cmd > $log_file 2>&1 &
    local qemu_pid=$!
    echo $qemu_pid > $pid_file
    
    print_status "Node $node_id started with PID $qemu_pid"
    print_status "  Log file: $log_file"
    print_status "  VNC port: $vnc_port"
    print_status "  Monitor port: $monitor_port"
    
    # Wait a moment for QEMU to start
    sleep 2
    
    # Check if process is still running
    if ! kill -0 $qemu_pid 2>/dev/null; then
        print_error "Node $node_id failed to start!"
        print_status "Check log file: $log_file"
        return 1
    fi
    
    return 0
}

stop_nodes() {
    print_status "Stopping all nodes..."
    
    for node_id in 1 2; do
        local pid_file="logs/node${node_id}.pid"
        if [ -f "$pid_file" ]; then
            local pid=$(cat "$pid_file")
            if kill -0 $pid 2>/dev/null; then
                print_status "Stopping Node $node_id (PID $pid)"
                kill $pid
                sleep 1
                # Force kill if still running
                if kill -0 $pid 2>/dev/null; then
                    kill -9 $pid
                fi
            fi
            rm -f "$pid_file"
        fi
    done
}

show_status() {
    print_status "Checking node status..."
    
    for node_id in 1 2; do
        local pid_file="logs/node${node_id}.pid"
        local log_file="logs/node${node_id}.log"
        
        if [ -f "$pid_file" ]; then
            local pid=$(cat "$pid_file")
            if kill -0 $pid 2>/dev/null; then
                print_status "Node $node_id: RUNNING (PID $pid)"
                if [ -f "$log_file" ]; then
                    echo "  Last log lines:"
                    tail -n 3 "$log_file" | sed 's/^/    /'
                fi
            else
                print_warning "Node $node_id: STOPPED (PID file exists but process not running)"
            fi
        else
            print_warning "Node $node_id: NOT STARTED (no PID file)"
        fi
    done
}

show_help() {
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  start     - Start both kernel nodes"
    echo "  stop      - Stop all nodes"
    echo "  restart   - Stop and start all nodes"
    echo "  status    - Show status of all nodes"
    echo "  setup     - Setup network interfaces (requires root)"
    echo "  cleanup   - Remove network interfaces (requires root)"
    echo "  logs      - Show recent logs from both nodes"
    echo "  help      - Show this help message"
    echo ""
    echo "Node Configuration:"
    echo "  Node 1: Compute node ($NODE1_IP) - VNC :$NODE1_VNC_PORT"
    echo "  Node 2: Graphics node ($NODE2_IP) - VNC :$NODE2_VNC_PORT"
    echo ""
    echo "Testing:"
    echo "  1. Build kernel: make all"
    echo "  2. Setup network: sudo $0 setup"
    echo "  3. Start nodes: $0 start"
    echo "  4. Connect via VNC or check logs"
    echo "  5. Test commands: 'd' (status), 'm' (migrate), 'f' (find nodes)"
}

show_logs() {
    for node_id in 1 2; do
        local log_file="logs/node${node_id}.log"
        if [ -f "$log_file" ]; then
            echo -e "${BLUE}=== Node $node_id Log (last 10 lines) ===${NC}"
            tail -n 10 "$log_file"
            echo ""
        else
            print_warning "Log file not found: $log_file"
        fi
    done
}

# Main script logic
case "${1:-help}" in
    start)
        print_banner
        check_dependencies
        
        # Stop any existing nodes first
        stop_nodes
        
        print_status "Starting distributed microkernel test setup..."
        
        # Start Node 1 (Compute)
        if start_node 1 "compute" $TAP1 $NODE1_VNC_PORT $NODE1_MONITOR_PORT $NODE1_IP; then
            sleep 3
            
            # Start Node 2 (Graphics)
            if start_node 2 "graphics" $TAP2 $NODE2_VNC_PORT $NODE2_MONITOR_PORT $NODE2_IP; then
                print_status "Both nodes started successfully!"
                echo ""
                print_status "Test the distributed system:"
                print_status "  1. Connect to Node 1: nc localhost $NODE1_MONITOR_PORT (or VNC localhost:$NODE1_VNC_PORT)"
                print_status "  2. Connect to Node 2: nc localhost $NODE2_MONITOR_PORT (or VNC localhost:$NODE2_VNC_PORT)"
                print_status "  3. Use commands: 'd' (status), 'f' (find nodes), 'm' (migrate raytracer)"
                print_status "  4. Check logs: $0 logs"
                print_status "  5. Stop nodes: $0 stop"
            else
                print_error "Failed to start Node 2"
                stop_nodes
                exit 1
            fi
        else
            print_error "Failed to start Node 1"
            exit 1
        fi
        ;;
        
    stop)
        print_banner
        stop_nodes
        ;;
        
    restart)
        print_banner
        stop_nodes
        sleep 2
        exec $0 start
        ;;
        
    status)
        print_banner
        show_status
        ;;
        
    setup)
        print_banner
        setup_network
        ;;
        
    cleanup)
        print_banner
        stop_nodes
        cleanup_network
        ;;
        
    logs)
        show_logs
        ;;
        
    help|*)
        print_banner
        show_help
        ;;
esac
