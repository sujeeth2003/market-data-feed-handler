#pragma once
#include <thread>
#if defined(_WIN32)
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
  inline void pin_thread(unsigned cpu) { SetThreadAffinityMask(GetCurrentThread(), 1ull << cpu); }
#elif defined(__linux__)
  #include <pthread.h>
  #include <sched.h>
  inline void pin_thread(unsigned cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
  }
#else
  inline void pin_thread(unsigned) {}
#endif

inline void cpu_relax() {
#if defined(__x86_64__) || defined(_M_X64)
  __builtin_ia32_pause();
#else
  std::this_thread::yield();
#endif
}
