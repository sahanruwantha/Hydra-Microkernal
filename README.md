# Pakaya-Microkernal

Pakaya-Microkernal is a microkernel-based operating system designed with a focus on distributed computing and task offloading capabilities. The project implements a minimalistic yet powerful microkernel architecture that supports basic process management, inter-process communication, and network-based task distribution.

## Project Overview

The microkernel implements several core components:

1. **Process Management**
   - Process creation and termination
   - Network-aware Process IDs (PIDs)
   - State management (running, ready, blocked)
   - Context switching

2. **Inter-Process Communication (IPC)**
   - Message passing system
   - Port-based communication
   - Message queuing

3. **Network Communication**
   - Node discovery
   - Task offloading
   - Load balancing
   - Health monitoring

4. **Task Scheduling**
   - Round-robin scheduling
   - Priority-based execution
   - Load balancing
   - Task migration

5. **Memory Management**
   - Memory allocation/deallocation
   - Process isolation
   - Migration support
   - Shared memory for IPC

## Building the Project

### Prerequisites

- GCC (32-bit support required)
- GNU Make
- GRUB
- QEMU (for testing)

### Build Instructions

1. Build the kernel:
   ```bash
   make build
   ```
   This will create a bootable ISO image at `build/PakayaOs.iso`

2. Clean build files:
   ```bash
   make clean
   ```

## Running the OS

### Using QEMU

To run the OS in QEMU, use:
```bash
make run
```

This will start QEMU with the following configuration:
- 512MB of RAM
- Boot from CD-ROM
- No reboot on kernel panic
- No shutdown on halt

