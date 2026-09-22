# Multi-Producer Market Data Feed Handler (C++20, x86-64)

Getting market-data events from producer threads (and eventually from the network) to a consumer with the lowest and most *predictable* latency. Six versions; each fixes the bottleneck the previous one exposed. Every version reports **p50 / p99 / p99.9 / max**, because tail latency is the point.

| Version | Change | What it targets |
|---|---|---|
| [v1_mutex_queue](v1_mutex_queue/queue.hpp) | `std::deque` + mutex + condition variable | Baseline. Tail latency comes from futex sleep/wake, not queue work |
| [v2_spsc_ring](v2_spsc_ring/queue.hpp) | Lock-free SPSC ring, acquire/release ordering, busy-polling consumer | Removes locks and the futex |
| [v3_padded](v3_padded/queue.hpp) | `alignas(64)` on head/tail/buffer | False sharing found with `perf c2c` (`scripts/perf_c2c.sh`) |
| [v4_ring_per_producer](v4_ring_per_producer/queue.hpp) | One SPSC ring per producer, single consumer polls all | Multi-producer with no CAS, so no retry storms and no ABA |
| [v5_af_packet](v5_af_packet/feed_rx.cpp) | `AF_PACKET` + `PACKET_MMAP` (TPACKET_V2): read packets straight from a memory-mapped ring | No `recvfrom()` per packet, no copy, no special NIC (not full kernel bypass) |
| [v6_realtime_tuning](v6_realtime_tuning/queue.hpp) | Core pinning, `isolcpus`, huge pages (`MAP_HUGETLB`), `mlock`, pre-touched memory | Page faults, TLB misses, scheduler and swap jitter |

## Build, test, run
```bash
make test                 # every message exactly once, in order per producer (1 and 3 producers, backpressure case)
make bench                # v1-v4: latency, 1 and 3 producers
make build/bench_rt       # adds v6 (huge pages / mlock) to the comparison
make rx                   # Linux + root: v5 AF_PACKET demo on loopback
sh scripts/perf_c2c.sh    # Linux: false sharing evidence, v2 vs v3
sh scripts/rt_setup.sh    # Linux: the boot/kernel settings v6 assumes (documentation)
```
Latency = consumer receive timestamp minus the producer's send timestamp, from a calibrated `rdtsc` clock.

## What has and has not been verified
| | Status |
|---|---|
| v1-v4 correctness (`make test`) | Passed on Windows 11 / clang 21 |
| v1-v4 latency | Measured (below) |
| v6 code path | Runs; on this machine it fell back to 4 KiB pages and no `mlock`, so **the huge-page effect was not measured** |
| v5 `AF_PACKET` | **Compiles for Linux, never executed** (no Linux host here). Run `make rx` as root and treat results as unmeasured until you do |
| `perf c2c`, `isolcpus` effects | **Not measured** (Windows dev box) |

## Results (what I can honestly claim)
11th-gen Core i5-1135G7 (4C/8T laptop), Windows 11, clang 21 `-O2`, 1 producer, 300k messages at 1 M msg/s, threads **not pinned**:

| Version | p50 | p99 | p99.9 |
|---|---|---|---|
| v1 mutex + condvar | 271 ns | 10,652 ns | 83,520 ns |
| v2 SPSC (unpadded) | 86 ns | 373 ns | 43,258 ns |
| v3 SPSC (padded) | 90 ns | 1,774 ns | 52,435 ns |
| v4 ring per producer | 102 ns | 179 ns | 18,688 ns |
| v6 (huge pages unavailable here) | 103 ns | 163 ns | 10,843 ns |

- **Robust:** v1 -> v2 cuts p99 by ~30x. That is the lock / futex removal and it reproduces every run.
- **Not distinguishable here:** v2 vs v3 vs v4 vs v6 swap places between runs. On an unpinned laptop the OS scheduler dominates p99.9 (tens of microseconds) and hides the cache-line and paging effects those versions target. The false-sharing fix (v3) in particular needs pinned cores on separate physical cores plus `perf c2c` to show. That is a reason to run it on an isolated Linux machine, not a reason to claim a win now.
- 3 producers oversubscribes this 4-core laptop (3 producers + consumer + OS), so its tail numbers are scheduling noise.

## Design notes
- `Msg` is 32 bytes, so two fit a cache line.
- Rings are power-of-two sized; indices are monotonically increasing counters masked on access, so full/empty are unambiguous without wasting a slot.
- v4 gives approximate cross-producer ordering (each ring is FIFO; the merge is by polling). Per-producer order and completeness are verified by sequence number.
- The consumer busy-polls and burns a core by design. That is the trade for removing the futex.
