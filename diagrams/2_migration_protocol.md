# Process Migration Protocol - 4-Phase Flow

## Overview
This diagram illustrates the complete 4-phase process migration protocol in Hydra, showing how processes are transparently moved between kernel nodes.

```mermaid
sequenceDiagram
    participant SRC as Source Node
    participant TGT as Target Node
    participant PROC as Process
    participant NET as Network
    
    Note over SRC, TGT: Phase 1: MIGRATION REQUEST
    
    SRC->>SRC: Detect migration trigger<br/>- High load<br/>- Better node available<br/>- Application request
    SRC->>SRC: Select target node<br/>find_best_migration_target()
    SRC->>PROC: Suspend process<br/>suspend_process_for_migration()
    PROC-->>SRC: Process suspended
    SRC->>SRC: Create checkpoint<br/>create_migration_checkpoint()
    SRC->>TGT: MIGRATION_MSG_REQUEST<br/>- process_id<br/>- estimated_size<br/>- process_type
    
    Note over SRC, TGT: Phase 2: ACCEPT/REJECT Decision
    
    TGT->>TGT: Check resources<br/>- Available memory<br/>- CPU capacity<br/>- Process slots
    TGT->>TGT: Evaluate capability<br/>- Node type match<br/>- Hardware requirements
    
    alt Resource Available
        TGT->>SRC: MIGRATION_MSG_ACCEPT<br/>- Confirmation<br/>- Allocated resources
        Note over SRC, TGT: Proceed to Phase 3
    else No Resources
        TGT->>SRC: MIGRATION_MSG_REJECT<br/>- Reason code<br/>- Alternative suggestions
        SRC->>PROC: Resume process
        Note over SRC, TGT: Migration aborted
    end
    
    Note over SRC, TGT: Phase 3: DATA TRANSFER
    
    SRC->>SRC: Serialize process state<br/>serialize_process()
    SRC->>SRC: Prepare data chunks<br/>- Process context<br/>- Memory state<br/>- Application data
    
    loop For each data chunk
        SRC->>NET: MIGRATION_MSG_DATA<br/>- chunk_index<br/>- total_chunks<br/>- data[1024]
        NET->>TGT: Forward chunk
        TGT->>TGT: Store chunk<br/>validate_chunk()
        TGT->>SRC: ACK chunk received
    end
    
    TGT->>TGT: Reassemble process<br/>deserialize_process()
    TGT->>TGT: Allocate resources<br/>- Memory pages<br/>- Task slot<br/>- Process context
    TGT->>TGT: Restore process state<br/>- Registers<br/>- Stack<br/>- Heap data
    
    Note over SRC, TGT: Phase 4: COMPLETION
    
    TGT->>PROC: Create migrated process<br/>create_migrated_task()
    PROC-->>TGT: Process ready
    TGT->>SRC: MIGRATION_MSG_COMPLETE<br/>- new_process_id<br/>- Success confirmation
    
    SRC->>SRC: Cleanup original process<br/>terminate_migrated_process()
    SRC->>SRC: Update distributed state<br/>- Remove from local registry<br/>- Notify other nodes
    TGT->>TGT: Register new process<br/>- Add to scheduler<br/>- Update load metrics
    TGT->>PROC: Resume execution<br/>resume_migrated_process()
    
    Note over PROC: Process continues execution<br/>transparently on target node
```

## Migration Decision Flow

```mermaid
flowchart TD
    START([Migration Trigger])
    
    CHECK_LOAD{Check Current<br/>Node Load}
    CHECK_NODES{Other Nodes<br/>Available?}
    EVAL_TARGET{Find Better<br/>Target Node?}
    CAN_MIGRATE{Process Can<br/>Migrate?}
    
    SUSPEND[Suspend Process]
    CHECKPOINT[Create Checkpoint]
    SEND_REQ[Send Migration Request]
    
    WAIT_RESP{Wait for<br/>Response}
    ACCEPTED{Migration<br/>Accepted?}
    
    TRANSFER[Transfer Process State]
    VERIFY{Transfer<br/>Successful?}
    CLEANUP[Cleanup Source]
    RESUME[Resume on Target]
    
    ABORT[Abort Migration]
    RESUME_LOCAL[Resume Locally]
    
    SUCCESS([Migration Complete])
    FAILURE([Migration Failed])
    
    START --> CHECK_LOAD
    CHECK_LOAD -->|Load > 80%| CHECK_NODES
    CHECK_LOAD -->|Load < 80%| FAILURE
    
    CHECK_NODES -->|Yes| EVAL_TARGET
    CHECK_NODES -->|No| FAILURE
    
    EVAL_TARGET -->|Better node found| CAN_MIGRATE
    EVAL_TARGET -->|No better node| FAILURE
    
    CAN_MIGRATE -->|Yes| SUSPEND
    CAN_MIGRATE -->|No| FAILURE
    
    SUSPEND --> CHECKPOINT
    CHECKPOINT --> SEND_REQ
    SEND_REQ --> WAIT_RESP
    
    WAIT_RESP --> ACCEPTED
    ACCEPTED -->|Yes| TRANSFER
    ACCEPTED -->|No| ABORT
    
    TRANSFER --> VERIFY
    VERIFY -->|Success| CLEANUP
    VERIFY -->|Failed| ABORT
    
    CLEANUP --> RESUME
    RESUME --> SUCCESS
    
    ABORT --> RESUME_LOCAL
    RESUME_LOCAL --> FAILURE
    
    classDef decision fill:#fff2cc
    classDef action fill:#d5e8d4
    classDef success fill:#c8e6c9
    classDef failure fill:#ffcdd2
    
    class CHECK_LOAD,CHECK_NODES,EVAL_TARGET,CAN_MIGRATE,WAIT_RESP,ACCEPTED,VERIFY decision
    class SUSPEND,CHECKPOINT,SEND_REQ,TRANSFER,CLEANUP,RESUME,ABORT,RESUME_LOCAL action
    class SUCCESS success
    class FAILURE failure
```

## Protocol Details

### Phase 1: Request
- **Trigger conditions**: High load (>80%), better node available, manual request
- **Target selection**: Based on node type, capabilities, and current load
- **Process preparation**: Suspend execution, create checkpoint

### Phase 2: Negotiation  
- **Resource check**: Memory, CPU, process slots
- **Capability match**: Hardware requirements, node specialization
- **Policy enforcement**: Load balancing rules, security constraints

### Phase 3: Transfer
- **State serialization**: Complete process context capture
- **Chunked transfer**: 1KB chunks with error detection
- **Progressive restore**: Incremental process reconstruction

### Phase 4: Completion
- **Process activation**: Resume execution on target
- **State synchronization**: Update distributed system state
- **Resource cleanup**: Free resources on source node

## Error Handling
- **Network failures**: Automatic retry with exponential backoff
- **Resource exhaustion**: Graceful rejection with alternatives
- **Corruption detection**: Checksums and validation at each step
- **Rollback capability**: Restore from checkpoint on failure