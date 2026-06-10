#pragma once
// Calibrated time-stamp counter. rdtsc is ~5-8 ns and needs no syscall.
// Requires an invariant TSC (constant rate, synchronized across cores).
#include <chrono>
#include <cstdint>

inline uint64_t rdtsc() { return __builtin_ia32_rdtsc(); }

struct TscCal {
  double ns_per_tick = 0;
  uint64_t tsc0 = 0, ns0 = 0;
  static uint64_t steady_ns() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
  }
  // Longer calibration = smaller error. 50 ms gives well under 0.1% on typical CPUs.
  explicit TscCal(uint64_t ms = 50) {
    uint64_t s0 = steady_ns(), t0 = rdtsc();
    while (steady_ns() - s0 < ms * 1'000'000) {}
    uint64_t s1 = steady_ns(), t1 = rdtsc();
    ns_per_tick = double(s1 - s0) / double(t1 - t0);
    tsc0 = t1; ns0 = s1;
  }
  uint64_t to_ns(uint64_t tsc) const { return ns0 + (uint64_t)(((double)tsc - (double)tsc0) * ns_per_tick); }
  double delta_ns(uint64_t a, uint64_t b) const { return (double)(b - a) * ns_per_tick; }
};
