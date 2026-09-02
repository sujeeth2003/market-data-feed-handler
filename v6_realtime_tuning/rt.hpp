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

namespace rt {
constexpr size_t kHuge = 2u << 20;

struct Region {
  void* p = nullptr;
  size_t bytes = 0;
  enum Kind { None, Mmap, Heap } kind = None;
  bool huge = false;   // true only if MAP_HUGETLB succeeded

  Region() = default;
  Region(const Region&) = delete;
  Region& operator=(const Region&) = delete;
  Region(Region&& o) noexcept : p(o.p), bytes(o.bytes), kind(o.kind), huge(o.huge) { o.p = nullptr; o.kind = None; }
  ~Region() {
#if defined(__linux__)
    if (kind == Mmap) munmap(p, bytes);
#endif
    if (kind == Heap) ::operator delete(p, std::align_val_t{4096});
  }
};

inline Region alloc(size_t bytes) {
  Region r;
  r.bytes = (bytes + kHuge - 1) / kHuge * kHuge;
#if defined(__linux__)
  void* m = mmap(nullptr, r.bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (m != MAP_FAILED) { r.p = m; r.kind = Region::Mmap; r.huge = true; return r; }
  m = mmap(nullptr, r.bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (m != MAP_FAILED) {
    madvise(m, r.bytes, MADV_HUGEPAGE);   // transparent huge pages if enabled
    r.p = m; r.kind = Region::Mmap; return r;
  }
#endif
  r.p = ::operator new(r.bytes, std::align_val_t{4096});
  r.kind = Region::Heap;
  return r;
}

