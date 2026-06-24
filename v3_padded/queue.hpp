#pragma once
// v3: v2 plus cache-line isolation. head_, tail_ and the buffer each get their
// own 64-byte line (alignas(64)), so the producer's stores to tail_ no longer
// invalidate the line the consumer is spinning on to read head_.
// Evidence: `perf c2c record` on v2 reports HITM on the head_/tail_ line; the
// same profile on v3 does not (scripts/perf_c2c.sh).
#include <atomic>
#include <cstddef>
#include <memory>
#include "../common/msg.hpp"
#include "../common/platform.hpp"

namespace v3 {
template <size_t N = (1u << 16)>
