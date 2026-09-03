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
void check(const char* name, unsigned P, uint64_t per) {
  Q q(P);
  std::vector<std::thread> th;
  for (unsigned p = 0; p < P; ++p)
    th.emplace_back([&, p] {
      for (uint64_t i = 0; i < per; ++i) q.push(p, Msg{0, i, p, 0, i * 3 + p});
      q.push(p, Msg{0, per, p, 1, 0});
    });
  std::vector<uint64_t> next(P, 0);
  unsigned stopped = 0;
  uint64_t bad = 0, got = 0;
  Msg m;
  while (stopped < P) {
    if (!q.pop(m)) continue;
    if (m.kind) { ++stopped; continue; }
    if (m.seq != next[m.producer] || m.payload != m.seq * 3 + m.producer) ++bad;
    next[m.producer] = m.seq + 1;
    ++got;
  }
  for (auto& t : th) t.join();
  bool ok = !bad && got == P * per;
  std::printf("%-22s producers=%u  received=%llu  bad=%llu  %s\n", name, P, (unsigned long long)got,
              (unsigned long long)bad, ok ? "ok" : "FAIL");
  failures += !ok;
}

int main() {
  const uint64_t n = 300000;
  check<v1::Queue>("v1 mutex+condvar", 3, n);
  check<v2::Queue<>>("v2 spsc", 1, n);
  check<v3::Queue<>>("v3 spsc padded", 1, n);
  check<v4::Queue<>>("v4 ring/producer", 3, n);
  check<v4::Queue<1024>>("v4 small rings (backpressure)", 3, n);
  std::puts(failures ? "FAILED" : "all queue tests ok");
  return failures != 0;
}
