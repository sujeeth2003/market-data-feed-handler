// End-to-end producer -> consumer latency for v1..v4 (and v6 when built with -DWITH_RT on Linux).
// usage: bench [producers=1] [msgs_per_producer=500000] [gap_ns=1000] [pin=0|1]
//   gap_ns  spacing between sends per producer (1000 = 1 M msg/s/producer)
//   v2/v3 are single-producer and are skipped when producers > 1.
// Latency = consumer timestamp on receipt - producer timestamp at send.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include "../common/clock.hpp"
#include "../common/msg.hpp"
#include "../common/platform.hpp"
#include "../v1_mutex_queue/queue.hpp"
#include "../v2_spsc_ring/queue.hpp"
#include "../v3_padded/queue.hpp"
#include "../v4_ring_per_producer/queue.hpp"
#ifdef WITH_RT
#include "../v6_realtime_tuning/queue.hpp"
#endif

