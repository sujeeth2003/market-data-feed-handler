#pragma once
// Calibrated time-stamp counter. rdtsc is ~5-8 ns and needs no syscall.
// Requires an invariant TSC (constant rate, synchronized across cores).
#include <chrono>
#include <cstdint>

inline uint64_t rdtsc() { return __builtin_ia32_rdtsc(); }

