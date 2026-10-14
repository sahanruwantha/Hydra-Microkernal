#!/usr/bin/env bash
#
# micro_bench.sh - repeat the null-syscall / IPC round-trip micro-benchmark
# across N fresh QEMU boots and collect the BENCH_MICRO lines.
#
# Unlike the migration sweep (benchmark.sh), these benchmarks run once per
# boot during kernel init, so repeats mean repeated boots.
#
# Usage:
#   ./bench/micro_bench.sh [repeats] [output_log]
#
# Requires a kernel built with the benchmarks enabled and per-message IPC
# tracing off -- each trace is a synchronous UART write and would otherwise
# dominate the IPC measurement by ~30x:
#
#   make MICRO_BENCH=1 TRACE_IPC=0
#
set -u
set -o pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

REPEATS=${1:-10}
OUT=${2:-build/bench/micro.log}
BOOT_TIMEOUT=${BOOT_TIMEOUT:-150}

if [[ ! -f build/kernel.elf ]]; then
    echo "ERROR: build/kernel.elf not built." >&2
    echo "Run: make MICRO_BENCH=1 TRACE_IPC=0" >&2
    exit 1
fi

if ! grep -q "MICRO_BENCH=1" build/.build-config 2>/dev/null; then
    echo "ERROR: build/kernel.elf was not built with MICRO_BENCH=1." >&2
    echo "Run: make MICRO_BENCH=1 TRACE_IPC=0" >&2
    exit 1
fi

if ! grep -q "TRACE_IPC=0" build/.build-config 2>/dev/null; then
    echo "WARNING: TRACE_IPC is not 0. Per-message UART tracing will" >&2
    echo "         dominate the IPC round-trip measurement (~30x)." >&2
    echo "         Rebuild with: make MICRO_BENCH=1 TRACE_IPC=0" >&2
fi

mkdir -p "$(dirname "$OUT")"
: > "$OUT"

echo "# micro_bench.sh: $REPEATS boots, $(date -Is)" >> "$OUT"
echo "# build-config:" >> "$OUT"
sed 's/^/#   /' build/.build-config >> "$OUT"

for ((i = 1; i <= REPEATS; i++)); do
    printf 'boot %d/%d ... ' "$i" "$REPEATS"
    # QEMU's serial console emits CRLF, so strip the CR before matching
    # -- an anchored grep silently matches nothing otherwise.
    run_out=$(timeout "$BOOT_TIMEOUT" \
        qemu-system-riscv64 -machine virt -nographic -kernel build/kernel.elf \
        </dev/null 2>&1 | tr -d '\r' | grep -a '^BENCH_MICRO')
    if [[ -z "$run_out" ]]; then
        echo "no BENCH_MICRO output (skipped)"
        continue
    fi
    # Tag every line with its run so analyze_micro.py can group them.
    echo "$run_out" | sed "s/^BENCH_MICRO /BENCH_MICRO run=$i /" >> "$OUT"
    echo "ok"
done

echo
echo "wrote $OUT"
echo "Analyse with: python3 bench/analyze_micro.py $OUT"
