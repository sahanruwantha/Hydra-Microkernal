# Distributed System Topology

## Overview
This diagram shows the distributed network topology of Hydra microkernel nodes, their specializations, and communication protocols.

```mermaid
graph TB
    subgraph "Hydra Distributed Cluster"
        subgraph "Master Node (ID: 0)"
            M1[Master Coordinator<br/>📊 Load Balancer<br/>🔍 Service Discovery<br/>📋 Resource Monitor]
            M1_SERVICES[Services:<br/>- Global State<br/>- Node Registry<br/>- Migration Coordination]
        end
        
        subgraph "Compute Nodes"
            C1[Compute Node 1<br/>💻 General Processing<br/>Load: 45%<br/>IP: 10.0.0.2]
            C2[Compute Node 2<br/>💻 CPU Intensive<br/>Load: 78%<br/>IP: 10.0.0.3]
            C3[Compute Node 3<br/>💻 Parallel Tasks<br/>Load: 23%<br/>IP: 10.0.0.4]
        end
        
        subgraph "Graphics Nodes"
            G1[Graphics Node 1<br/>🎮 GPU Rendering<br/>Load: 67%<br/>IP: 10.0.0.5]
            G2[Graphics Node 2<br/>🎨 Raytracing<br/>Load: 34%<br/>IP: 10.0.0.6]
        end
        
        subgraph "Storage Nodes"
            S1[Storage Node 1<br/>💾 Persistent Data<br/>Capacity: 85%<br/>IP: 10.0.0.7]
            S2[Storage Node 2<br/>💿 Backup Storage<br/>Capacity: 42%<br/>IP: 10.0.0.8]
        end
        
        subgraph "Gateway Node"
            GW1[Gateway Node<br/>🌐 External Comm<br/>📡 Protocol Bridge<br/>IP: 10.0.0.9]
        end
    end
    
    %% Heartbeat connections
    M1 -.->|Heartbeat 5s| C1
    M1 -.->|Heartbeat 5s| C2
    M1 -.->|Heartbeat 5s| C3
    M1 -.->|Heartbeat 5s| G1
    M1 -.->|Heartbeat 5s| G2
    M1 -.->|Heartbeat 5s| S1
    M1 -.->|Heartbeat 5s| S2
    M1 -.->|Heartbeat 5s| GW1
    
    %% Process migration flows
    C2 -->|Migrate Heavy Process| C3
    C1 -->|Migrate Raytracer| G2
    G1 -->|Offload General Task| C1
    
    %% Service communications
    C1 <-->|IPC Messages| S1
    G1 <-->|Data Access| S2
    GW1 <-->|External Requests| M1
    
    %% Network topology
    subgraph "Network Infrastructure"
        SWITCH[Ethernet Switch<br/>1 Gbps]
        ROUTER[Router/Firewall<br/>External Gateway]
    end
    
    C1 --- SWITCH
    C2 --- SWITCH
    C3 --- SWITCH
    G1 --- SWITCH
    G2 --- SWITCH
    S1 --- SWITCH
    S2 --- SWITCH
    M1 --- SWITCH
    GW1 --- SWITCH
    
    SWITCH --- ROUTER
    ROUTER --- INTERNET[Internet<br/>External Networks]
    
    classDef master fill:#ffebee,stroke:#f44336
    classDef compute fill:#e3f2fd,stroke:#2196f3
    classDef graphics fill:#f3e5f5,stroke:#9c27b0
    classDef storage fill:#e8f5e8,stroke:#4caf50
    classDef gateway fill:#fff3e0,stroke:#ff9800
    classDef network fill:#fafafa,stroke:#757575
    
    class M1,M1_SERVICES master
    class C1,C2,C3 compute
    class G1,G2 graphics
    class S1,S2 storage
    class GW1 gateway
    class SWITCH,ROUTER,INTERNET network
```

## Node Discovery Protocol

```mermaid
sequenceDiagram
    participant N1 as New Node
    participant BC as Broadcast
    participant M as Master Node
    participant EX as Existing Nodes
    
    Note over N1, EX: Node Bootstrap & Discovery
    
    N1->>BC: Discovery Request<br/>UDP Broadcast (Port 7777)
    Note right of N1: {<br/>  type: DISCOVERY_REQUEST,<br/>  node_id: 42,<br/>  node_type: COMPUTE,<br/>  capabilities: 0x01<br/>}
    
    M->>N1: Discovery Response<br/>Master Acknowledgment
    Note right of M: {<br/>  type: DISCOVERY_RESPONSE,<br/>  master_id: 0,<br/>  cluster_nodes: [...],<br/>  assigned_ip: 10.0.0.10<br/>}
    
    EX->>N1: Discovery Response<br/>Peer Announcements
    Note right of EX: Multiple responses from<br/>existing cluster members
    
    N1->>M: Registration Request<br/>Formal cluster join
    M->>N1: Registration Complete<br/>Welcome to cluster
    
    M->>EX: Node Announcement<br/>Broadcast new member
    Note over N1, EX: Node successfully joined cluster
    
    loop Every 5 seconds
        N1->>BC: Heartbeat Broadcast
        EX->>N1: Heartbeat Response
        Note over N1, EX: Maintain cluster membership
    end
```

