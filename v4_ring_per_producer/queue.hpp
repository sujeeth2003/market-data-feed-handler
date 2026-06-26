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
class Queue {
  std::vector<std::unique_ptr<v3::Queue<N>>> rings_;
  size_t next_ = 0;

 public:
  explicit Queue(unsigned producers) {
    for (unsigned i = 0; i < producers; ++i) rings_.push_back(std::make_unique<v3::Queue<N>>(1));
  }
  void push(unsigned producer, const Msg& m) { rings_[producer]->push(0, m); }
  bool pop(Msg& out) {
    for (size_t i = 0, n = rings_.size(); i < n; ++i) {
      size_t r = next_ + i < n ? next_ + i : next_ + i - n;
      if (rings_[r]->pop(out)) { next_ = r + 1 == n ? 0 : r + 1; return true; }
    }
    return false;
  }
};
}  // namespace v4
