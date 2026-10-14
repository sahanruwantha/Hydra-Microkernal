# Build and functional checks

Run commands from the repository root unless stated otherwise. Requires the
RISC-V GCC toolchain and QEMU. Python analysis uses the standard library;
optional plots use matplotlib.

## Build both comparison paths

```bash
make MIGRATION_MODE=KERNEL
strings build/kernel.elf | grep 'Migration mode: KERNEL-RESIDENT'
make MIGRATION_MODE=USERSPACE
strings build/kernel.elf | grep 'Migration mode: USER-SPACE'
```

Switch without cleaning to check `build/.build-config` invalidates stale
objects. `make clean` removes the entire generated `build/` directory,
including new logs and plots. Retained data under `bench/results/` is preserved.

## Single-node boot and local tests

```bash
make run-net
```

Boot automatically checks scheduler lifecycle/preemption, full integer/FP
register preservation, page allocation, user-buffer validation, isolated
checkpoint/restore, and contained U-mode access faults. The subsequent local
sweep performs 15 restores at each of four synthetic extra-payload sizes,
followed by malformed-checkpoint rejection and fault recovery.

Expected markers include:

```text
[schedtest] PASS: 32 returns, busy-task preemption, integer+FP frame
[mmtest] PASS: aligned contiguous allocation and reclamation
[isotest] PASS: U-mode IPC, data/heap migration, W^X, safe restore, containment, reclamation
[migtest] PASS: restored task is running
[fdemo] PASS: workload recovered from simulated fault
```

Expect 60 local restore PASS lines in a complete boot. A contained U-mode
store page fault is an intentional negative test. `FAIL`, `FATAL`, unhandled
or recursive traps, allocation leaks, and failed restores require investigation.
Interactive `c` adds a local test; `r` repeats fault recovery; `d` shows status.
Exit QEMU with `Ctrl-a x`.

## Capture a full tagged local sweep

```bash
make MIGRATION_MODE=USERSPACE
mkdir -p build/bench
timeout 200s qemu-system-riscv64 -machine virt -nographic \
  -kernel build/kernel.elf -global virtio-mmio.force-legacy=false \
  -netdev user,id=net0 -device virtio-net-device,netdev=net0 \
  </dev/null >build/bench/USERSPACE-full.log 2>&1
tr -d '\r' <build/bench/USERSPACE-full.log \
  | grep -aE '^(BENCH |BENCH_SIZE )' >build/bench/USERSPACE.log
python3 bench/analyze.py build/bench/USERSPACE.log
```

The timeout ends a kernel that remains at its console, so exit status 124 is
expected when the cap is reached. Repeat with KERNEL mode and a different
output filename for a new comparison. Current source has later hardening;
new cycle counts do not reproduce the historical dissertation configuration.

## Two-node acceptance

```bash
./bench/two_node_test.sh 60
```

This launches two QEMUs over UDP multicast, caps the run, then audits:

- 20 peer frames received by each node;
- 60 local restores and one isolation PASS per node;
- 15 supervisor destination restores and one isolated destination restore;
- 16 source acknowledgements and no unexpected errors.

The script exits nonzero if a criterion fails. Increase the timeout on a
slower host. Logs and packet captures are written under `/tmp/hydra-node*`.

```bash
python3 bench/analyze_xmig.py /tmp/hydra-node1.log /tmp/hydra-node2.log
```

Do not overwrite the historical files under `bench/results/` with these runs.
The retained 8/15 sample used the original unacknowledged protocol; current
ACK/retry and isolated migration are separate functional checks.

## Other utilities

`scripts/test_distributed.sh` retains the older long-running node launcher;
`help` and `status` are read-only. It stores runtime state in `build/logs/`.
Use `scripts/log_manager.sh` for those runtime logs. Prefer the bounded
`bench/two_node_test.sh` for repeatable acceptance testing.