## Heartbeat & Health Monitoring

```mermaid
stateDiagram-v2
    [*] --> Unknown
    Unknown --> Alive : First heartbeat received
    Alive --> Busy : Load > 80%
    Busy --> Alive : Load < 60%
    Alive --> Dead : No heartbeat > 15s
    Busy --> Dead : No heartbeat > 15s
    Dead --> Alive : Heartbeat restored
    Dead --> [*] : Remove from cluster
    
    state Alive {
        [*] --> Ready
        Ready --> Processing : Accept new tasks
        Processing --> Ready : Task completed
    }
    
    state Busy {
        [*] --> Overloaded
        Overloaded --> Rejecting : Reject new tasks
        Rejecting --> Overloaded : Still busy
    }
```

## Load Balancing Strategy

```mermaid
graph LR
    subgraph "Load Distribution Algorithm"
        START[New Process/Task]
        CLASSIFY[Classify Workload]
        
        subgraph "Workload Types"
            CPU[CPU Intensive]
            GPU[Graphics/Rendering]
            IO[I/O Heavy]
            NET[Network Processing]
        end
        
        subgraph "Node Selection"
            SELECT_COMPUTE[Select Compute Node<br/>Lowest CPU load]
            SELECT_GRAPHICS[Select Graphics Node<br/>GPU availability]
            SELECT_STORAGE[Select Storage Node<br/>I/O capacity]
            SELECT_GATEWAY[Select Gateway Node<br/>Network bandwidth]
        end
        
        START --> CLASSIFY
        CLASSIFY --> CPU
        CLASSIFY --> GPU
        CLASSIFY --> IO
        CLASSIFY --> NET
        
        CPU --> SELECT_COMPUTE
        GPU --> SELECT_GRAPHICS
        IO --> SELECT_STORAGE
        NET --> SELECT_GATEWAY
        
        SELECT_COMPUTE --> DEPLOY[Deploy to Selected Node]
        SELECT_GRAPHICS --> DEPLOY
        SELECT_STORAGE --> DEPLOY
        SELECT_GATEWAY --> DEPLOY
    end
    
    classDef workload fill:#e1f5fe
    classDef selection fill:#f3e5f5
    classDef process fill:#e8f5e8
    
    class CPU,GPU,IO,NET workload
    class SELECT_COMPUTE,SELECT_GRAPHICS,SELECT_STORAGE,SELECT_GATEWAY selection
    class START,CLASSIFY,DEPLOY process
```

## Network Communication Protocols

### Discovery Protocol (UDP Port 7777)
```c
typedef struct {
    uint32_t magic;           // 0xDEADBEEF
    uint8_t type;            // DISCOVERY_REQUEST/RESPONSE
    uint32_t node_id;        // Unique node identifier
    node_type_t node_type;   // Node specialization
    uint32_t capabilities;   // Capability flags
    uint32_t ip_addr;        // Node IP address
    char node_name[32];      // Human-readable name
} discovery_packet_t;
```

### Migration Protocol (TCP Port 7778)
```c
typedef struct {
    migration_msg_type_t type;  // Message type
    uint32_t process_id;        // Process identifier
    uint32_t data_size;         // Payload size
    uint32_t sequence;          // Message sequence
    uint32_t total_chunks;      // Total data chunks
    uint32_t chunk_index;       // Current chunk index
    uint8_t data[1024];         // Payload data
} migration_message_t;
```

### Heartbeat Protocol (UDP Port 7779)
```c
typedef struct {
    uint32_t node_id;         // Source node ID
    uint64_t timestamp;       // Current timestamp
    uint32_t load_percent;    // Current load (0-100)
    uint32_t memory_used;     // Memory usage
    uint32_t active_processes;// Process count
    node_status_t status;     // Node status
} heartbeat_packet_t;
```

## Fault Tolerance Features

1. **Node Failure Detection**
   - Heartbeat timeout (15 seconds)
   - Automatic cluster reorganization
   - Process migration from failed nodes

2. **Network Partition Handling**
   - Split-brain prevention
   - Master election protocol
   - Graceful degradation

3. **Load Balancing**
   - Dynamic process redistribution  
   - Automatic load shedding
   - Performance-based routing

4. **Service Recovery**
   - Process checkpoint/restart
   - State replication
   - Transparent failover