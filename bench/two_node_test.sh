#!/usr/bin/env bash
#
# two_node_test.sh — launch two Hydra QEMU instances over UDP multicast.
# Both instances run the same build/kernel.elf but identify
# themselves by their MAC last byte (52:54:00:00:00:01 vs ...02).
#
# Outputs:
#   /tmp/hydra-node1.log
#   /tmp/hydra-node2.log
#   Set HYDRA_LOG_DIR to retain logs and packet captures elsewhere.
#
# The final audit exits non-zero unless networking, local regressions, and both
# supervisor and isolated cross-node migrations meet their exact pass counts.
#
# Usage: ./bench/two_node_test.sh [timeout_seconds]

set -u
set -o pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

TIMEOUT=${1:-60}
LOG_DIR=${HYDRA_LOG_DIR:-/tmp}

if [[ $# -gt 1 || ! $TIMEOUT =~ ^[1-9][0-9]*$ ]]; then
    echo "Usage: $0 [positive timeout_seconds]" >&2
    exit 2
fi
mkdir -p "$LOG_DIR"

if [[ ! -f build/kernel.elf ]]; then
    echo "build/kernel.elf not built" >&2; exit 1
fi

cleanup() {
    pkill -P $$ 2>/dev/null || true
}
trap cleanup EXIT

# Use UDP multicast so both QEMUs symmetrically join the same group.
# TCP listen/connect is finicky around startup ordering; multicast is
# fire-and-forget.
MCAST_ADDR=${MCAST_ADDR:-230.0.0.1:1234}

echo "[two_node] launching node 1 (mcast $MCAST_ADDR) ..."
rm -f "$LOG_DIR/hydra-node1-tx.pcap"
qemu-system-riscv64 -machine virt -nographic -kernel build/kernel.elf \
    -global virtio-mmio.force-legacy=false \
    -netdev socket,id=net0,mcast=${MCAST_ADDR} \
    -device virtio-net-device,netdev=net0,mac=52:54:00:00:00:01 \
    -object "filter-dump,id=f1,netdev=net0,file=$LOG_DIR/hydra-node1-tx.pcap" \
    < /dev/null > "$LOG_DIR/hydra-node1.log" 2>&1 &
N1=$!

echo "[two_node] launching node 2 (mcast $MCAST_ADDR) ..."
rm -f "$LOG_DIR/hydra-node2-tx.pcap"
qemu-system-riscv64 -machine virt -nographic -kernel build/kernel.elf \
    -global virtio-mmio.force-legacy=false \
    -netdev socket,id=net0,mcast=${MCAST_ADDR} \
    -device virtio-net-device,netdev=net0,mac=52:54:00:00:00:02 \
    -object "filter-dump,id=f2,netdev=net0,file=$LOG_DIR/hydra-node2-tx.pcap" \
    < /dev/null > "$LOG_DIR/hydra-node2.log" 2>&1 &
N2=$!

# Wait for the timeout, then kill both.
sleep "$TIMEOUT"
kill "$N1" "$N2" 2>/dev/null || true
wait "$N1" "$N2" 2>/dev/null || true

echo "[two_node] node 1 log lines: $(wc -l < "$LOG_DIR/hydra-node1.log")"
echo "[two_node] node 2 log lines: $(wc -l < "$LOG_DIR/hydra-node2.log")"

for name in node1 node2; do
    log="$LOG_DIR/hydra-$name.log"
    peer_line=$(tr -d '\r' < "$log" | grep -a 'peer_frames_seen' | head -1)
    rx_lines=$(tr -d '\r' < "$log" | grep -ac '\[nettest\] RX hydra' || true)
    mac=$(tr -d '\r' < "$log" | grep -aoE 'MAC [0-9a-f:]+ ' | head -1)
    echo "[two_node] $name  $mac  hydra_rx=$rx_lines  $peer_line"
done

count_matches() {
    tr -d '\000\r' < "$1" | grep -Ec "$2" || true
}

N1_LOG="$LOG_DIR/hydra-node1.log"
N2_LOG="$LOG_DIR/hydra-node2.log"
n1_peer=$(count_matches "$N1_LOG" 'peer_frames_seen=20')
n2_peer=$(count_matches "$N2_LOG" 'peer_frames_seen=20')
n1_local=$(count_matches "$N1_LOG" '\[migtest\] PASS: restored task is running')
n2_local=$(count_matches "$N2_LOG" '\[migtest\] PASS: restored task is running')
n1_iso=$(count_matches "$N1_LOG" '\[isotest\] PASS')
n2_iso=$(count_matches "$N2_LOG" '\[isotest\] PASS')
source_acks=$(count_matches "$N1_LOG" '\[xmig\].*ACK received')
supervisor_restores=$(count_matches "$N2_LOG" '\[xmig\] DESTINATION: PASS')
isolated_restores=$(count_matches "$N2_LOG" '\[xmig\] DESTINATION U: PASS')
ERROR_PATTERN='FAIL|FATAL|Unhandled trap|Recursive trap|timed out|TIMEOUT|restore failed|UNACKNOWLEDGED|WARN: (counter|workload) did not advance'
errors=$(count_matches "$N1_LOG" "$ERROR_PATTERN")
errors=$((errors + $(count_matches "$N2_LOG" "$ERROR_PATTERN")))

echo "[two_node] audit local=$n1_local/$n2_local isolation=$n1_iso/$n2_iso peer=$n1_peer/$n2_peer supervisor=$supervisor_restores/15 isolated=$isolated_restores/1 acks=$source_acks/16 errors=$errors"

if [[ $n1_peer -ne 1 || $n2_peer -ne 1 ||
      $n1_local -ne 60 || $n2_local -ne 60 ||
      $n1_iso -ne 1 || $n2_iso -ne 1 ||
      $source_acks -ne 16 || $supervisor_restores -ne 15 ||
      $isolated_restores -ne 1 || $errors -ne 0 ]]; then
    echo "[two_node] FAIL: acceptance criteria not met" >&2
    exit 1
fi

echo "[two_node] PASS: all acceptance criteria met"
