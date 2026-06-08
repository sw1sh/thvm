// Bump allocator over CURRENT_CTX->heap_next.  Atomic-fetch-add so
// multiple workers in the parallel WNF / NF pool can allocate
// concurrently without losing cells.  Single-thread cost is identical
// to the legacy plain-bump on x86 / arm64 -- one LL/SC pair vs one
// load/store, both single-cycle on modern cores.
//
// Per-worker arenas are a future optimisation (HVM4 uses
// HEAP_NEXT[tid * STRIDE]); the global atomic bump is the simplest
// always-correct fallback so the rest of the parallel runtime can
// land without arena bookkeeping.

// Heap-exhaust recovery hook.  A LibraryLink entry (thvm_wl_atp_run_proof
// etc.) can register a longjmp target before driving the saturator: when
// heap_alloc would have called exit(1), it now longjmps back to that
// target, which returns LIBRARY_FUNCTION_ERROR to WL instead of killing
// WolframKernel.  Pre-fix, heap-exhaust exit'd the entire process --
// inside WolframKernel that orphans the kernel (the wrapper can't reap
// it cleanly), accumulating multi-GB ghosts that strain Darwin VM
// (feedback_wolframscript_oom_risk.md).  NULL = default behaviour
// (fprintf + exit), used by stand-alone benches.
//
// Set/cleared via thvm_set_heap_exhaust_handler.
#include <setjmp.h>
jmp_buf  *thvm_heap_exhaust_jmp = NULL;
volatile int thvm_heap_exhausted = 0;

fn u64 heap_alloc(u64 size) {
  u64 cap = gc_enabled() ? gc_from_end() : thvm_heap_cells();
  u64 at  = (u64)__atomic_fetch_add(&CURRENT_CTX->heap_next, size,
                                    __ATOMIC_RELAXED);
  if (at + size > cap) {
    fprintf(stderr, "heap_alloc: from-space exhausted (need %llu cells, "
            "HEAP_NEXT=%llu, cap=%llu); call gc_collect or grow GC_SPACE_SZ\n",
            (unsigned long long)size,
            (unsigned long long)at,
            (unsigned long long)cap);
    if (thvm_heap_exhaust_jmp != NULL) {
      thvm_heap_exhausted = 1;
      longjmp(*thvm_heap_exhaust_jmp, 1);
    }
    exit(1);
  }
  return at;
}
