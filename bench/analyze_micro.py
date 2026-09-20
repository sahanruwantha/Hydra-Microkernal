#!/usr/bin/env python3
"""
Parse BENCH_MICRO lines from bench/micro_bench.sh and report Hydra's
null-syscall and IPC round-trip costs, plus the ratio between them.

Why the ratio matters
---------------------
Chapter 5 compares Hydra against real Amoeba (FSD-Amoeba 2002A, booted
under qemu-system-i386). Absolute cycle counts are NOT comparable
between the two: different ISA, different emulated machine, different
QEMU translation cost. What *is* comparable is

    IPC round trip / null syscall

because both paths pay the same per-trap and per-emulated-instruction
overhead on the same host. Amoeba ships the matching pair of tests in
its own source tree:

    test/performance/syscall/sys_null.c   -> null syscall
    test/performance/rpc/rpc_client.c     -> RPC round trip

so the same ratio can be computed for Amoeba and set beside Hydra's.

Input format (one group per boot, tagged by run):

    BENCH_MICRO run=<n> null_syscall        <iters> <total_cycles>
    BENCH_MICRO run=<n> ipc_roundtrip       <iters> <total_cycles>
    BENCH_MICRO run=<n> null_syscall_per_op <cycles>
    BENCH_MICRO run=<n> ipc_roundtrip_per_op <cycles>
    BENCH_MICRO run=<n> ratio_x100          <ratio * 100>

Per-op values are recomputed here from the totals rather than trusted
from the kernel's integer division.

Usage:
    python3 bench/analyze_micro.py bench/results/micro.log
"""

from __future__ import annotations

import math
import statistics
import sys
from pathlib import Path


def parse(path: Path) -> dict[int, dict[str, tuple[int, int]]]:
    """Return {run: {metric: (iters, total_cycles)}}."""
    runs: dict[int, dict[str, tuple[int, int]]] = {}
    for line in path.read_text(errors="replace").splitlines():
        if not line.startswith("BENCH_MICRO"):
            continue
        parts = line.split()
        # BENCH_MICRO run=N <metric> <iters> <total>
        if len(parts) != 5 or not parts[1].startswith("run="):
            continue
        try:
            run = int(parts[1].split("=", 1)[1])
            iters = int(parts[3])
            total = int(parts[4])
        except ValueError:
            continue
        metric = parts[2]
        if metric not in ("null_syscall", "ipc_roundtrip"):
            continue
        runs.setdefault(run, {})[metric] = (iters, total)
    return runs


def ci95(values: list[float]) -> float:
    """Half-width of the 95% confidence interval of the mean."""
    n = len(values)
    if n < 2:
        return 0.0
    return 1.96 * statistics.stdev(values) / math.sqrt(n)


def summarise(name: str, values: list[float]) -> str:
    if not values:
        return f"{name:<24} (no samples)"
    mean = statistics.mean(values)
    return (
        f"{name:<24} {mean:12,.0f} ± {ci95(values):<10,.0f} "
        f"{statistics.median(values):12,.0f} {min(values):12,.0f} "
        f"{max(values):12,.0f}"
    )


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__)
        return 2

    path = Path(argv[1])
    if not path.is_file():
        print(f"ERROR: no such file: {path}", file=sys.stderr)
        return 1

    runs = parse(path)
    # Keep only boots that produced both measurements, so the per-run
    # ratio is always computed from a matched pair.
    complete = {
        r: m for r, m in runs.items()
        if "null_syscall" in m and "ipc_roundtrip" in m
    }
    dropped = len(runs) - len(complete)

    if not complete:
        print(f"ERROR: no complete runs found in {path}", file=sys.stderr)
        return 1

    null_per, ipc_per, ratios = [], [], []
    for m in complete.values():
        n_iters, n_total = m["null_syscall"]
        i_iters, i_total = m["ipc_roundtrip"]
        if n_iters <= 0 or i_iters <= 0:
            continue
        n = n_total / n_iters
        i = i_total / i_iters
        null_per.append(n)
        ipc_per.append(i)
        if n > 0:
            ratios.append(i / n)

    print(f"=== {path}  (n={len(complete)} boots"
          + (f", {dropped} incomplete dropped" if dropped else "") + ") ===")
    print(f"{'metric':<24} {'mean ± 95% CI':>24} {'p50':>12} "
          f"{'min':>12} {'max':>12}")
    print("-" * 88)
    print(summarise("null_syscall (cyc/op)", null_per))
    print(summarise("ipc_roundtrip (cyc/op)", ipc_per))
    print()
    if ratios:
        mean_ratio = statistics.mean(ratios)
        print(f"IPC round trip costs {mean_ratio:,.1f}x a null syscall "
              f"(± {ci95(ratios):,.1f}, n={len(ratios)})")
        print()
        print("Compare against real Amoeba's own sys_null / rpc_client pair.")
        print("Absolute cycles are NOT comparable across the two ISAs; the")
        print("ratio is the defensible cross-system number.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
