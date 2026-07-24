#pragma once
// v6 helpers: the "make the OS stay out of the way" toolkit.
//   alloc()    huge-page backed memory (MAP_HUGETLB, then THP madvise, then plain)
//   pretouch() fault every page in now so the hot path never takes a page fault
//   lock()     mlock so pages can never be swapped out
//   set_fifo() SCHED_FIFO real-time priority (needs CAP_SYS_NICE)
// Everything degrades gracefully: each call reports what it achieved and the
// program still runs (slower) without privileges. Pair with the kernel/boot
// settings in scripts/rt_setup.sh (isolcpus, nohz_full, hugepages).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#if defined(__linux__)
  #include <sched.h>
  #include <sys/mman.h>
#endif

