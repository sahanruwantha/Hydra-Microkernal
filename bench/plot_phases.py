#!/usr/bin/env python3
"""
Stacked bar chart of migration phases.

For each payload size and each mode, show the breakdown of total
latency into (freeze, serialize, transport, deserialize). Helps the
reader see *where* time is going inside a migration rather than just
the totals.

Usage:
    bench/plot_phases.py bench/results/KERNEL.log bench/results/USERSPACE.log
"""
from __future__ import annotations

import sys
import statistics
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from analyze import parse_log, derive  # noqa: E402


PHASES = [
    ("freeze_time",   "Freeze",      "#e67e22"),
    ("serialize_t",   "Serialize",   "#2980b9"),
    ("transport_t",   "Transport",   "#27ae60"),
    ("deserialize_t", "Deserialize", "#8e44ad"),
]


def phases_per_size(runs):
    """Return {size: {phase: mean_cycles}}."""
    sizes = sorted({r.get("__size") for r in runs if "__size" in r})
    out = {}
    for sz in sizes:
        bucket = [r for r in runs if r.get("__size") == sz]
        d = derive(bucket)
        out[sz] = {phase: statistics.mean(d[phase]) if d[phase] else 0
                   for phase, *_ in PHASES}
    return out


def main(argv):
    if len(argv) != 3:
        print("usage: plot_phases.py KERNEL.log USERSPACE.log", file=sys.stderr)
        return 2

    kern = phases_per_size(parse_log(Path(argv[1])))
    user = phases_per_size(parse_log(Path(argv[2])))
    sizes = sorted(set(kern) | set(user))
    if not sizes:
        print("ERROR: no BENCH_SIZE data", file=sys.stderr)
        return 1

    fig, ax = plt.subplots(figsize=(8.5, 5), constrained_layout=True)
    bar_w = 0.4
    x = np.arange(len(sizes))

    for x_offset, label, data in (
        (-bar_w/2, "KERNEL",    kern),
        ( bar_w/2, "USERSPACE", user),
    ):
        bottoms = np.zeros(len(sizes))
        for phase, ph_label, color in PHASES:
            vals = np.array([data.get(sz, {}).get(phase, 0) for sz in sizes]) / 1e6
            ax.bar(x + x_offset, vals, width=bar_w, bottom=bottoms,
                   label=f"{ph_label} ({label})" if x_offset > 0 else ph_label,
                   color=color, edgecolor="white", linewidth=0.5,
                   alpha=0.95 if label == "USERSPACE" else 0.75)
            bottoms += vals

    ax.set_xticks(x)
    ax.set_xticklabels([f"{(sz+8192)/1024:.0f} KB" for sz in sizes])
    ax.set_xlabel("Process state size (KB, including 8 KB stack)")
    ax.set_ylabel("Cycles (millions)")
    ax.set_title("Hydra RISC-V: per-phase migration cost\n"
                 "(left bar = kernel-resident, right bar = user-space)")
    ax.grid(True, axis="y", alpha=0.3)
    # Custom legend with phase names only (mode shown by left/right bars)
    handles = [plt.Rectangle((0,0),1,1, color=c) for _, _, c in PHASES]
    ax.legend(handles, [p[1] for p in PHASES], loc="upper left",
              title="Phase", frameon=True)

    out = Path("build/figures/phase_breakdown.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, dpi=120)
    print(f"[plot_phases.py] wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
