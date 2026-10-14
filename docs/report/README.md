# Current dissertation

[Word report](Final_Report.docx) · [PDF preview](Final_Report.pdf) ·
[Supervisor revision notes](Revision_Notes.txt)

This is the supervisor-revised report delivered on 13 October 2026, renamed
and moved during repository cleanup. Its Word and PDF contents are unchanged.
It supersedes the earlier reports, chapter drafts, and standalone diagrams.

The report describes historical code paths under `kernal/`; that folder is now
`kernel/`. Old references to root `CHANGES.md` map to `docs/changes.md`, and
`test_distributed.sh` now lives under `scripts/`. These path changes do not
alter the historical measured implementation.

The unresolved four-byte checkpoint-size discrepancy remains disclosed in
Section 5.2.2. The citation check matched 52 reference keys; it did not
independently authenticate every unchanged source.

## Figures

These images are copied directly from the current Word report. Cover logos
are omitted. They are the reference assets for this report; diagnostic plot
tools write separate copies under `build/figures/`.

| Figure | Caption and image |
|---|---|
| 2.1 | [Null-RPC latency reported in the literature, c. 1990 (Tanenbaum et al., 1990, Fig. 6)](figures/figure-2-1.png) |
| 3.1 | [Conceptual framework for the architectural placement comparison](figures/figure-3-1.png) |
| 4.1 | [Local migration sequence — BENCH timestamps and per-phase cost](figures/figure-4-1.png) |
| 4.2 | [Total migration latency as a function of process state size](figures/figure-4-2.png) |
| 4.3 | [Per-phase contribution to migration latency by mode and payload size](figures/figure-4-3.png) |
| 4.4 | [Latency distribution by mode and payload size, n = 15 per box](figures/figure-4-4.png) |
| 4.5 | [Cross-node migration over VirtIO-Net — fragment protocol](figures/figure-4-5.png) |
| 4.6 | [Fault-tolerance demonstration — counter trajectory](figures/figure-4-6.png) |
| 4.7 | [Kernel TCB attributable to migration — Mach (1993) vs Hydra (2025)](figures/figure-4-7.png) |
| 4.8 | [In-kernel code size — Amoeba 5.3 (2001) vs Hydra (2025)](figures/figure-4-8.png) |
| 4.9 | [Cost of a round trip relative to the syscall floor — Amoeba 5.3 vs Hydra](figures/figure-4-9.png) |
| 5.1 | [Hydra components by logical layer (added blue, modified orange, inherited grey); the measured paths are not privilege-separated.](figures/figure-5-1.png) |
| 5.2 | [Hydra checkpoint record layout](figures/figure-5-2.png) |
| 5.3 | [Two architectural placements compared by the dissertation, selectable by MIGRATION_MODE](figures/figure-5-3.png) |
| 6.1 | [Adoption barriers from Milojičić et al. (1999), Section 7.3, reassessed by deployment context.](figures/figure-6-1.png) |
| 6.2 | [Microkernel and migration milestones, 1985–2025 (each event has a primary-source citation)](figures/figure-6-2.png) |
