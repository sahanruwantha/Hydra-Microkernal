# What changed for this dissertation

Historical contribution log. Line counts and early design descriptions below
refer to their original snapshots. Current paths use `kernel/`; the latest
reviewed dissertation is [Final_Report.docx](report/Final_Report.docx).


This document distinguishes between code that was **inherited** from
the upstream Hydra teaching kernel and code that was **added or
materially modified** for this BSc dissertation project. The
distinction matters: the dissertation's contribution claims are
strictly against the new and modified work; the inherited code is
acknowledged scaffolding.

## New files (created for this project)

These files did not exist before the project began. Every line is
attributable to this dissertation.

### Kernel

| File | Lines | Purpose |
|------|-------|---------|
| `kernel/migration.c` | 250 | `sys_checkpoint` and `sys_restore` — the kernel migration mechanism. Real CPU register and 8 KB stack capture, sp-offset rebasing, magic/version validation. |
| `kernel/migration.h` | 64 | Public API for the above; checkpoint format definition. |
| `kernel/bench.h` | 69 | `BENCH_TS(label)` macro plus `bench_rdcycle()` inline assembly reading the RISC-V `cycle` CSR. |

### Applications and tests

| File | Lines | Purpose |
|------|-------|---------|
| `apps/migration_test.c` | 154 | Local checkpoint/restore round-trip with variable payload sweep (0/4K/16K/64K extra). |
| `apps/fault_tolerance_demo.c` | 142 | Kill-and-recover demonstration: same primitives subsume migration and fault recovery. |
| `apps/net_test.c` | ~100 | ARP probe (single-node) + Hydra-tagged two-node broadcast ping. |
| `apps/cross_node_migration.c` | ~210 | Fragment protocol over Ethernet (ethertype 0x88B5, magic `HYDM`). Role-based: MAC last byte 01 = source, 02 = destination. |

### Benchmark infrastructure

| File | Lines | Purpose |
|------|-------|---------|
| `bench/benchmark.sh` | 85 | QEMU runner that captures BENCH lines into a log. |
| `bench/two_node_test.sh` | ~55 | Dual-QEMU launcher with UDP multicast bridge and per-side pcap capture. |
| `bench/analyze.py` | 207 | Per-run grouping, mean ± 95% CI, p50/p95; per-payload-size buckets. |
| `bench/plot.py` | 89 | Latency-vs-state-size figure (Chapter 5 Figure 5.1). |
| `bench/plot_phases.py` | 96 | Stacked-bar phase breakdown (Chapter 5 Figure 5.2). |
| `bench/analyze_xmig.py` | ~80 | Cross-node BENCH analyser for two-node runs. |

### Dissertation documents

The active documents are indexed in [docs/README.md](README.md). Superseded
chapter drafts and one-off Word editing helpers were removed during the
14 October 2026 cleanup and are recoverable from the external backup.

**Total new code: ~1,900 lines of C/Python/Shell + ~2,000 lines of
dissertation prose.**

## Materially modified files

These files existed in the upstream Hydra kernel but received
substantive changes for this dissertation.

| File | Lines changed | What changed |
|------|---------------|--------------|
| `kernel/main.c` | ~40 | Mode banner on the boot screen; `c` and `r` UART commands; auto-trigger for the 60-run migration sweep + fault demo; `cross_node_migration()` invocation when MAC last byte is 01/02; net self-test call after net init. |
| `kernel/sched.c` | ~15 | **`create_task` slot-reuse fix.** Original implementation only ever appended to `tasks[num_tasks]`; we changed it to scan for `TASK_UNUSED` first. Without this fix the local benchmark exhausts the 16-slot task table after ~6 runs. |
| `kernel/syscall.c` | ~15 | Two new dispatch cases (`SYS_CHECKPOINT`, `SYS_RESTORE`); new include of `migration.h`. |
| `kernel/syscall.h` | ~5 | Two new syscall numbers (20, 21); two new user-wrapper declarations. |
| `kernel/printk.c` | ~25 | Added `%0Nx` / `%0Nd` zero-padded width support. Required because MAC addresses print as `52:54:00:12:34:56`-style hex. Pre-existing printk had only `%d/%x/%lx/%ld`. |
| `kernel/net_driver.c` | 444 / 376 | **Complete rewrite.** The original was VirtIO MMIO probing + stubs that only printk'd. The new driver implements VirtIO 1.1 modern MMIO transport: feature negotiation, virtqueue setup, TX/RX descriptor management, real packet send and receive. The original stub is retained in the external pre-cleanup backup. |
| `lib/libhydra/syscall_user.c` | ~10 | Two new wrappers `syscall_checkpoint` and `syscall_restore`. |
| `Makefile` | ~25 | `MIGRATION_MODE` build flag (KERNEL or USERSPACE); `-global virtio-mmio.force-legacy=false` added to all QEMU run targets; help text update. |

## Unmodified pre-existing files

