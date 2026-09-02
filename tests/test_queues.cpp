// Correctness under concurrency: every message arrives exactly once, in order
// per producer, across v1, v2, v3 and v4 (multi-producer versions with 3 producers).
#include <cstdio>
#include <thread>
#include <vector>
#include "../common/clock.hpp"
#include "../v1_mutex_queue/queue.hpp"
#include "../v2_spsc_ring/queue.hpp"
#include "../v3_padded/queue.hpp"
#include "../v4_ring_per_producer/queue.hpp"

static int failures = 0;

template <class Q>
