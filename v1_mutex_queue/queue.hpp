#pragma once
// v1: the obvious multi-producer design. std::deque guarded by a mutex, with a
// condition variable so the consumer sleeps when idle.
// Cost to look for: when the queue is empty the consumer goes to sleep in the
// kernel (futex wait) and every push has to wake it (futex wake). The sleep/wake
// path, not the queue operation, is what shows up in p99 / p99.9.
#include <condition_variable>
#include <deque>
#include <mutex>
#include "../common/msg.hpp"

namespace v1 {
class Queue {
  std::mutex m_;
  std::condition_variable cv_;
  std::deque<Msg> q_;

 public:
  explicit Queue(unsigned /*producers*/) {}
  void push(unsigned /*producer*/, const Msg& msg) {
    { std::lock_guard<std::mutex> g(m_); q_.push_back(msg); }
    cv_.notify_one();
  }
  // Blocks until a message is available (the stop markers guarantee wake-ups).
  bool pop(Msg& out) {
    std::unique_lock<std::mutex> g(m_);
    cv_.wait(g, [&] { return !q_.empty(); });
    out = q_.front();
    q_.pop_front();
    return true;
  }
};
}  // namespace v1
