typedef enum {
    RUNNING,
    READY,
    BLOCKED,
    TERMINATED
} process_state_t;

typedef struct {
    int pid;
    process_state_t state;
    int priority;
    int context;
} pcb_t;
