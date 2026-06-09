#pragma once
#include <cstdint>

// One market-data event. 32 bytes: two per cache line, no padding surprises.
struct Msg {
  uint64_t t_ns;      // producer timestamp (calibrated TSC clock, see tsc.hpp)
  uint64_t seq;       // per-producer sequence number, starts at 0
  uint32_t producer;
  uint32_t kind;      // 0 = data, 1 = end-of-stream marker from this producer
  uint64_t payload;   // price/qty stand-in
};
static_assert(sizeof(Msg) == 32);
