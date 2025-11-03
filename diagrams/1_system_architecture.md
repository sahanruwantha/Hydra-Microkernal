# Hydra Microkernel System Architecture

## Overview
This diagram shows the core microkernel architecture of Hydra with its distributed capabilities.

```mermaid
graph TB
    subgraph "User Space"
        APP1[Distributed Raytracer]
        APP2[Network Client]
        APP3[Library Test]
        LIBC[libc Library]
        LIBHYDRA[libhydra Library]
        LIBNET[libnet Library]
    end
    
    subgraph "Kernel Space - Core Microkernel"
        MAIN[Kernel Main]
        SCHED[Scheduler<br/>- Task States<br/>- Context Switch<br/>- Round Robin]
        IPC[IPC System<br/>- Message Passing<br/>- Queues<br/>- 512B Max Msg]
        MM[Memory Manager<br/>- Page Allocation<br/>- Heap Management]
        SYSCALL[System Calls<br/>- Process Control<br/>- IPC Interface]
    end
    
    subgraph "Distributed System Layer"
        DIST[Distributed Manager<br/>- Node Discovery<br/>- Heartbeat Protocol<br/>- Load Balancing]
        MIGRATION[Process Migration<br/>- Serialization<br/>- 4-Phase Protocol<br/>- State Capture]
        NET[Network Driver<br/>- UDP/TCP Support<br/>- lwIP Integration]
    end
    
    subgraph "Hardware Abstraction"
        UART[UART Driver]
        TIMER[Timer System]
        NET_HW[Network Hardware]
        TRAP[Trap Handlers]
    end
    
    %% User-Kernel Communication
    APP1 --> SYSCALL
    APP2 --> SYSCALL
    APP3 --> SYSCALL
    LIBC --> SYSCALL
    LIBHYDRA --> SYSCALL
    
    %% Core Kernel Components
    MAIN --> SCHED
    MAIN --> IPC
    MAIN --> MM
    MAIN --> DIST
    MAIN --> MIGRATION
    
    SCHED --> IPC
    SCHED --> MM
    SYSCALL --> SCHED
    SYSCALL --> IPC
    
    %% Distributed System
    DIST --> NET
    MIGRATION --> DIST
    MIGRATION --> SCHED
    MIGRATION --> MM
    
    %% Hardware Layer
    NET --> NET_HW
    SCHED --> TIMER
    MAIN --> UART
    SYSCALL --> TRAP
    
    %% Network Communication
    NET --> LIBNET
    
    classDef userspace fill:#e1f5fe
    classDef kernel fill:#f3e5f5
    classDef distributed fill:#e8f5e8
    classDef hardware fill:#fff3e0
    
    class APP1,APP2,APP3,LIBC,LIBHYDRA,LIBNET userspace
    class MAIN,SCHED,IPC,MM,SYSCALL kernel
    class DIST,MIGRATION,NET distributed
    class UART,TIMER,NET_HW,TRAP hardware
```

## Key Components

### Microkernel Core
- **Minimal kernel**: Only essential services in kernel space
- **Message-passing IPC**: All communication through messages
- **Preemptive scheduler**: Round-robin with context switching
- **Memory management**: Page-based allocation

### Distributed Layer
- **Node discovery**: Automatic detection of other kernel instances
- **Process migration**: Transparent process movement between nodes
- **Load balancing**: Intelligent workload distribution
- **Network services**: Built-in networking support

### Design Principles
1. **Separation of concerns**: Each component has clear responsibilities
2. **Modularity**: Components communicate only through well-defined interfaces
3. **Fault isolation**: User processes cannot crash the kernel
4. **Distributed transparency**: Applications unaware of node boundaries