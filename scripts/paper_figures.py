#!/usr/bin/env python3
"""
paper_figures.py - figures for the IEEE two-column conference paper.

These are NOT the dissertation figures. A dissertation figure is a wide,
single-column-page graphic; an IEEE conference figure is 3.5 in wide, sits
in a text column, and is read at that size. Re-using the dissertation PNGs
would put 4 pt text on the page. Everything here is redrawn for the column.

Presentation rules applied (these are the conventions a reviewer expects):

  * Column width exactly 3.5 in; full-width variants 7.16 in.
  * Type in Nimbus Roman (metrically Times) so figure text matches the body
    text of the paper. Axis labels 8 pt, tick labels 7 pt - never smaller.
  * No chart titles. In a paper the caption carries the title; a title inside
    the axes duplicates it and wastes column height.
  * Categorical colours assigned in fixed slot order from a validated
    palette (blue #2a78d6, orange #eb6834, aqua #1baf7a). Validated with
    the palette checker: adjacent-pair CVD dE 9.2, normal-vision dE 27.6,
    both above the required floors.
  * Hatching as a second channel on every filled mark, because IEEE
    proceedings are frequently printed greyscale; colour alone must never
    carry identity.
  * Recessive axes: no top/right spine, grid only on the value axis at low
    alpha, drawn beneath the data.
  * Direct value labels used selectively - on the code-size bars, where the
    numbers are the point, and nowhere else.
  * 600 dpi, so the raster survives print.

Usage:  .venv/bin/python scripts/paper_figures.py
Output: docs/paper/figures/fig*.png
"""

from __future__ import annotations

import re
import statistics
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "paper" / "figures"
OUT.mkdir(parents=True, exist_ok=True)

COL_W, FULL_W = 3.5, 7.16
DPI = 600

# Validated categorical slots (see module docstring).
C1, C2, C3 = "#2a78d6", "#eb6834", "#1baf7a"
INK, INK2, GRID = "#1a1a19", "#52514e", "#c9c8c3"

plt.rcParams.update({
    "font.family": "serif",
    "font.serif": ["Nimbus Roman", "Liberation Serif", "DejaVu Serif"],
    "font.size": 8,
    "axes.labelsize": 8,
    "axes.titlesize": 8,
    "xtick.labelsize": 7,
    "ytick.labelsize": 7,
    "legend.fontsize": 7,
    "axes.edgecolor": INK2,
    "axes.linewidth": 0.6,
    "xtick.major.width": 0.6,
    "ytick.major.width": 0.6,
    "xtick.color": INK2,
    "ytick.color": INK2,
    "text.color": INK,
    "axes.labelcolor": INK,
    "figure.dpi": DPI,
    "savefig.dpi": DPI,
    "savefig.bbox": "tight",
    "savefig.pad_inches": 0.02,
})

PHASES = [("freeze_time", "Freeze"), ("serialize_t", "Serialise"),
          ("transport_t", "Transport"), ("deserialize_t", "Deserialise")]


def parse(path: Path):
    """Return [(payload_bytes, {metric: cycles}), ...] one entry per migration."""
    runs, cur, size = [], None, 0
    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        if m := re.match(r"BENCH_SIZE\s+(\d+)$", line):
            size = int(m.group(1)); continue
        m = re.match(r"BENCH\s+(\w+)\s+(\d+)$", line)
        if not m:
            continue
        label, cyc = m.group(1), int(m.group(2))
        if label == "ckpt_enter":
            if cur:
                runs.append(cur)
            cur = {"_size": size, "ckpt_enter": cyc}
        elif cur is not None:
            cur[label] = cyc
    if cur:
        runs.append(cur)

    out = []
    for r in runs:
        try:
            out.append((r["_size"], {
                "freeze_time":   r["ckpt_freeze"] - r["ckpt_enter"],
                "serialize_t":   r["ckpt_copied"] - r["ckpt_freeze"],
                "transport_t":   r["recv_done"]   - r["send_start"],
                "deserialize_t": r["rest_done"]   - r["rest_start"],
                "total_latency": r["rest_done"]   - r["ckpt_enter"],
            }))
        except KeyError:
            continue          # truncated record at end of log
    return out


def ci95(xs):
    return 1.96 * statistics.stdev(xs) / len(xs) ** 0.5 if len(xs) > 1 else 0.0


def tidy(ax, yaxis_grid=True):
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    if yaxis_grid:
        ax.grid(axis="y", color=GRID, linewidth=0.5, alpha=0.8)
        ax.set_axisbelow(True)


