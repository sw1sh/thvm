// Concurrency primitives of the parallel reducer, after the platform stanza of
// TinyHVM/HVM4/clang/hvm.c, so the wsq port (util/wsq.c) keeps HVM4's types.

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
