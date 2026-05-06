// src/aot/halloc.h
//
// Thread-local bump allocator for the AOT runtime.
//
// At T>1, every cont_alloc + every CTR construction would otherwise
// hit `__atomic_fetch_add(&CURRENT_CTX->heap_next, ...)` -- 800k+
// contended atomics for a tree-sum at d=18.  Caching one chunk per
// thread (Bend2's `tl_hp`/`tl_he` pattern) collapses this to one
// atomic per chunk (~12 atomics per thread for d=18).
//
// Lives in its own header (not src/aot/worker.c) because cont.c is
// compiled BEFORE worker.c in the bundle and needs to call
// aot_heap_alloc.
//
// Reset across aot_run calls: aot_heap_tl_reset() sets the calling
// thread's tl state to (0, 0), forcing a fresh chunk reservation
// on the first alloc.  Spawned workers start with zero-initialised
// __thread vars (per the C standard), so they always get a fresh
// chunk at their first alloc.  No epoch counter needed because
// workers don't outlive their aot_run.

#ifndef THVM_AOT_HALLOC_H
#define THVM_AOT_HALLOC_H

#include "../thvm.h"

#define AOT_HEAP_CHUNK_CELLS  65536u   // 64k cells = 512KB per chunk

extern __thread u64 aot_tl_chunk_lo;
extern __thread u64 aot_tl_chunk_hi;

static inline u64 aot_heap_alloc(u64 size) {
  if (aot_tl_chunk_lo + size > aot_tl_chunk_hi) {
    u64 chunk = AOT_HEAP_CHUNK_CELLS;
    if (size > chunk) chunk = size;
    aot_tl_chunk_lo = (u64)__atomic_fetch_add(
        &CURRENT_CTX->heap_next, chunk, __ATOMIC_RELAXED);
    aot_tl_chunk_hi = aot_tl_chunk_lo + chunk;
  }
  u64 at = aot_tl_chunk_lo;
  aot_tl_chunk_lo += size;
  return at;
}

static inline void aot_heap_tl_reset(void) {
  aot_tl_chunk_lo = 0;
  aot_tl_chunk_hi = 0;
}

#endif  // THVM_AOT_HALLOC_H
