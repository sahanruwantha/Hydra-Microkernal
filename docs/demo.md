# Presentation demo

Use a short functional demonstration plus the saved research comparison.
Each claim should point to a command, log message, or cited discussion.

For a short presentation, use the [two prepared terminal evidence images](presentation/README.md)
alongside the call-path diagram and performance chart. Their complete command
transcripts and raw-log provenance are included.

A [36-second recorded QEMU terminal demonstration](presentation/hydra_qemu_checkpoint_recovery.mp4)
shows boot, local checkpoint/restore and task-failure recovery. It is an edited
replay of actual UART output with reading pauses; the original timed recording
is retained in its evidence archive. Use it when there is no time for a live run.

## Objectives and evidence

| Objective from Section 1.3 | What to show |
|---|---|
| 1. Implement and compare kernel-direct and syscall-wrapper paths | Both build banners, the direct/wrapper calls, and shared checkpoint primitives |
| 2. Measure intervals and added kernel code; document the calls | BENCH timestamps, phase analysis, source calls and checkpoint line counts; the report documents added versus inherited code |
| 3. Compare performance and kernel requirements | Saved 60-per-mode comparison and the shared checkpoint/restore implementation |
| 4. Reassess the 1993 findings and 1999 adoption barriers | Bounded observations interpreted alongside the cited literature; this needs discussion, not a terminal PASS |
| 5. Provide open source and reproducible benchmarks | Repository, commit identity, runnable commands, raw logs and analysis tools |

## Rehearse and retain logs

From the repository root:

```bash
./scripts/presentation_demo.sh
```

The runner builds KERNEL then USERSPACE, checks both banners, runs the existing
two-node acceptance test for 60 seconds, and prints selected real evidence.
It leaves USERSPACE built. Use `./scripts/presentation_demo.sh 90` on a slower
host. It requires the normal RISC-V compiler, QEMU, Make, Python 3 and standard
Linux tools; plotting packages are not required.

Every rehearsal gets a separate `build/demo/run-XXXXXX/` directory; its actual
path appears at startup. This contains raw UART logs, readable log copies,
packet captures, build logs, kernel hash, source revision, working-tree status,
QEMU version, and current/historical analyses. Historical files under
`bench/results/` remain unchanged. `make clean` removes demo folders, so copy
evidence you need to retain first.

The runner checks 60 local restores and one isolation PASS on each node,
20 peer frames per node, 15 supervisor migrations, one isolated migration,
16 acknowledgements and zero unexpected errors. It also requires scheduler,
allocator, malformed-checkpoint rejection and fault-recovery PASS markers on
both nodes. A failed test or missing marker exits nonzero.

## Presentation sequence

### 1. Two selectable paths — about 30 seconds

```bash
make MIGRATION_MODE=KERNEL
strings build/kernel.elf | grep 'Migration mode:'
make MIGRATION_MODE=USERSPACE
strings build/kernel.elf | grep 'Migration mode:'
grep -nE 'long (n|new_pid) = (migration_|syscall_)(checkpoint|restore)' apps/migration_test.c
```

Say: "I implemented a direct path and a wrapper/dispatch path to the same
checkpoint and restore primitives. Both paths in the historical timing
comparison execute in supervisor mode."

### 2. Interactive checkpoint and recovery — about one minute

```bash
make run-net
```

Let the automatic migration sweep and fault demonstration finish. Press `c`
for an additional checkpoint/restore. Show the checkpoint size, new PID,
counter delta and `[migtest] PASS: restored task is running`.

Press `r` to repeat fault recovery. Show checkpoint capture, the simulated
task kill, the frozen counter, restore and resumed counter. This demonstrates
controlled task-failure recovery; it does not demonstrate host-crash recovery.
Exit QEMU with **Ctrl+A, then X** before the two-node test.

The current console has three commands: `c` for checkpoint/restore, `r` for
fault recovery, and `d` for test/network status. The older discovery and
handshake-only commands were removed with the teaching demos. Use the
two-node acceptance test to demonstrate actual cross-node state transfer.

### 3. Transfer between two instances — about one minute

```bash
./scripts/presentation_demo.sh
```

During the 60-second run, explain that two QEMU instances exchange Ethernet
frames through VirtIO-Net over a host multicast connection. Afterwards show:

```text
supervisor=15/15 isolated=1/1 acks=16/16 errors=0
[two_node] PASS: all acceptance criteria met
[xmig] DESTINATION U: PASS — new pid=... data/heap intact
```

Say: "The current acknowledged protocol completed 15 supervisor migrations.
A separate isolated task was restored and checked its private data."
The isolated sample is functional evidence, not a statistical performance
estimate. Fifteen successes do not establish production reliability.

For a live destination view in a second terminal, replace `run-XXXXXX` with
the actual directory printed by the runner:

```bash
tail -f build/demo/run-XXXXXX/hydra-node2.log
```

### 4. Explain the measurements — about one minute

```bash
python3 bench/analyze.py bench/results/KERNEL.log bench/results/USERSPACE.log
wc -l kernel/migration.c kernel/migration.h
```

Point to 60 observations per mode, 15 repetitions at each of four synthetic
extra-payload sizes, phase means, confidence intervals and pooled totals of
4,061,687 and 4,061,801 cycles. Say: "The sampled means are similar under the
measured conditions. Separate confidence intervals do not prove equivalence."

The source line count covers the current checkpoint files, not the entire
trusted kernel or its security. Use the report's historical contribution and
line-count discussion for the Mach/Amoeba comparison.

The run folder's `current-local-analysis.txt` and
`current-cross-node-analysis.txt` describe the current implementation. They
do not replace the dissertation's historical dataset.

### 5. Interpretation and reproducibility — about one minute

Show [report Figure 6.1](report/figures/figure-6-1.png) and
[Figure 6.2](report/figures/figure-6-2.png), explain the context-dependent adoption
barriers, and connect industry changes to the need for new empirical
measurements. The terminal demo does not establish that the barriers vanished.

```bash
git rev-parse --short HEAD
git remote -v
```

Show the repository and the run's `commit.txt`, `kernel-sha256.txt`,
`qemu-version.txt`, `build-config.txt` and raw logs. A nonempty
`working-tree.txt` records uncommitted differences that must accompany that
revision. These records identify the run; they do not guarantee identical
host-sensitive cycle counts.

## Final-slide limitations

- Historical comparison: supervisor-mode invocation paths; no measured
  hardware-enforced user/kernel transition.
- Original transport sample: 8/15 without ACK/retry. Current 15/15 validation
  is separate.
- QEMU cycle counts depend on the platform and host. The original extra
  payload was synthetic; later isolated data/heap support is a separate result.
- The historical four-byte checkpoint-size discrepancy remains disclosed
  in Section 5.2.2; current record sizes do not resolve the older format.
