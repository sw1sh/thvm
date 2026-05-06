// util/atomics.h -- shared concurrency primitives for the parallel
// reducer (Phase 1 of docs/aot.md / docs/plans/levy_optimal.md).
//
// Defines:
//   - cpu_relax()       busy-wait hint (yield on aarch64, pause on x86)
//   - CACHE_L1          cache-line size used to pad atomics
//   - CachePaddedAtomic _Atomic u64 padded to one cache line
//   - MAX_THREADS       upper bound on the worker count
//
// Mirrors TinyHVM/HVM4/clang/hvm.c's platform stanza so the wsq /
// wspq ports keep their identical type signatures.

#ifndef THVM_UTIL_ATOMICS_H
#define THVM_UTIL_ATOMICS_H

#include <stdatomic.h>

#if defined(__aarch64__)
#define cpu_relax() __asm__ __volatile__("yield" ::: "memory")
#elif defined(__x86_64__)
#define cpu_relax() __asm__ __volatile__("pause")
#else
#define cpu_relax() ((void)0)
#endif

#define CACHE_L1 128

typedef struct {
  _Alignas(CACHE_L1) _Atomic u64 v;
  char _pad[CACHE_L1 - sizeof(_Atomic u64)];
} CachePaddedAtomic;

#ifndef MAX_THREADS
#define MAX_THREADS 64
#endif

#endif // THVM_UTIL_ATOMICS_H
