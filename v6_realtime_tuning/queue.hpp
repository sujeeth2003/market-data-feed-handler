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

