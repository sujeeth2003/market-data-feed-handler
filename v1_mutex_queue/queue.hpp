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