Everything else in the repository was inherited as-is. The notable
inherited files (referred to in the chapter drafts) are listed for
completeness:

- `kernel/distributed.c`, `kernel/process_migration.c` — original
  migration-protocol skeleton with stub network sends. **The project
  does not depend on these for the new measurements.** The new
  measurements use `kernel/migration.c` (via the syscall dispatch and
  direct call) and `kernel/net_driver.c` (the rewrite). The old
  stubs remain in the build but are not exercised by any benchmark
  presented in Chapter 5.
- Before the post-dissertation hardening phases, `kernel/ipc.c`,
  `kernel/mm.c`, `kernel/timer.c`, and most of `kernel/printk.c` were inherited
  verbatim. Their later safety and preemption changes are listed below rather
  than being claimed as part of the original measured dissertation artifact.
- `boot/start.s`, `boot/linker.ld` — kept verbatim.
- `lib/libc/*`, `lib/libnet/*`, `lib/libhydra/*` (other than
  `syscall_user.c`) — kept verbatim.
- `apps/raytracer/*`, `apps/distributed_raytracer.c`,
  `apps/library_test.c`, etc. — kept verbatim; the dissertation does
  not require modifying them.

## Pre-existing issues found and disclosed

Two pre-existing Hydra issues that were not necessarily *bugs* in
the original teaching context but became bugs once the project's
benchmark harness exercised them at scale:

1. **`create_task` slot leak** (fixed; see `kernel/sched.c`). Without
   the fix the 60-run benchmark fails after the first ~6 iterations.
2. **`startup_counter` frozen** (fixed; see `kernel/main.c`). The
   counter was conditionally incremented only while the raytracer
   had not yet auto-started; once the raytracer started, the counter
   stopped advancing and the migration auto-trigger never fired.

Both fixes are credited to this project. The disclosure pattern in
the dissertation: name the issue, name the fix, attach a one-line
explanation in the relevant chapter in the retained report and the historical cleanup backup.

## Diff summary (what to check in a code-review pass)

If an examiner wants to verify the contribution by reading the diff
against upstream:

```bash
# Files entirely new (no upstream version):
ls kernel/migration.* kernel/bench.h \
   apps/migration_test.c apps/fault_tolerance_demo.c \
   apps/net_test.c apps/cross_node_migration.c \
   bench/

# Files materially modified (upstream version exists in git history):
git log -p -- kernel/main.c kernel/sched.c kernel/syscall.{c,h} \
                       kernel/printk.c kernel/net_driver.c \
                       lib/libhydra/syscall_user.c Makefile \
                       | head -100

# Older driver snapshots remain in Git history and the cleanup backup.
```

Original tracked code remains in Git history; the cleanup removed only
empty implementation files and superseded auxiliary material.

## Post-dissertation hardening and performance pass (2026-08-14)

The implementation was subsequently audited and upgraded without replacing
the dissertation's historical result files:

- Build-mode stamps prevent stale `KERNEL`/`USERSPACE` objects.
- ELF load segments are split into RX code and RW data; the RWX warning is gone.
- Scheduler PIDs are unique opaque identifiers and lifecycle calls resolve PIDs
  rather than confusing them with zero- or one-based task slots.
- New task stacks start at a 16-byte-aligned RISC-V ABI boundary.
- Restore validates the complete checkpoint layout before task allocation, and
  the boot test includes a malformed-record rejection case.
- `memcpy` and `memset` use aligned RISC-V word operations where safe, with a
  reproducible `MEMOPS_MODE=BYTE` baseline.
- The allocator rejects zero-size/overflow requests and invalid frees.
- VirtIO queue depth increased to 16, TX descriptors are not reused before
  completion, and malformed RX descriptor IDs are rejected.
- Cross-node migration now uses message IDs, duplicate tracking, ACKs, and up
  to four retries. Validation improved from the historical 8/15 to 15/15.
- Benchmark parsers exclude fault-demo timestamps from the 60-run local sample
  and report ACK-completion time separately from fragmentation time.

Measured validation is recorded in
`bench/results/improvement-validation.txt`.

## Preemptive scheduler and complete migration state (2026-08-14)

The next hardening phase replaced the cooperative-only execution core:

- Scheduler slot zero is permanently reserved for the boot/kernel-main
  context. The former first switch could overwrite the first server task's
  prepared context.
- New tasks enter through a trampoline and terminate cleanly if their entry
  function returns. Unused slots are recycled and READY is published only
  after the full context is initialized.
- A 100 Hz timer now performs round-robin preemption through a complete
  544-byte supervisor trap frame: every integer register, `sepc`, `sstatus`,
  `stval`, all 32 floating-point registers, and `fcsr`.
- The checkpoint format is version 2 and carries that complete trap frame,
  rather than only the cooperative ABI's callee-saved subset.
- A supervisor breakpoint provides an explicit yield gate until tasks move to
  U-mode, where a conventional `ecall` ABI can be used.
