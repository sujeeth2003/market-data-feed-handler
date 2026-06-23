#!/bin/sh
# Show false sharing in v2 and its absence in v3 with `perf c2c` (Linux; needs perf
# and a CPU with load-latency sampling; run as root or set perf_event_paranoid<=0).
# HITM (line modified in another core's cache) on the head_/tail_ line = false sharing.
set -e
[ -x build/bench ] || make build/bench
OUT=/tmp/c2c.$$.data
perf c2c record -o "$OUT" -- ./build/bench 1 2000000 0 1 >/dev/null
perf c2c report -i "$OUT" --stdio | sed -n '1,80p'
echo
echo "In the report, find the cache line holding v2::Queue::head_ and tail_: expect high HITM"
echo "and both fields on the same line. For v3::Queue they sit on different lines."
