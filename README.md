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

