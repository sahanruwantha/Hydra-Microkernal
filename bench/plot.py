#!/usr/bin/env python3
"""
Plot migration latency vs payload size for KERNEL and USERSPACE modes.
Produces build/figures/latency_vs_size.png — Chapter 5 Figure 1
candidate, the RISC-V 2025 analogue of Milojicic et al. (1999) Fig. 8.

Usage:
    bench/plot.py bench/results/KERNEL.log bench/results/USERSPACE.log
"""
from __future__ import annotations

import sys
import math
import statistics
from pathlib import Path

import matplotlib
matplotlib.use("Agg")  # headless
import matplotlib.pyplot as plt

# Reuse parse_log from analyze.py
sys.path.insert(0, str(Path(__file__).parent))
from analyze import parse_log, derive, ci95


def collect(runs):
    """Return {size_bytes: (mean_total_cycles, halfwidth)}."""
    sizes = sorted({r.get("__size") for r in runs if "__size" in r})
    out = {}
    for sz in sizes:
        bucket = [r for r in runs if r.get("__size") == sz]
        d = derive(bucket)
        mean, hw = ci95(d["total_latency"])
        if not math.isnan(mean):
            out[sz] = (mean, hw)
    return out


def main(argv):
    if len(argv) != 3:
        print("usage: plot.py KERNEL.log USERSPACE.log", file=sys.stderr)
        return 2

    kern = collect(parse_log(Path(argv[1])))
    user = collect(parse_log(Path(argv[2])))

    if not kern or not user:
        print("ERROR: no BENCH_SIZE-tagged data in one or both logs", file=sys.stderr)
        print("       (re-run the kernel after the payload-sweep changes)", file=sys.stderr)
        return 1

    fig, ax = plt.subplots(figsize=(7, 4.5), constrained_layout=True)

    for label, data, color, marker in (
        ("Kernel-resident", kern, "#c0392b", "s"),
        ("User-space",      user, "#2980b9", "o"),
    ):
        sizes = sorted(data.keys())
        means = [data[s][0] for s in sizes]
        hws   = [data[s][1] if not math.isnan(data[s][1]) else 0 for s in sizes]
        # Convert bytes to KB on x; cycles to millions on y
        x = [(s + 8192) / 1024 for s in sizes]  # +8KB stack always present
        y = [m / 1e6 for m in means]
        yerr = [h / 1e6 for h in hws]
        ax.errorbar(x, y, yerr=yerr, label=label, color=color, marker=marker,
                    markersize=7, capsize=4, linewidth=1.6)

    ax.set_xlabel("Process state size (KB, including 8 KB stack)")
    ax.set_ylabel("Total migration latency (Mcycles)")
    ax.set_title("Hydra RISC-V: migration latency vs. state size\n"
                 "(n per point per mode shown in bench/results/)")
    ax.grid(True, alpha=0.3)
    ax.legend()

    out_path = Path("build/figures/latency_vs_size.png")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=120)
    print(f"[plot.py] wrote {out_path}")

    # Per-mode mean across all sizes
    for label, data in (("Kernel-resident", kern), ("User-space", user)):
        all_means = [m for m, _ in data.values()]
        if all_means:
            print(f"  {label:>16}: {statistics.mean(all_means)/1e6:.2f} Mcycles avg across {len(all_means)} sizes")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
