#pragma once
// v2: lock-free single-producer / single-consumer ring buffer.
//   * head/tail are plain atomics with acquire/release ordering, no CAS, no locks
//   * consumer busy-polls instead of sleeping (burns a core, removes the futex)
//   * exactly ONE producer: the ring is not safe with more
// Known flaw kept on purpose: head_ and tail_ are declared next to each other,
// so they share a cache line. Producer writes tail_, consumer writes head_, and
// the line ping-pongs between cores (false sharing). v3 fixes it after
// `perf c2c` shows the contention.
#include <atomic>
#include <cstddef>
#include <memory>
#include "../common/msg.hpp"
#include "../common/platform.hpp"