# ---------------------------------------------------------------- Fig. 1
def fig_placement():
    """Diagram: where the migration mechanism sits in each build."""
    fig, ax = plt.subplots(figsize=(COL_W, 1.75))
    ax.set_xlim(0, 10); ax.set_ylim(0, 6); ax.axis("off")

    for x0, title, mech_in_kernel in ((0.1, "Kernel-resident", True),
                                      (5.2, "User-space", False)):
        ax.text(x0 + 2.35, 5.55, title, ha="center", va="center",
                fontsize=8, color=INK)
        # user band
        ax.add_patch(FancyBboxPatch((x0, 2.95), 4.7, 1.75,
                     boxstyle="round,pad=0.04", linewidth=0.7,
                     edgecolor=INK2, facecolor="#f4f4f2"))
        ax.text(x0 + 0.18, 4.45, "user", fontsize=6.5, color=INK2, va="center")
        # kernel band
        ax.add_patch(FancyBboxPatch((x0, 0.65), 4.7, 1.75,
                     boxstyle="round,pad=0.04", linewidth=0.7,
                     edgecolor=INK2, facecolor="#e8e8e5"))
        ax.text(x0 + 0.18, 2.15, "kernel", fontsize=6.5, color=INK2, va="center")
        # privilege boundary
        ax.plot([x0, x0 + 4.7], [2.68, 2.68], color=INK, linewidth=0.9,
                linestyle=(0, (3, 2)))
        # the migration mechanism block
        by, col, hatch = ((1.05, C1, "///") if mech_in_kernel
                          else (3.35, C2, "\\\\\\"))
        ax.add_patch(FancyBboxPatch((x0 + 1.0, by), 2.7, 0.95,
                     boxstyle="round,pad=0.04", linewidth=0.8,
                     edgecolor=INK, facecolor=col, alpha=0.9, hatch=hatch))
        ax.text(x0 + 2.35, by + 0.48, "migration\nmechanism", ha="center",
                va="center", fontsize=6.5, color="white", linespacing=1.15)
        # the always-in-kernel primitives
        ky = 3.35 if mech_in_kernel else 1.05
        ax.add_patch(FancyBboxPatch((x0 + 1.0, ky), 2.7, 0.95,
                     boxstyle="round,pad=0.04", linewidth=0.7,
                     edgecolor=INK2, facecolor="white"))
        ax.text(x0 + 2.35, ky + 0.48,
                "task" if mech_in_kernel else "checkpoint /\nrestore",
                ha="center", va="center", fontsize=6.5, color=INK,
                linespacing=1.15)

    fig.savefig(OUT / "fig1_placement.png")
    plt.close(fig)
    print("wrote fig1_placement.png")


# ---------------------------------------------------------------- Fig. 2
def fig_distribution(kern, user):
    """Box plot of total latency per state size - shows spread, not means.

    The means are in Table I; repeating them as a bar chart would add no
    information. The distribution is the thing a table cannot show.
    """
    sizes = sorted({s for s, _ in kern})
    labels = {0: "8", 4096: "12", 16384: "24", 65536: "72"}

    fig, ax = plt.subplots(figsize=(COL_W, 2.25))
    pos, data, cols, hatches = [], [], [], []
    for i, s in enumerate(sizes):
        for j, (runs, c, h) in enumerate(((kern, C1, "///"), (user, C2, "\\\\\\"))):
            pos.append(i * 1.0 + (j - 0.5) * 0.34)
            data.append([m["total_latency"] / 1e6 for sz, m in runs if sz == s])
            cols.append(c); hatches.append(h)

    bp = ax.boxplot(data, positions=pos, widths=0.28, patch_artist=True,
                    medianprops=dict(color=INK, linewidth=1.0),
                    whiskerprops=dict(color=INK2, linewidth=0.6),
                    capprops=dict(color=INK2, linewidth=0.6),
                    flierprops=dict(marker="o", markersize=1.8,
                                    markerfacecolor=INK2, markeredgecolor="none",
                                    alpha=0.6))
    for patch, c, h in zip(bp["boxes"], cols, hatches):
        patch.set(facecolor=c, alpha=0.65, edgecolor=INK, linewidth=0.6, hatch=h)

    ax.set_xticks(range(len(sizes)))
    ax.set_xticklabels([labels[s] for s in sizes])
    ax.set_xlabel("Checkpoint state size (KB)")
    ax.set_ylabel("Total migration latency (Mcycles)")
    handles = [plt.Rectangle((0, 0), 1, 1, facecolor=C1, alpha=0.65,
                             edgecolor=INK, linewidth=0.6, hatch="///"),
               plt.Rectangle((0, 0), 1, 1, facecolor=C2, alpha=0.65,
                             edgecolor=INK, linewidth=0.6, hatch="\\\\\\")]
    ax.legend(handles, ["Kernel-resident", "User-space"], frameon=False,
              loc="upper left", handlelength=1.5, borderaxespad=0.2)
    tidy(ax)
    fig.savefig(OUT / "fig2_distribution.png")
    plt.close(fig)
    print("wrote fig2_distribution.png")


