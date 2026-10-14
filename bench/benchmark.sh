#!/usr/bin/env bash
#
# benchmark.sh - run Hydra migration self-test N times in QEMU, capture
# BENCH lines, write them to a log for analyze.py.
#
# Usage:
#   ./bench/benchmark.sh <mode> <repeats> <output_log>
#
#   mode    : KERNEL | USERSPACE
#   repeats : number of times to trigger the self-test in one boot
#   output  : log file path (defaults to build/bench/<mode>.log)
#
# Examples:
#   ./bench/benchmark.sh KERNEL 30
#   ./bench/benchmark.sh USERSPACE 30 build/bench/userspace-run1.log
#
# Notes:
#   - The kernel must already be built for the requested mode:
#       make MIGRATION_MODE=KERNEL    (or USERSPACE)
#   - We drive QEMU with `-serial mon:stdio` and feed 'c' on stdin to
#     trigger the self-test; the kernel emits BENCH lines we capture.
#   - QEMU is killed after a fixed timeout so repeated runs can proceed.

set -u
set -o pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

MODE=${1:-USERSPACE}
REPEATS=${2:-30}
OUT=${3:-build/bench/${MODE}.log}

if [[ "$MODE" != "KERNEL" && "$MODE" != "USERSPACE" ]]; then
    echo "ERROR: mode must be KERNEL or USERSPACE, got '$MODE'" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUT")"

if [[ ! -f build/kernel.elf ]]; then
    echo "ERROR: build/kernel.elf not built. Run 'make MIGRATION_MODE=$MODE' first." >&2
    exit 1
fi

# Confirm the kernel was built for the requested mode by grepping the
# elf for the banner text. (Cheap sanity check.)
EXPECTED_BANNER="Migration mode: $([[ $MODE == KERNEL ]] && echo KERNEL-RESIDENT || echo USER-SPACE)"
if ! strings build/kernel.elf 2>/dev/null | grep -aF "$EXPECTED_BANNER" >/dev/null; then
    echo "WARNING: build/kernel.elf does not appear to be built for $MODE mode" >&2
    echo "         Expected banner: $EXPECTED_BANNER" >&2
fi

QEMU_CMD=(qemu-system-riscv64 -machine virt -nographic -kernel build/kernel.elf
          -global virtio-mmio.force-legacy=false
          -netdev user,id=net0 -device virtio-net-device,netdev=net0)

# Run N rounds of the self-test by feeding 'c' repeatedly. Sleep a bit
# between each so the test has time to finish before the next press.
RUN_INPUT_LINES=()
for ((i=0; i<REPEATS; i++)); do
    RUN_INPUT_LINES+=("c")
done

echo "# Hydra migration benchmark"            >  "$OUT"
echo "# mode=$MODE repeats=$REPEATS"          >> "$OUT"
echo "# date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"  >> "$OUT"
echo "# kernel=$(sha256sum build/kernel.elf | awk '{print $1}')" >> "$OUT"

# Capture run with a hard timeout. Each 'c' takes ~1s in QEMU; allow
# generous slack. Use printf to emit one 'c' per second.
TIMEOUT=$((REPEATS * 2 + 5))
echo "# timeout=${TIMEOUT}s" >> "$OUT"

(
    for line in "${RUN_INPUT_LINES[@]}"; do
        printf '%s' "$line"
        sleep 1
    done
) | timeout "${TIMEOUT}s" "${QEMU_CMD[@]}" 2>&1 \
    | tee /tmp/qemu-run.log \
    | tr -d '\r' \
    | grep -a -E '^(BENCH |BENCH_SIZE )' \
    >> "$OUT" || true

NLINES=$(grep -a -c '^BENCH ' "$OUT" || true)
echo "[benchmark.sh] mode=$MODE captured $NLINES BENCH lines -> $OUT"
