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

namespace v2 {
template <size_t N = (1u << 16)>
class Queue {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");
  std::atomic<size_t> head_{0};  // consumer writes
  std::atomic<size_t> tail_{0};  // producer writes  <- same cache line as head_
  std::unique_ptr<Msg[]> buf_ = std::make_unique<Msg[]>(N);

 public:
  explicit Queue(unsigned producers = 1) { if (producers != 1) throw "v2 supports exactly one producer"; }
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
}  // namespace v2
