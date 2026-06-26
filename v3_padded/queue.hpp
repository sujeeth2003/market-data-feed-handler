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
class Queue {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");
  alignas(64) std::atomic<size_t> head_{0};   // consumer writes
  alignas(64) std::atomic<size_t> tail_{0};   // producer writes
  alignas(64) std::unique_ptr<Msg[]> buf_ = std::make_unique<Msg[]>(N);

 public:
  explicit Queue(unsigned producers = 1) { if (producers != 1) throw "v3 supports exactly one producer"; }
  bool try_push(const Msg& m) {
    size_t t = tail_.load(std::memory_order_relaxed);
    if (t - head_.load(std::memory_order_acquire) == N) return false;
    buf_[t & (N - 1)] = m;
    tail_.store(t + 1, std::memory_order_release);
    return true;
  }
  void push(unsigned, const Msg& m) { while (!try_push(m)) cpu_relax(); }
  bool pop(Msg& out) {
    size_t h = head_.load(std::memory_order_relaxed);
    if (h == tail_.load(std::memory_order_acquire)) return false;
    out = buf_[h & (N - 1)];
    head_.store(h + 1, std::memory_order_release);
    return true;
  }
};
}  // namespace v3
