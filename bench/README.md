# Benchmarks and evidence

`results/` holds retained local and cross-node logs, their saved summaries,
and a separate record of later implementation validation. Keep this evidence
separate from new runs, which default to `build/bench/`.

## Analyse retained data

From the repository root:

```bash
python3 bench/analyze.py bench/results/KERNEL.log bench/results/USERSPACE.log
python3 bench/analyze_xmig.py bench/results/xmig-source.log bench/results/xmig-dest.log
python3 bench/analyze_micro.py bench/results/micro.log
```

The current cross-node analyser filters XMIG contexts and reports isolated
UXMIG paths separately. It excludes unrelated boot pings and local restores.
The old unfiltered `xmig-comparison.CONTAMINATED.txt` has been removed.

## Capture new data

```bash
make MIGRATION_MODE=USERSPACE
./bench/benchmark.sh USERSPACE 30
./bench/two_node_test.sh 60

make MICRO_BENCH=1 TRACE_IPC=0
./bench/micro_bench.sh 7
```

`benchmark.sh` captures BENCH and BENCH_SIZE lines from interactive repetitions. Its default
output is `build/bench/USERSPACE.log` or `build/bench/KERNEL.log`.
`micro_bench.sh` defaults to `build/bench/micro.log`. Two-node acceptance writes
`/tmp/hydra-node1.log` and `/tmp/hydra-node2.log` plus packet captures.
See [testing](../docs/testing.md) for a full tagged boot sweep.

## Diagnostic plots

```bash
.venv/bin/python bench/plot.py bench/results/KERNEL.log bench/results/USERSPACE.log
.venv/bin/python bench/plot_phases.py bench/results/KERNEL.log bench/results/USERSPACE.log
.venv/bin/python bench/fig_variance.py
```

Plots are regenerated under `build/figures/`. The exact figures in the current
dissertation are retained separately under `docs/report/figures/`.
Historical Amoeba source measurements and tools remain under [amoeba/](amoeba/RESULTS.md).

The original placement comparison is supervisor-mode, QEMU-specific, and uses
synthetic extra payload. The original 8/15 transport sample and later 15/15
acknowledged validation are different datasets; new runs do not overwrite
either automatically. See `results/improvement-validation.txt` for the later
functional validation, and the current report for the statistical limitations.
