#pragma once
// v6: v4's ring-per-producer design with every ring's storage on huge pages,
// pre-touched and mlock'ed, so the steady state has no page faults, no TLB
// pressure from 4 KiB pages, and no swap. Thread pinning and isolated cores
// are applied by the caller (bench pin flag, scripts/rt_setup.sh).
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <vector>
#include "../common/msg.hpp"
#include "../common/platform.hpp"
#include "rt.hpp"

namespace v6 {
template <size_t N = (1u << 16)>
class Ring {
  static_assert((N & (N - 1)) == 0);
  alignas(64) std::atomic<size_t> head_{0};
  alignas(64) std::atomic<size_t> tail_{0};
  alignas(64) rt::Region mem_ = rt::alloc(N * sizeof(Msg));
  Msg* buf_ = static_cast<Msg*>(mem_.p);

 public:
  Ring() {
    rt::pretouch(mem_);
    if (!rt::lock(mem_)) std::fputs("v6: mlock failed (raise RLIMIT_MEMLOCK); continuing\n", stderr);
  }
  bool huge() const { return mem_.huge; }
  bool try_push(const Msg& m) {
    size_t t = tail_.load(std::memory_order_relaxed);
    if (t - head_.load(std::memory_order_acquire) == N) return false;
    buf_[t & (N - 1)] = m;
    tail_.store(t + 1, std::memory_order_release);
    return true;
  }
  bool pop(Msg& out) {
    size_t h = head_.load(std::memory_order_relaxed);
    if (h == tail_.load(std::memory_order_acquire)) return false;
    out = buf_[h & (N - 1)];
    head_.store(h + 1, std::memory_order_release);
    return true;
  }
};

template <size_t N = (1u << 16)>
class Queue {
  std::vector<std::unique_ptr<Ring<N>>> rings_;
  size_t next_ = 0;

 public:
  explicit Queue(unsigned producers) {
    for (unsigned i = 0; i < producers; ++i) rings_.push_back(std::make_unique<Ring<N>>());
    std::printf("v6: huge pages %s\n", rings_[0]->huge() ? "in use" : "NOT available (fell back to 4 KiB pages)");
  }
  void push(unsigned producer, const Msg& m) { while (!rings_[producer]->try_push(m)) cpu_relax(); }
  bool pop(Msg& out) {
    for (size_t i = 0, n = rings_.size(); i < n; ++i) {
      size_t r = next_ + i < n ? next_ + i : next_ + i - n;
      if (rings_[r]->pop(out)) { next_ = r + 1 == n ? 0 : r + 1; return true; }
    }
    return false;
  }
};
}  // namespace v6
