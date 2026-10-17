# Hydra

A RISC-V teaching microkernel and process-migration research artefact.
The current implementation includes a preemptive scheduler, Sv39 isolation,
validated user-buffer access, checkpoint/restore, and acknowledged cross-node
transfer over VirtIO-Net.

## Build and run

Requires `riscv64-unknown-elf-gcc`, GNU Make, and `qemu-system-riscv64`.

```bash
make                            # default USERSPACE comparison label
make run-net                    # boot with VirtIO networking
make MIGRATION_MODE=KERNEL      # switch comparison path; objects rebuild
make clean                      # remove generated build/ files
```

The executable is `build/kernel.elf`. At the console, `c` runs a local
checkpoint/restore test, `r` runs fault recovery, and `d` shows test/network status.
Boot also runs the scheduler, isolation, and 60-run local migration tests.

```bash
./bench/two_node_test.sh 60
```

The two-node acceptance test requires 60 local restores on each node,
15 supervisor migrations, one isolated migration, 16 acknowledgements,
and no unexpected failures. It exits nonzero if a check fails. Exit an
interactive QEMU session with **Ctrl+A, then X**.

For a presentation rehearsal with saved logs and objective-by-objective
evidence, run `./scripts/presentation_demo.sh`. See the
[presentation demo guide](docs/demo.md) for commands and speaking points.

## Repository layout

```text
apps/          workloads, demonstrations, and in-kernel regression tests
boot/          boot assembly and linker script
kernel/        scheduler, memory, IPC, migration, drivers, and traps
lib/           checkpoint/restore wrappers and memory-copy primitives
bench/         benchmark runners, analysis tools, and retained evidence
scripts/       presentation rehearsal
docs/          final report, presentation, figures, and research reference
build/         generated executable, objects, new logs, and plots (ignored)
```

## Documents and evidence

- [Current dissertation — Word](docs/report/Final_Report.docx)
- [Current dissertation — PDF](docs/report/Final_Report.pdf)
- [Report figures](docs/report/figures/)
- [Presentation — PowerPoint](docs/presentation/Hydra_Research_Summary.pptx)
- [Presentation — PDF](docs/presentation/Hydra_Research_Summary.pdf)
- [Demo video, terminal images and capture evidence](docs/presentation/README.md)
- [Process migration survey](docs/references/Process_Migration_Survey.pdf)
- [Saved benchmark evidence and analysis](bench/README.md)

The current dissertation includes the supervisor feedback revision.

## Interpreting the measurements

The historical local comparison contains 120 observations: 15 repetitions
at four synthetic payload sizes in each of two modes. Both measured paths
run in supervisor mode and use the same checkpoint primitives; the USERSPACE
label denotes a wrapper/dispatch path, not a measured hardware privilege
transition. Similar sample means do not establish architectural equivalence.

The original cross-node sample completed 8 of 15 attempts under best-effort
UDP multicast without acknowledgement/retry. Later acknowledged transfer
and isolated-process validation are separate engineering results. All cycle
counts depend on the QEMU and host configuration.

Saved timings and terminal recordings describe their captured revisions.
New runs validate the current implementation and may have different startup
output, task IDs and timings. The recording archives retain their original
logs and source identity.

The report discloses an unresolved historical checkpoint-size discrepancy:
8,340 bytes claimed versus 8,344 bytes in the illustrated layout. Current
version-4 checkpoint sizes do not resolve the earlier format discrepancy.

## Optional plotting tools

```bash
python3 -m venv .venv
.venv/bin/pip install matplotlib
```

Benchmark analysers use the Python standard library. Figure tools require
matplotlib. Kernel builds have no Python or external runtime-library dependency.
