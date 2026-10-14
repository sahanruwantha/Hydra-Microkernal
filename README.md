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
checkpoint/restore test, `r` runs fault recovery, and `d` shows node status.
Boot also runs the scheduler, isolation, and 60-run local migration tests.

```bash
./bench/two_node_test.sh 60
```

The two-node acceptance test requires 60 local restores on each node,
15 supervisor migrations, one isolated migration, 16 acknowledgements,
and no unexpected failures. See [testing instructions](docs/testing.md).

## Repository layout

```text
apps/          workloads, demonstrations, and in-kernel regression tests
boot/          boot assembly and linker script
kernel/        scheduler, memory, IPC, migration, drivers, and traps
lib/           freestanding C, Hydra, and networking libraries
server/        Bullet and network services
bench/         benchmark runners, analysis tools, and retained evidence
scripts/       distributed-run, log, and paper-building utilities
docs/          current report and figures, proposal, paper, and guides
build/         generated executable, objects, new logs, and plots (ignored)
```

## Documents and evidence

- [Current dissertation — Word](docs/report/Final_Report.docx)
- [Current dissertation — PDF](docs/report/Final_Report.pdf)
- [Report figures and revision notes](docs/report/README.md)
- [Document index](docs/README.md)
- [Saved benchmark evidence and analysis](bench/README.md)
- [Implementation history](docs/changes.md)
- [Cleanup record](docs/cleanup.md)

The current dissertation includes the supervisor feedback revision.
The proposal and conference paper are earlier, separate documents; they have
not been brought into line with that revision.

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

The report discloses an unresolved historical checkpoint-size discrepancy:
8,340 bytes claimed versus 8,344 bytes in the illustrated layout. Current
version-4 checkpoint sizes do not resolve the earlier format discrepancy.

## Optional document and plotting tools

```bash
python3 -m venv .venv
.venv/bin/pip install matplotlib python-docx
```

Benchmark analysers use the Python standard library. Figure tools require
matplotlib; paper building requires python-docx. Kernel builds have no Python
or external runtime-library dependency.
