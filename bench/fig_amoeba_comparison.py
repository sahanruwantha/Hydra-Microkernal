#!/usr/bin/env python3
"""Three Amoeba-vs-Hydra figures for Chapter 5.

Every number here is either measured or quoted with a page-level source.
Provenance for all of them is in bench/amoeba/RESULTS.md.

  fig_amoeba_tcb.png    in-kernel code size, measured with wc -l on the
                        real Amoeba 2001 source tree (src-kernel-2002a.tgz
                        from the FSD-Amoeba 2002B CD) and on this tree.

  fig_amoeba_published.png
                        null-RPC latency reported in the literature,
                        Tanenbaum et al., CACM 33(12), Dec 1990, Fig. 6.
                        1990s hardware -- historical context only.

  fig_amoeba_ratio.png  measured ratio of RPC/IPC round trip to null
                        syscall, for real Amoeba under qemu-system-i386
                        and Hydra under qemu-system-riscv64, both on the
                        same host. Absolute times are NOT comparable
                        across the two ISAs; the ratio largely cancels
                        emulation and ISA differences, which is why it is
                        the number the comparison rests on.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

OUT = Path("bench/results")
OUT.mkdir(parents=True, exist_ok=True)

AMOEBA = "#c0392b"
HYDRA = "#27ae60"
OTHER = "#7f8c8d"
INK = "#34495e"


def _clean(ax):
    ax.grid(True, axis="y", alpha=0.3)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


# --------------------------------------------------------------------
# Figure 1: in-kernel TCB, measured
# --------------------------------------------------------------------
def fig_tcb():
    groups = ["RPC / IPC", "Process mgmt\n+ migration", "Whole kernel"]
    amoeba = [3752, 3020, 115824]
    hydra = [450, 393, 5558]

    fig, ax = plt.subplots(figsize=(10, 5.5), constrained_layout=True)
    x = np.arange(len(groups))
    w = 0.36

    b1 = ax.bar(x - w / 2, amoeba, w, label="Amoeba 5.3 (2001)",
                color=AMOEBA, edgecolor=INK, linewidth=1.0)
    b2 = ax.bar(x + w / 2, hydra, w, label="Hydra (this work)",
                color=HYDRA, edgecolor=INK, linewidth=1.0)

    for bars in (b1, b2):
        for bar in bars:
            ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() * 1.12,
                    f"{int(bar.get_height()):,}", ha="center", va="bottom",
                    fontsize=10, weight="bold", color=INK)

    ax.set_yscale("log")
    ax.set_ylim(100, 3_000_000)
    ax.set_xticks(x)
    ax.set_xticklabels(groups, fontsize=11)
    ax.set_ylabel("Lines of code inside the kernel (log scale)", fontsize=11)
    ax.legend(fontsize=10, loc="upper center", ncol=2,
              bbox_to_anchor=(0.5, -0.13), frameon=False)
    ax.set_title("In-kernel code size: Amoeba 5.3 vs Hydra",
                 fontsize=13, weight="bold", pad=8)
    _clean(ax)

    for i, (a, h) in enumerate(zip(amoeba, hydra)):
        ax.text(i, 900_000, f"{a / h:.1f}x", ha="center", fontsize=11,
                weight="bold", color=AMOEBA)

    fig.text(0.5, -0.26,
             "Measured with `wc -l`: Amoeba from src-kernel-2002a.tgz (FSD-Amoeba 2002B CD, 287 files); Hydra from this tree.\n"
             "Amoeba's process server (server/process/, excluding the map.c.orig editor backup) implements PS_CHECKPOINT and\n"
             "immigrating threads, so it is the direct functional counterpart to Hydra's migration.{c,h}.",
             ha="center", fontsize=9, color=INK, style="italic")

    p = OUT / "fig_amoeba_tcb.png"
    fig.savefig(p, dpi=160, bbox_inches="tight")
    print(f"wrote {p}")
    plt.close(fig)


# --------------------------------------------------------------------
# Figure 2: published null-RPC latency (literature)
# --------------------------------------------------------------------
def fig_published():
    # CACM 1990 Fig. 6, in the paper's own order.
    systems = ["Amoeba\n(Sun 3/60)", "Cedar\n(Dorado)", "x-Kernel\n(Sun 3/75)",
               "V\n(Sun 3/75)", "Topaz\n(Firefly)", "Sprite\n(Sun 3/75)",
               "Mach\n(Sun 3/60)"]
    latency = [1.1, 1.1, 1.7, 2.5, 2.7, 2.8, 11.0]
    colors = [AMOEBA] + [OTHER] * 5 + ["#e67e22"]

    fig, ax = plt.subplots(figsize=(10, 5), constrained_layout=True)
    bars = ax.bar(np.arange(len(systems)), latency, color=colors,
                  edgecolor=INK, linewidth=1.0, width=0.6)

    for bar, v in zip(bars, latency):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.2,
                f"{v} ms", ha="center", va="bottom", fontsize=10,
                weight="bold", color=INK)

    ax.set_xticks(np.arange(len(systems)))
    ax.set_xticklabels(systems, fontsize=9)
    ax.set_ylabel("Null RPC latency (ms)", fontsize=11)
    ax.set_ylim(0, 12.5)
    ax.set_title("Null-RPC latency reported in the literature, c. 1990",
                 fontsize=13, weight="bold", pad=8)
    _clean(ax)

    fig.text(0.5, -0.16,
             "Quoted from Tanenbaum, van Renesse, van Staveren, Sharp, Mullender, Jansen and van Rossum, \"Experiences with the\n"
             "Amoeba Distributed Operating System\", CACM 33(12), Dec 1990, Fig. 6. These are 1990s machines (2-5 MIPS) and are\n"
             "NOT comparable with the emulated measurements in the other figures; they show what the design cost at the time.",
             ha="center", fontsize=9, color=INK, style="italic")

    p = OUT / "fig_amoeba_published.png"
    fig.savefig(p, dpi=160, bbox_inches="tight")
    print(f"wrote {p}")
    plt.close(fig)


# --------------------------------------------------------------------
# Figure 3: measured ratio, RPC/IPC round trip vs null syscall
# --------------------------------------------------------------------
def fig_ratio():
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5),
                                   constrained_layout=True,
                                   gridspec_kw={"width_ratios": [1.15, 1]})

    # Left: each system in its own units -- deliberately not compared.
    labels = ["Amoeba\nnull syscall", "Amoeba\nnull RPC",
              "Hydra\nnull syscall", "Hydra\nIPC round trip"]
    vals = [0.767, 19.94, 4804, 190229]
    texts = ["0.77 us", "19.9 us", "4,804 cyc", "190,229 cyc"]
    colors = [AMOEBA, AMOEBA, HYDRA, HYDRA]

    x = np.arange(4)
    bars = ax1.bar(x, vals, color=colors, edgecolor=INK, linewidth=1.0,
                   width=0.6)
    for bar, t in zip(bars, texts):
        ax1.text(bar.get_x() + bar.get_width() / 2, bar.get_height() * 1.15,
                 t, ha="center", va="bottom", fontsize=10, weight="bold",
                 color=INK)
    ax1.set_yscale("log")
    ax1.set_ylim(0.3, 2_000_000)
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels, fontsize=9)
    ax1.set_ylabel("Cost in each system's own units (log scale)", fontsize=10)
    ax1.set_title("Raw costs — different units, NOT comparable",
                  fontsize=11, weight="bold", pad=6)
    _clean(ax1)
    ax1.axvline(1.5, color=INK, linestyle=":", linewidth=1.2)

    # Right: the ratio, which is comparable.
    rl = ["Amoeba 5.3\n(i386, QEMU)", "Hydra\n(RISC-V, QEMU)"]
    rv = [26.0, 39.7]
    rerr = [0.4, 1.8]
    bars = ax2.bar(np.arange(2), rv, yerr=rerr, capsize=6,
                   color=[AMOEBA, HYDRA], edgecolor=INK, linewidth=1.0,
                   width=0.5)
    for bar, v in zip(bars, rv):
        ax2.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 2.5,
                 f"{v}x", ha="center", va="bottom", fontsize=13,
                 weight="bold", color=INK)
    ax2.set_xticks(np.arange(2))
    ax2.set_xticklabels(rl, fontsize=10)
    ax2.set_ylabel("RPC / IPC round trip, in units of one null syscall",
                   fontsize=10)
    ax2.set_ylim(0, 50)
    ax2.set_title("Ratio — normalised, comparable",
                  fontsize=11, weight="bold", pad=6)
    _clean(ax2)

    fig.suptitle("Cost of a round trip relative to the syscall floor: real Amoeba vs Hydra",
                 fontsize=13, weight="bold")

    fig.text(0.5, -0.30,
             "Both measured on the same host: Amoeba 5.3 under qemu-system-i386 using its OWN tests (test/performance/syscall/sys_null.c,\n"
             "test/performance/rpc/rpc_client.c); Hydra under qemu-system-riscv64 using apps/micro_bench.c with real U-mode ecall traps.\n"
             "Amoeba: null syscall 0.767 us (2 runs of 60M calls), null RPC 19.94 us (sd 0.25, n=9 blocks of 50,000). Hydra: n=7 boots.\n"
             "Absolute times cannot be compared across the two ISAs and emulators; the ratio cancels most of that. Caveat: Amoeba's\n"
             "rpc_trans is one user-to-user request/reply, while Hydra's round trip also includes scheduling latency for a supervisor\n"
             "echo server, so part of Hydra's 39.7x is scheduler delay rather than IPC cost.",
             ha="center", fontsize=8.5, color=INK, style="italic")

    p = OUT / "fig_amoeba_ratio.png"
    fig.savefig(p, dpi=160, bbox_inches="tight")
    print(f"wrote {p}")
    plt.close(fig)


if __name__ == "__main__":
    fig_tcb()
    fig_published()
    fig_ratio()