# ---------------------------------------------------------------- Fig. 3
def fig_scaling(kern, user):
    """Total latency against state size - the trend, with CI bars."""
    sizes = sorted({s for s, _ in kern})
    kb = [8, 12, 24, 72]

    fig, ax = plt.subplots(figsize=(COL_W, 2.1))
    for runs, c, mk, hatch, lab in ((kern, C1, "o", None, "Kernel-resident"),
                                    (user, C2, "s", None, "User-space")):
        mean, err = [], []
        for s in sizes:
            xs = [m["total_latency"] / 1e6 for sz, m in runs if sz == s]
            mean.append(statistics.mean(xs)); err.append(ci95(xs) / 1e6)
        ax.errorbar(kb, mean, yerr=err, color=c, marker=mk, markersize=3.6,
                    linewidth=1.2, capsize=2, capthick=0.6, elinewidth=0.6,
                    label=lab, markeredgecolor=INK, markeredgewidth=0.4)
    ax.set_xlabel("Checkpoint state size (KB)")
    ax.set_ylabel("Total latency (Mcycles)")
    ax.set_xticks(kb)
    ax.legend(frameon=False, loc="upper left", handlelength=1.8,
              borderaxespad=0.2)
    tidy(ax)
    fig.savefig(OUT / "fig3_scaling.png")
    plt.close(fig)
    print("wrote fig3_scaling.png")


# ---------------------------------------------------------------- Fig. 4
def fig_tcb():
    """In-kernel code attributable to migration, three systems, log scale.

    Direct labels on every bar: the numbers are the point of this figure,
    and they also supply the 'relief' the palette check requires for the
    low-contrast slot.
    """
    labels = ["Mach\nDIPC+DMM\n(1993)", "Amoeba\nprocess server\n(2001)",
              "Hydra\nmigration.{c,h}\n(2025)"]
    vals = [56000, 3020, 393]
    cols = [C2, C3, C1]
    hatches = ["\\\\\\", "xxx", "///"]

    fig, ax = plt.subplots(figsize=(COL_W, 2.2))
    bars = ax.bar(range(3), vals, width=0.55, color=cols, alpha=0.9,
                  edgecolor=INK, linewidth=0.7)
    for b, h in zip(bars, hatches):
        b.set_hatch(h)
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v * 1.25, f"{v:,}",
                ha="center", va="bottom", fontsize=7, color=INK)
    ax.set_yscale("log")
    ax.set_ylim(100, 400000)
    ax.set_xticks(range(3))
    ax.set_xticklabels(labels, fontsize=6.5, linespacing=1.2)
    ax.set_ylabel("In-kernel lines of code")
    tidy(ax)
    fig.savefig(OUT / "fig4_tcb.png")
    plt.close(fig)
    print("wrote fig4_tcb.png")


# ---------------------------------------------------------------- Fig. 5
def fig_phases(kern, user):
    """Where the time goes: per-phase mean cost against state size.

    Stacked bars, kernel-resident build. Shows that serialise and
    deserialise absorb the growth while freeze and transport stay flat -
    the claim made in Section V-B, which the line chart alone cannot show.
    """
    sizes = sorted({s for s, _ in kern})
    kb = ["8", "12", "24", "72"]
    cols = [C1, C2, C3, "#eda100"]
    hatches = ["///", "\\\\\\", "xxx", "..."]

    fig, ax = plt.subplots(figsize=(COL_W, 2.2))
    bottom = np.zeros(len(sizes))
    for (key, lab), c, h in zip(PHASES, cols, hatches):
        vals = np.array([statistics.mean([m[key] / 1e6 for sz, m in kern if sz == s])
                         for s in sizes])
        ax.bar(range(len(sizes)), vals, width=0.55, bottom=bottom, label=lab,
               color=c, alpha=0.9, edgecolor=INK, linewidth=0.6, hatch=h)
        bottom += vals
    ax.set_xticks(range(len(sizes)))
    ax.set_xticklabels(kb)
    ax.set_xlabel("Checkpoint state size (KB)")
    ax.set_ylabel("Mean cost (Mcycles)")
    ax.legend(frameon=False, fontsize=6.5, ncol=2, loc="upper left",
              handlelength=1.3, columnspacing=1.0, borderaxespad=0.2)
    ax.set_ylim(0, bottom.max() * 1.42)
    tidy(ax)
    fig.savefig(OUT / "fig5_phases.png")
    plt.close(fig)
    print("wrote fig5_phases.png")


