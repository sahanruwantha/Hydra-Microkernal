#!/usr/bin/env python3
"""Figure A: kernel TCB attributable to migration — Hydra vs Mach.

All numbers are sourced and verifiable:
  Mach DIPC = 32,000 lines  — Milojičić et al. (1999), p. 29:
     "the size of DIPC (nmk21b1) consisted of over 32,000 lines of code"
  Mach DMM  = 24,000 lines  — Milojičić et al. (1999), p. 29:
     "The DMM ... consists of 24,000 lines of code"
  Hydra migration.c = 319 lines, migration.h = 74 lines
     — from `wc -l kernal/migration.{c,h}` on the current source tree.

Apples-to-apples caveat: Mach's DIPC+DMM provided general distributed
IPC and memory management used for many things beyond migration; Hydra's
migration.{c,h} provide only the checkpoint/restore primitives.
Network transport, retry, and policy live in user space on Hydra
(net_driver.c is 506 lines, bullet.c is 221 lines, also user-space).
The chart compares only what lives INSIDE the kernel for the migration
mechanism — that's the architectural measurement Chapter 4 §4.1.5 makes.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

components = [
    ("Mach DIPC\n(in-kernel distributed IPC)",      32000, "#c0392b"),
    ("Mach DMM\n(in-kernel distributed memory)",    24000, "#e74c3c"),
    ("Hydra migration.c +\nmigration.h (this work)",  393, "#27ae60"),
]

labels = [c[0] for c in components]
sizes  = [c[1] for c in components]
colors = [c[2] for c in components]

fig, ax = plt.subplots(figsize=(11, 5.5), constrained_layout=True)

x = np.arange(len(labels))
bars = ax.bar(x, sizes, color=colors, edgecolor="#34495e", linewidth=1.2,
              width=0.6)

# Value labels on each bar
for bar, val in zip(bars, sizes):
    ax.text(bar.get_x() + bar.get_width()/2,
            bar.get_height() + 600, f"{val:,}",
            ha="center", va="bottom",
            fontsize=12, weight="bold", color="#34495e")

ax.set_xticks(x)
ax.set_xticklabels(labels, fontsize=10)
ax.set_ylabel("Lines of code in the kernel TCB", fontsize=11)
ax.set_ylim(0, 36000)
ax.grid(True, axis="y", alpha=0.3)
ax.spines["top"].set_visible(False)
ax.spines["right"].set_visible(False)

ax.set_title("Kernel TCB attributable to migration: Mach (1993) vs Hydra (2025)",
             fontsize=13, weight="bold", pad=8)

# Summary box (verifiable arithmetic) — placed on the right where the
# tall Mach bars don't reach.
ax.text(0.62, 0.92,
        "Mach in-kernel total:  56,000 lines  (DIPC + DMM)\n"
        "Hydra in-kernel total:    393 lines  (migration.c + migration.h)\n"
        "Ratio:  56,000 / 393  ≈  142×",
        transform=ax.transAxes, fontsize=10, family="monospace",
        color="#34495e", weight="bold",
        bbox=dict(boxstyle="round,pad=0.4", facecolor="#ecf0f1",
                  edgecolor="#bdc3c7", linewidth=0.8),
        verticalalignment="top")

# Honest caption
fig.text(0.5, -0.06,
         "Mach figures quoted from Milojičić et al. (1999, p. 29). Hydra figures from `wc -l kernal/migration.{c,h}`.\n"
         "Comparison is in-kernel migration code only. Mach's DIPC/DMM also supported general distributed IPC and memory\n"
         "beyond migration; Hydra delegates transport, retry, and policy to user-space (net_driver.c, bullet.c, not counted).",
         ha="center", fontsize=9, color="#34495e", style="italic")

out = Path("bench/results/fig_tcb_comparison.png")
fig.savefig(out, dpi=160, bbox_inches="tight")
print(f"wrote {out}")
