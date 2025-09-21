amoeba/
├── kernel/             # Microkernel core
│   ├── main.c          # Boot + init
│   ├── sched.c         # Scheduler
│   ├── mm.c            # Memory manager
│   ├── ipc.c           # IPC primitives
│   ├── switch_riscv.s  # Context switching (new)
│   ├── trap_riscv.s    # Trap/interrupt handling (new)
│   └── net_driver.c    # Minimal NIC driver
│
├── server/
│   ├── bullet.c        # Process migration service
│   └── net_srv.c       # Network send/receive helpers
│
├── lib/
│   ├── libc/           # Minimal C stdlib
│   ├── libnet/         # Thin network wrapper over lwIP
│   └── libamoeba/      # Basic Amoeba APIs
│
├── apps/
│   └── raytracer/      # Your computation demo
│
└── lwip/               # Modern TCP/IP stack