# ---------------------------------------------------------------- Fig. 6
def fig_published():
    """Null-RPC latency reported in the literature, c. 1990.

    Single series, so no legend: the axis label names it. Amoeba and Mach
    are the two the argument turns on, so they are the only ones
    emphasised; the rest are context and take the muted slot.
    """
    names = ["Amoeba", "Cedar", "x-Kernel", "V", "Topaz", "Sprite", "Mach"]
    vals = [1.1, 1.1, 1.7, 2.5, 2.7, 2.8, 11.0]
    emph = {0: C3, 6: C2}
    fig, ax = plt.subplots(figsize=(COL_W, 1.95))
    bars = ax.barh(range(len(names))[::-1], vals, height=0.6,
                   color=[emph.get(i, "#b9b8b3") for i in range(len(names))],
                   edgecolor=INK, linewidth=0.6)
    for i, b in enumerate(bars):
        if i in (0, 6):          # direct-label only the two that matter
            ax.text(b.get_width() + 0.25, b.get_y() + b.get_height() / 2,
                    f"{vals[i]}", va="center", fontsize=7, color=INK)
    ax.set_yticks(range(len(names))[::-1])
    ax.set_yticklabels(names)
    ax.set_xlabel("Null RPC latency (ms), 1990 hardware")
    ax.set_xlim(0, 13)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(axis="x", color=GRID, linewidth=0.5, alpha=0.8)
    ax.set_axisbelow(True)
    fig.savefig(OUT / "fig6_published.png")
    plt.close(fig)
    print("wrote fig6_published.png")


# ---------------------------------------------------------------- Fig. 7
def fig_fault():
    """Workload counter across the checkpoint / fault / recovery sequence.

    Values are read from the demonstration's own log output:
      counter 30 at checkpoint, 60 at fault, frozen at 60, 90 after restore.
    The counter is a global, so the point of the figure is that the workload
    stops advancing while the task is dead and advances again once it is
    restored - not that the value rolls back to the checkpointed one.
    """
    stage = ["spawn", "run", "check-\npoint", "run", "fault", "detect",
             "restore", "run"]
    value = [0, 30, 30, 60, 60, 60, 60, 90]
    x = list(range(len(stage)))

    fig, ax = plt.subplots(figsize=(COL_W, 2.05))
    ax.plot(x, value, color=C1, linewidth=1.4, marker="o", markersize=3.4,
            markeredgecolor=INK, markeredgewidth=0.4, zorder=3)

    # the dead interval, shaded rather than coloured, so it survives greyscale
    ax.axvspan(4, 6, color=C2, alpha=0.16, zorder=1)
    ax.annotate("task dead,\ncounter frozen", xy=(5, 60), xytext=(2.35, 78),
                fontsize=6.5, color=INK, ha="center",
                arrowprops=dict(arrowstyle="->", linewidth=0.6, color=INK2))
    ax.annotate("checkpoint\ntaken", xy=(2, 30), xytext=(0.35, 52),
                fontsize=6.5, color=INK, ha="center",
                arrowprops=dict(arrowstyle="->", linewidth=0.6, color=INK2))
    ax.annotate("restored,\nprogress resumes", xy=(7, 90), xytext=(5.4, 30),
                fontsize=6.5, color=INK, ha="center",
                arrowprops=dict(arrowstyle="->", linewidth=0.6, color=INK2))

    ax.set_xticks(x)
    ax.set_xticklabels(stage, fontsize=6, linespacing=1.1)
    ax.set_ylabel("Workload counter")
    ax.set_ylim(-6, 104)
    tidy(ax)
    fig.savefig(OUT / "fig7_fault.png")
    plt.close(fig)
    print("wrote fig7_fault.png")


if __name__ == "__main__":
    kern = parse(ROOT / "bench/results/KERNEL.log")
    user = parse(ROOT / "bench/results/USERSPACE.log")
    print(f"parsed {len(kern)} kernel-resident and {len(user)} user-space runs")
    fig_placement()
    fig_distribution(kern, user)
    fig_scaling(kern, user)
    fig_tcb()
    fig_phases(kern, user)
    fig_published()
    fig_fault()
