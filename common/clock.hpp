#pragma once
#include "tsc.hpp"

// Process-wide calibrated clock in nanoseconds. First call calibrates (~50 ms).
inline uint64_t now_ns() {
  static const TscCal cal;
  return cal.to_ns(rdtsc());
}
