#pragma once
// v4: multi-producer without a multi-producer queue.
// Each producer owns a private SPSC ring (v3); the single consumer polls all
// rings round-robin. There is no CAS anywhere, so there is no CAS retry storm
// and no ABA problem: every ring has exactly one writer of tail_ and one of head_.
// Trade-off: arrival order *across* producers is approximate (each ring is FIFO,
// the merge is by polling), which is fine for independent feeds and is checked
// per producer by sequence number.
#include <memory>
#include <vector>
#include "../v3_padded/queue.hpp"

namespace v4 {
template <size_t N = (1u << 16)>