- IPC queues resolve opaque PIDs to scheduler slots, preserve the real sender
  PID, filter receives by sender, and are reclaimed when a task exits.
- The legacy checkpoint cache is keyed by opaque PID rather than rejecting all
  PIDs above the fixed task-slot count.
- `apps/scheduler_test.c` stress-tests 32 create/run/return/reuse cycles,
  preempts a task that never yields, and verifies temporary, argument, saved,
  and floating-point register sentinels across multiple timer interrupts.

Validation after this phase: scheduler test PASS, 60/60 local restores,
malformed record rejection PASS, fault recovery PASS, 15/15 acknowledged
cross-node restores, and zero unhandled or recursive traps.

## Sv39 isolation and concurrency foundations (2026-08-14)

- Added an 8 MiB, 2,048-page physical allocator with aligned contiguous
  allocation, validation, double-free/range checks, and exact reclamation.
- Enabled Sv39 with page-granular permissions: kernel text RX, kernel data and
  stacks RW/NX, and UART/VirtIO MMIO supervisor-only.
- Added per-task address-space roots and private two-page user stacks. Address
  spaces and page-table trees are reclaimed after isolated tasks exit.
- Trap entry now uses `sscratch` to switch U-mode traps onto a supervisor-only
  kernel stack before executing C code or changing `satp`.
- Added a real U-mode `ecall` path. The initial safe ABI exposes only
  pointer-free GET_PID, YIELD, and EXIT operations; pointer-bearing calls are
  rejected until copyin/copyout validation is implemented.
- Contained U-mode instruction/load/store page faults terminate only the
  offending task. The regression deliberately writes to the supervisor task
  table and verifies a hardware store-page fault.
- Added nest-safe interrupt critical sections and applied them to scheduler
  lifecycle publication, heap/page allocators, IPC queues, VirtIO rings,
  packet pools, UART input, and `printk` serialization.

The migration benchmark services remain in S-mode, so this phase did not alter
the dissertation's measured placement comparison.

## Validated U-mode data paths and checkpoint v3 (2026-08-14)

- Added overflow-safe, page-by-page user-range validation and copyin/copyout
  through physical aliases. U-mode IPC and network send/receive now use bounded
  kernel bounce buffers and reject invalid or permission-incompatible ranges.
- Added an end-to-end U-to-supervisor IPC regression that proves request data is
  copied in and reply data is copied back to a private user stack.
- Upgraded checkpoints to version 3. Isolated records include the complete trap
  frame, kernel stack, and private user stack; restore creates a fresh Sv39
  root, rebases both stack pointers, and publishes the task only after the
  address space is complete.
- Added a live U-mode checkpoint/terminate/restore regression. It verifies
  preserved execution state, a new destination PID, and exact page reclamation.
- Removed a test/setup publication race by supplying initial U-mode arguments
  before a task becomes READY; the dual-node regression had caught the attack
  task occasionally running before a post-creation register write.
- Added compiler-generated header dependencies so checkpoint layout changes
  cannot silently reuse stale objects from an earlier format.

Validation after this phase: U-mode IPC and isolated checkpoint/restore PASS,
hardware fault containment PASS, allocator reclamation PASS, 60/60 local
restores, and zero unhandled, recursive, or fatal traps in the clean local run.

## Complete current U-mode process images and checkpoint v4 (2026-08-14)

- Added two private RW/NX user data/heap pages to every isolated address space,
  separated from the stack by an unmapped guard page. Allocation, construction
  failures, deferred self-exit, and external termination reclaim the pages.
- Upgraded checkpoints to version 4 with an explicit user-data size and payload.
  A current isolated image is 25,168 bytes: 48-byte header, 544-byte trap frame,
  8 KiB kernel stack, 8 KiB user stack, and 8 KiB private data/heap.
- Restore rebuilds the private mapping and copies both data pages before making
  the task runnable. The regression places sentinels in the first and final
  words of the region and verifies them after migration.
- Added restore control-state validation. A U-mode image with an out-of-range
  resume PC, supervisor previous-privilege bit, or misaligned PC is rejected
  before any task or page allocation; privileged status bits come from the
  freshly constructed destination task rather than the serialized image.
- Fixed partial page-table construction cleanup so failed mapping operations do
  not leak intermediate table pages.
- Extended the two-node harness with a separate isolated migration after the 15
  historical supervisor runs. The source sends the 25,168-byte image in 18
  fragments; the destination ACKs only after restored U-mode code validates
  both private data-page sentinels and exits with its destination PID.
- Updated the cross-node analyzer to classify supervisor and isolated BENCH
  brackets separately instead of pooling different checkpoint sizes.

Validation after this phase: 60/60 local restores on both nodes, 15/15
supervisor cross-node restores, 1/1 isolated cross-node restore with data/heap
verification, 20/20 unique smoke frames on both nodes, fault recovery PASS,
and zero failure, fatal, timeout, unhandled-trap, or recursive-trap markers.
