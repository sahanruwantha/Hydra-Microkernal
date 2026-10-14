#!/usr/bin/env python3
"""
Parse BENCH lines from one or more benchmark.sh log files and produce
the comparison table that Chapter 5 of the dissertation hangs on.

Input log format (lines starting with #) are metadata. Each BENCH line:

    BENCH <label> <cycle_count>

Multiple BENCH lines with the same label across the file represent
repeated runs of the same migration phase. Phases recognised:

    ckpt_enter   - migration_checkpoint() entered
    ckpt_freeze  - source task marked BLOCKED
    ckpt_copied  - state serialized into user buffer
    send_start   - first byte handed to transport
    send_done    - transport acknowledged
    recv_start   - destination received first byte
    recv_done    - destination has full record
    rest_start   - migration_restore() entered
    rest_done    - destination task READY

Derived metrics:
    freeze_time   = ckpt_freeze - ckpt_enter
    serialize_t   = ckpt_copied - ckpt_freeze
    transport_t   = recv_done   - send_start
    deserialize_t = rest_done   - rest_start
    total_latency = rest_done   - ckpt_enter

Usage:
    bench/analyze.py bench/results/KERNEL.log bench/results/USERSPACE.log
"""

from __future__ import annotations

import math
import statistics
import sys
from pathlib import Path

PHASES = [
    "ckpt_enter",
    "ckpt_freeze",
    "ckpt_copied",
    "send_start",
    "send_done",
    "recv_start",
    "recv_done",
    "rest_start",
    "rest_done",
]

DERIVED = [
    ("freeze_time",   "ckpt_freeze",  "ckpt_enter"),
    ("serialize_t",   "ckpt_copied",  "ckpt_freeze"),
    ("transport_t",   "recv_done",    "send_start"),
    ("deserialize_t", "rest_done",    "rest_start"),
    ("total_latency", "rest_done",    "ckpt_enter"),
]


def parse_log(path: Path) -> list[dict[str, int]]:
    """Group BENCH lines into per-migration records.

    Assumption: each call to migration_self_test() emits the full
    sequence of phases once, in order, with no interleaving. We use
    'ckpt_enter' as the run delimiter: a new ckpt_enter starts a new
    record. BENCH_SIZE lines optionally tag the next record with the
    payload size in bytes (extra payload beyond the 8 KB stack).
    """
    runs: list[dict[str, int]] = []
    cur: dict[str, int] | None = None
    pending_size: int | None = None
    # Read as bytes and decode permissively; QEMU mixes \r\n line endings
    # and the log file may contain non-text noise that .read_text() chokes
    # on. We sanitise here so parsing is robust.
    raw_text = path.read_bytes().decode("utf-8", errors="ignore")
    for raw in raw_text.splitlines():
        raw = raw.lstrip("\r").rstrip("\r")
        if raw.startswith("BENCH_SIZE "):
            try:
                pending_size = int(raw.split()[1])
            except (IndexError, ValueError):
                pending_size = None
            continue
        if not raw.startswith("BENCH "):
            continue
        parts = raw.split()
        if len(parts) != 3:
            continue
        _, label, cycle_str = parts
        try:
            cycles = int(cycle_str)
        except ValueError:
            continue
        if label == "ckpt_enter":
            if cur is not None:
                runs.append(cur)
            # A local benchmark run is always introduced by BENCH_SIZE.
            # Ignore checkpoint brackets from the later fault demo; without
            # this guard a full UART log is reported as n=61 instead of 60.
            cur = {label: cycles} if pending_size is not None else None
            if pending_size is not None:
                cur["__size"] = pending_size
                pending_size = None
        elif cur is not None:
            cur.setdefault(label, cycles)
    if cur is not None:
        runs.append(cur)
    return runs


def derive(runs: list[dict[str, int]]) -> dict[str, list[int]]:
    """Compute derived metrics per run."""
    out: dict[str, list[int]] = {name: [] for name, *_ in DERIVED}
    for run in runs:
        for name, end, start in DERIVED:
            if end in run and start in run:
                out[name].append(run[end] - run[start])
    return out


def ci95(values: list[int]) -> tuple[float, float]:
    """Mean and half-width of 95% CI for a sample, in cycles."""
    if len(values) < 2:
        return (float(values[0]) if values else float("nan"), float("nan"))
    mean = statistics.mean(values)
    stdev = statistics.stdev(values)
    halfwidth = 1.96 * stdev / math.sqrt(len(values))
    return (mean, halfwidth)


def fmt(mean: float, hw: float) -> str:
    if math.isnan(mean):
        return "n/a"
    if math.isnan(hw):
        return f"{mean:>10.0f}"
    return f"{mean:>10.0f} ± {hw:.0f}"


def percentile(values: list[int], p: float) -> float:
    if not values:
        return float("nan")
    s = sorted(values)
    k = (len(s) - 1) * p
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return float(s[f])
    return s[f] + (s[c] - s[f]) * (k - f)


def report(path: Path) -> dict[str, list[int]]:
    runs = parse_log(path)
    print(f"\n=== {path}  (n={len(runs)} runs) ===")
    derived = derive(runs)
    print(f"{'metric':<16} {'mean ± 95% CI':>22}   {'p50':>10}   {'p95':>10}")
    print("-" * 70)
    for name, *_ in DERIVED:
        vals = derived[name]
        mean, hw = ci95(vals)
        p50 = percentile(vals, 0.5)
        p95 = percentile(vals, 0.95)
        print(f"{name:<16} {fmt(mean, hw):>22}   {p50:>10.0f}   {p95:>10.0f}")

    # Per-size breakdown if BENCH_SIZE tags were present.
    sizes = sorted({r.get("__size") for r in runs if "__size" in r})
    if sizes:
        print(f"\n  Per-payload-size totals (mean cycles, n per bucket):")
        print(f"  {'bytes':>8}  {'n':>4}  {'total_latency':>16}  {'freeze_t':>12}  {'serialize_t':>14}")
        for sz in sizes:
            bucket = [r for r in runs if r.get("__size") == sz]
            b_derived = derive(bucket)
            tot = ci95(b_derived["total_latency"])[0]
            frz = ci95(b_derived["freeze_time"])[0]
            ser = ci95(b_derived["serialize_t"])[0]
            print(f"  {sz:>8}  {len(bucket):>4}  {tot:>16.0f}  {frz:>12.0f}  {ser:>14.0f}")
    return derived


def comparison_table(a_path: Path, b_path: Path) -> None:
    a = derive(parse_log(a_path))
    b = derive(parse_log(b_path))
    print(f"\n=== Comparison: {a_path.stem} vs {b_path.stem} ===")
    print(f"{'metric':<16} {a_path.stem:>14} {b_path.stem:>14} {'ratio (B/A)':>14}")
    print("-" * 70)
    for name, *_ in DERIVED:
        am, _ = ci95(a[name])
        bm, _ = ci95(b[name])
        ratio = bm / am if am and not math.isnan(am) and am != 0 else float("nan")
        print(f"{name:<16} {am:>14.0f} {bm:>14.0f} {ratio:>14.2f}")


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    logs = [Path(p) for p in argv[1:]]
    for p in logs:
        if not p.exists():
            print(f"ERROR: {p} not found", file=sys.stderr)
            return 1
        report(p)
    if len(logs) == 2:
        comparison_table(logs[0], logs[1])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
