#!/usr/bin/env bash
# Build both comparison paths, run existing acceptance, and show real evidence.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
TIMEOUT=${1:-60}
if [[ $# -gt 1 || ! $TIMEOUT =~ ^[1-9][0-9]*$ ]]; then
    echo "Usage: $0 [positive timeout_seconds, default 60]" >&2
    exit 2
fi

mkdir -p build/demo
RUN_DIR=$(mktemp -d "$REPO_ROOT/build/demo/run-XXXXXX")
echo "Presentation evidence: $RUN_DIR"
git rev-parse HEAD > "$RUN_DIR/commit.txt"
git status --short > "$RUN_DIR/working-tree.txt"
qemu-system-riscv64 --version > "$RUN_DIR/qemu-version.txt"

echo
echo "Objective 1: two selectable invocation paths"
for mode in KERNEL USERSPACE; do
    echo "Building $mode (compiler output saved in the run folder)..."
    if ! make MIGRATION_MODE="$mode" > "$RUN_DIR/build-$mode.log" 2>&1; then
        cat "$RUN_DIR/build-$mode.log" >&2
        exit 1
    fi
    case "$mode" in
        KERNEL) banner='Migration mode: KERNEL-RESIDENT' ;;
        USERSPACE) banner='Migration mode: USER-SPACE' ;;
    esac
    strings build/kernel.elf | grep -F "$banner"
done
cp build/.build-config "$RUN_DIR/build-config.txt"
sha256sum build/kernel.elf > "$RUN_DIR/kernel-sha256.txt"
grep -nE 'long (n|new_pid) = (migration_|syscall_)(checkpoint|restore)' apps/migration_test.c
echo "Both historical comparison paths execute in supervisor mode."

echo
echo "Current implementation: two-node functional acceptance (${TIMEOUT}s)"
if HYDRA_LOG_DIR="$RUN_DIR" ./bench/two_node_test.sh "$TIMEOUT" 2>&1 | tee "$RUN_DIR/acceptance.log"; then
    accepted=1
else
    accepted=0
fi

# Preserve raw UART logs and split bare CR/CRLF correctly for terminal searches.
python3 - "$RUN_DIR" <<'PY'
from pathlib import Path
import sys
folder = Path(sys.argv[1])
for n in (1, 2):
    raw = folder / f"hydra-node{n}.log"
    if raw.exists():
        text = raw.read_bytes().decode("utf-8", errors="replace").replace("\0", "")
        (folder / f"node{n}.log").write_text("\n".join(text.splitlines()) + "\n")
PY

if [[ $accepted -ne 1 ]]; then
    echo "Demo did not pass. Inspect $RUN_DIR; do not present it as a successful run." >&2
    exit 1
fi

echo
echo "Local restore, scheduler, isolation, and malformed-record evidence"
for node in 1 2; do
    log="$RUN_DIR/node$node.log"
    echo "Node $node:"
    # Each marker is a live assertion: missing evidence stops the demo.
    for marker in '[schedtest] PASS:' '[mmtest] PASS:' '[isotest] PASS:' \
                  '[migtest] PASS: malformed checkpoint rejected' \
                  '[migtest] PASS: restored task is running' \
                  '[fdemo] PASS: workload recovered from simulated fault'; do
        if ! grep -F -m1 "$marker" "$log"; then
            echo "Missing required evidence in $log: $marker" >&2
            exit 1
        fi
    done
done

echo
echo "Fault recovery: capture, simulated failure, restore, resumed counter"
grep -E '\[fdemo\] (Step [235678]:|PASS:)' "$RUN_DIR/node1.log"
echo
echo "Cross-node transfer: source acknowledgement and destination evidence"
grep -m1 -E '\[xmig\].*ACK received' "$RUN_DIR/node1.log"
grep -F -m1 '[xmig] DESTINATION: PASS' "$RUN_DIR/node2.log"
grep -F -m1 '[xmig] DESTINATION U: PASS' "$RUN_DIR/node2.log"

echo
echo "Objective 2: one local timestamp sequence from this run"
awk '/^BENCH_SIZE / {show=1} show && /^BENCH(_SIZE)? / {print} show && /^BENCH rest_done / {exit}' "$RUN_DIR/node1.log"
python3 bench/analyze.py "$RUN_DIR/hydra-node1.log" > "$RUN_DIR/current-local-analysis.txt"
python3 bench/analyze_xmig.py "$RUN_DIR/hydra-node1.log" "$RUN_DIR/hydra-node2.log" > "$RUN_DIR/current-cross-node-analysis.txt"
wc -l kernel/migration.c kernel/migration.h | tee "$RUN_DIR/checkpoint-source-lines.txt"
echo "These are checkpoint source lines, not the entire trusted kernel."

echo
echo "Objectives 2 and 3: historical dissertation comparison (saved observations)"
python3 bench/analyze.py bench/results/KERNEL.log bench/results/USERSPACE.log | tee "$RUN_DIR/historical-comparison.txt"
echo "Current ACK/retry validation and the historical 8/15 transport sample are separate."
echo "QEMU-specific sample means do not establish hardware-boundary equivalence."
echo
echo "Objective 4: discuss the evidence and literature using docs/demo.md."
echo "Objective 5: source, commands, commit identity, and raw logs are reproducible artefacts."
echo "Demo checks passed. Evidence saved in $RUN_DIR"
