#!/usr/bin/env python3
"""Figure B: latency distribution comparison (variance / tail behaviour).

Uses the actual KERNEL.log / USERSPACE.log measurement data — no
synthetic numbers. Each box shows the full quartile + whisker
distribution of total migration latency for one mode × payload-size
bucket, computed from 15 runs per bucket × 4 sizes × 2 modes = 120
real measurements.
"""
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Reuse the parser from analyze.py
sys.path.insert(0, str(Path(__file__).parent))
from analyze import parse_log, derive   # noqa: E402


def latencies_by_size(path):
    """Return dict {size_bytes: [total_latency_per_run]}."""
    runs = parse_log(path)
    out = {}
    for r in runs:
        sz = r.get("__size")
        if sz is None or "ckpt_enter" not in r or "rest_done" not in r:
            continue
        out.setdefault(sz, []).append(r["rest_done"] - r["ckpt_enter"])
    return out


kern = latencies_by_size(Path("bench/results/KERNEL.log"))
user = latencies_by_size(Path("bench/results/USERSPACE.log"))
sizes = sorted(set(kern) | set(user))

if not sizes:
    raise SystemExit("No tagged data in logs — re-run benchmark.sh first.")

fig, ax = plt.subplots(figsize=(11, 5.5), constrained_layout=True)

# Build matching box pairs at each size
positions_k = []
positions_u = []
data_k = []
data_u = []
labels = []
for i, sz in enumerate(sizes):
    positions_k.append(i * 3.0 + 0.7)
    positions_u.append(i * 3.0 + 1.7)
    data_k.append([v / 1e6 for v in kern.get(sz, [])])
    data_u.append([v / 1e6 for v in user.get(sz, [])])
    labels.append(f"{(sz + 8192) // 1024} KB")

bp_k = ax.boxplot(data_k, positions=positions_k, widths=0.8, patch_artist=True,
                  boxprops=dict(facecolor="#c0392b", alpha=0.7, linewidth=1.0),
                  medianprops=dict(color="white", linewidth=1.8),
                  whiskerprops=dict(color="#c0392b"),
                  capprops=dict(color="#c0392b"),
                  flierprops=dict(marker="o", markersize=4,
                                  markerfacecolor="#c0392b",
                                  markeredgecolor="white", linewidth=0.4))
bp_u = ax.boxplot(data_u, positions=positions_u, widths=0.8, patch_artist=True,
                  boxprops=dict(facecolor="#2980b9", alpha=0.7, linewidth=1.0),
                  medianprops=dict(color="white", linewidth=1.8),
                  whiskerprops=dict(color="#2980b9"),
                  capprops=dict(color="#2980b9"),
                  flierprops=dict(marker="o", markersize=4,
                                  markerfacecolor="#2980b9",
                                  markeredgecolor="white", linewidth=0.4))

# X axis labels (one per size, centered between the K/U pair)
ax.set_xticks([(positions_k[i] + positions_u[i]) / 2 for i in range(len(sizes))])
ax.set_xticklabels(labels)
ax.set_xlabel("Process state size (KB, including 8 KB stack)")
ax.set_ylabel("Total migration latency (Mcycles)")
ax.set_title("Latency distribution by mode and payload size (n = 15 per box)",
             fontsize=12.5, weight="bold", pad=8)
ax.grid(True, axis="y", alpha=0.3)
ax.spines["top"].set_visible(False)
ax.spines["right"].set_visible(False)

# Per-box n labels along the top
ymax = max(max(d) if d else 0 for d in (*data_k, *data_u)) * 1.05
for i in range(len(sizes)):
    nk = len(data_k[i]); nu = len(data_u[i])
    ax.text(positions_k[i], ymax, f"n={nk}",
            ha="center", fontsize=7.5, color="#c0392b")
    ax.text(positions_u[i], ymax, f"n={nu}",
            ha="center", fontsize=7.5, color="#2980b9")
ax.set_ylim(top=ymax * 1.05)

# Legend
from matplotlib.patches import Patch
ax.legend(handles=[
    Patch(facecolor="#c0392b", alpha=0.7, label="KERNEL-resident"),
    Patch(facecolor="#2980b9", alpha=0.7, label="USER-SPACE"),
], loc="upper left", fontsize=9, framealpha=0.95)

out = Path("build/figures/fig_variance.png")
out.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(out, dpi=160)
print(f"wrote {out}")
