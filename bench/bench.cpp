// End-to-end producer -> consumer latency for v1..v4 (and v6 when built with -DWITH_RT on Linux).
// usage: bench [producers=1] [msgs_per_producer=500000] [gap_ns=1000] [pin=0|1]
//   gap_ns  spacing between sends per producer (1000 = 1 M msg/s/producer)
//   v2/v3 are single-producer and are skipped when producers > 1.
// Latency = consumer timestamp on receipt - producer timestamp at send.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include "../common/clock.hpp"
#include "../common/msg.hpp"
#include "../common/platform.hpp"
#include "../v1_mutex_queue/queue.hpp"
#include "../v2_spsc_ring/queue.hpp"
#include "../v3_padded/queue.hpp"
#include "../v4_ring_per_producer/queue.hpp"
#ifdef WITH_RT
#include "../v6_realtime_tuning/queue.hpp"
#endif

struct Result { uint32_t p50, p99, p999, max; uint64_t received, errors; double mps; };

template <class Q>
Result run(unsigned P, size_t per, uint64_t gap, bool pin) {
  Q q(P);
  std::vector<std::thread> th;
  for (unsigned p = 0; p < P; ++p) {
    th.emplace_back([&, p] {
      if (pin) pin_thread(p + 1);
      uint64_t next = now_ns();
      for (uint64_t i = 0; i < per; ++i) {
        if (gap) { while (now_ns() < next) cpu_relax(); next += gap; }
        q.push(p, Msg{now_ns(), i, p, 0, i});
      }
      q.push(p, Msg{now_ns(), per, p, 1, 0});
    });
  }
  if (pin) pin_thread(0);
  std::vector<uint64_t> expect(P, 0);
  std::vector<uint32_t> lat;
  lat.reserve(P * per);
  uint64_t errors = 0;
  unsigned stopped = 0;
  uint64_t t0 = now_ns();
  Msg m;
  while (stopped < P) {
    if (!q.pop(m)) { cpu_relax(); continue; }
    uint64_t t = now_ns();
    if (m.kind) { ++stopped; continue; }
    if (m.seq != expect[m.producer]) ++errors;            // loss or reorder within a producer
    expect[m.producer] = m.seq + 1;
    lat.push_back(t > m.t_ns ? (uint32_t)(t - m.t_ns) : 0);  // cross-core TSC skew can be a few ns
  }
  double dt = (double)(now_ns() - t0);
  for (auto& t : th) t.join();
  std::sort(lat.begin(), lat.end());
  auto at = [&](double f) { return lat[std::min(lat.size() - 1, (size_t)(f * lat.size()))]; };
  return {at(.5), at(.99), at(.999), lat.back(), lat.size(), errors, lat.size() / (dt / 1e3)};
}

static void show(const char* name, const Result& r) {
  std::printf("%-26s p50=%6uns p99=%7uns p99.9=%8uns max=%9uns  recv=%llu errors=%llu\n", name, r.p50, r.p99,
              r.p999, r.max, (unsigned long long)r.received, (unsigned long long)r.errors);
}

int main(int argc, char** argv) {
  unsigned P = argc > 1 ? (unsigned)std::atoi(argv[1]) : 1;
  size_t per = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 500000;
  uint64_t gap = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1000;
  bool pin = argc > 4 && std::atoi(argv[4]);
  (void)now_ns();  // calibrate before any timing
  std::printf("producers=%u msgs/producer=%zu gap=%lluns pin=%d\n", P, per, (unsigned long long)gap, (int)pin);
  show("v1 mutex + condvar", run<v1::Queue>(P, per, gap, pin));
  if (P == 1) {
    show("v2 SPSC (unpadded)", run<v2::Queue<>>(P, per, gap, pin));
    show("v3 SPSC (cache-line pad)", run<v3::Queue<>>(P, per, gap, pin));
  }
  show("v4 ring per producer", run<v4::Queue<>>(P, per, gap, pin));
#ifdef WITH_RT
  show("v6 + huge pages/mlock", run<v6::Queue<>>(P, per, gap, pin));
#endif
}
