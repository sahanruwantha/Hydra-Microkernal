#!/usr/bin/env python3
"""
Pull cross-node migration BENCH numbers out of two-node QEMU logs.

Usage:
    bench/analyze_xmig.py /tmp/hydra-node1.log /tmp/hydra-node2.log
"""
import sys
import statistics
import math
from pathlib import Path


# Which subsystem a BENCH bracket belongs to, keyed off the most recent
# context line in the log.
#
# This matters because a single boot emits BENCH brackets from three
# unrelated sources, and they share label names:
#
#   XMIG    - cross-node migration (apps/cross_node_migration.c)
#   NETTEST - ARP/ping smoke test at boot (apps/net_test.c), also net_send_*
#   LOCAL   - the 60-run local migration sweep, also rest_start/rest_done
#
# Pooling them was the original bug here: "source fragmentation" mixed in
# 20 boot-time pings, and "dest restore" mixed in 61 local-sweep restores.
CTX_MARKERS = (
    ("[nettest]", "NETTEST"),
    ("[migration] checkpoint", "LOCAL"),
    ("[migtest]", "LOCAL"),
)


def parse_bench(path: Path):
    """Return ordered list of (label, cycles, context) tuples in source order.

    NOTE ON LINE ENDINGS: these logs come off a UART and use bare "\\r" as a
    separator (tens of thousands of them), with the occasional NUL byte.
    str.splitlines() splits on \\r, \\n and \\r\\n, so it reads them correctly.
    Do not reach for grep/awk here — they split only on \\n and see roughly
    half the lines, silently mis-parsing the file.
    """
    out = []
    ctx = None
    xmig_ctx = "XMIG"
    for raw in path.read_bytes().decode("utf-8", errors="ignore").splitlines():
        raw = raw.strip("\r")
        if "[xmig] === isolated U-mode" in raw:
            xmig_ctx = "UXMIG"
            ctx = xmig_ctx
        elif "[xmig] === run " in raw:
            xmig_ctx = "XMIG"
            ctx = xmig_ctx
        elif "[xmig]" in raw:
            ctx = xmig_ctx
        else:
            for needle, name in CTX_MARKERS:
                if needle in raw:
                    ctx = name
                    break
        if not raw.startswith("BENCH "):
            continue
        parts = raw.split()
        if len(parts) != 3:
            continue
        try:
            out.append((parts[1], int(parts[2]), ctx))
        except ValueError:
            pass
    return out


def pair_brackets(events, start_lbl, end_lbl, want_ctx=None):
    """Yield (start_cycles, end_cycles) for each matching pair.

    If want_ctx is given, only pairs opened in that context are returned.
    """
    out = []
    pending = None
    pending_ctx = None
    for lbl, c, ctx in events:
        if lbl == start_lbl:
            pending, pending_ctx = c, ctx
        elif lbl == end_lbl and pending is not None:
            if want_ctx is None or pending_ctx == want_ctx:
                out.append((pending, c))
            pending, pending_ctx = None, None
    return out


def ci95(vals):
    if not vals:
        return float("nan"), float("nan")
    if len(vals) < 2:
        return float(vals[0]), float("nan")
    m = statistics.mean(vals)
    s = statistics.stdev(vals)
    return m, 1.96 * s / math.sqrt(len(vals))


def main(argv):
    if len(argv) != 3:
        print("usage: analyze_xmig.py NODE1.log NODE2.log", file=sys.stderr)
        return 2
    src = parse_bench(Path(argv[1]))
    dst = parse_bench(Path(argv[2]))

    # Restrict to brackets opened inside the cross-node migration path.
    # Without this filter net_send_* also catches the boot-time ARP/ping
    # smoke test and rest_* also catches the local 60-run migration sweep.
    send_pairs = pair_brackets(src, "net_send_start", "net_send_done", "XMIG")
    ack_pairs = pair_brackets(src, "net_send_start", "net_ack_done", "XMIG")
    recv_pairs = pair_brackets(dst, "net_recv_start", "net_recv_done", "XMIG")
    rest_pairs = pair_brackets(dst, "rest_start",     "rest_done",     "XMIG")
    user_send_pairs = pair_brackets(src, "net_send_start", "net_send_done", "UXMIG")
    user_ack_pairs = pair_brackets(src, "net_send_start", "net_ack_done", "UXMIG")
    user_recv_pairs = pair_brackets(dst, "net_recv_start", "net_recv_done", "UXMIG")
    user_rest_pairs = pair_brackets(dst, "rest_start", "rest_done", "UXMIG")

    # Unfiltered totals, reported alongside so any future contamination is
    # visible rather than silently folded into the means.
    send_all = pair_brackets(src, "net_send_start", "net_send_done")
    recv_all = pair_brackets(dst, "net_recv_start", "net_recv_done")
    rest_all = pair_brackets(dst, "rest_start",     "rest_done")

    send_costs = [b - a for a, b in send_pairs]
    ack_costs = [b - a for a, b in ack_pairs]
    recv_costs = [b - a for a, b in recv_pairs]
    rest_costs = [b - a for a, b in rest_pairs]

    print(f"\n=== Cross-node migration (source {Path(argv[1]).name},"
          f" dest {Path(argv[2]).name}) ===")
    print("  context filter: XMIG/UXMIG (excludes net_test pings and the"
          " local migration sweep; reports S and U images separately)")
    print(f"  source fragmentations:    {len(send_pairs):>3}"
          f"   (of {len(send_all)} net_send brackets in log)")
    print(f"  destination reassemblies: {len(recv_pairs):>3}"
          f"   (of {len(recv_all)} net_recv brackets in log)")
    print(f"  destination restores:     {len(rest_pairs):>3}"
          f"   (of {len(rest_all)} restore brackets in log)")
    print(f"  isolated U migrations:    {len(user_send_pairs):>3} source /"
          f" {len(user_rest_pairs)} destination")

    def print_table(title, rows):
        print(f"\n{title}")
        print(f"{'phase':<22} {'mean ± 95% CI (cycles)':>28}   {'min':>10}   {'max':>10}")
        print("-" * 80)
        for label, vals in rows:
            if vals:
                mean, hw = ci95(vals)
                mn = min(vals); mx = max(vals)
                hw_s = f"{hw:.0f}" if not math.isnan(hw) else "n/a"
                print(f"{label:<22} {mean:>15.0f} ± {hw_s:<10}   {mn:>10}   {mx:>10}")
            else:
                print(f"{label:<22} {'n/a':>28}   {'-':>10}   {'-':>10}")

    print_table("Supervisor checkpoints", (
        ("source fragmentation", send_costs),
        ("source ACK completion", ack_costs),
        ("dest reassembly",      recv_costs),
        ("dest restore",         rest_costs),
    ))
    print_table("Isolated U checkpoint", (
        ("source fragmentation", [b - a for a, b in user_send_pairs]),
        ("source ACK completion", [b - a for a, b in user_ack_pairs]),
        ("dest reassembly", [b - a for a, b in user_recv_pairs]),
        ("dest restore", [b - a for a, b in user_rest_pairs]),
    ))

    print("\nIntervals use node-local QEMU cycle counters; source and destination")
    print("means are not a paired end-to-end latency. The isolated sample is")
    print("reported separately and does not establish a performance distribution.")

if __name__ == "__main__":
    sys.exit(main(sys.argv))
