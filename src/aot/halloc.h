// src/aot/halloc.h -- per-thread bump allocator for the AOT (ahead-of-time)
// runtime: one atomic on heap_next per chunk instead of one per continuation
// cell or CTR, like Bend2's tl_hp / tl_he
// (external/TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c:82).  A header of
// its own because cont.c, included before worker.c, allocates through it.

#ifndef THVM_AOT_HALLOC_H
#define THVM_AOT_HALLOC_H

#include "../thvm.h"

#define AOT_HEAP_CHUNK_CELLS  65536u   // 512 KB per chunk

extern __thread u64 aot_tl_chunk_lo;
extern __thread u64 aot_tl_chunk_hi;

static inline u64 aot_heap_alloc(u64 size) {
  if (aot_tl_chunk_lo + size > aot_tl_chunk_hi) {
    u64 chunk = AOT_HEAP_CHUNK_CELLS;
    if (size > chunk) chunk = size;
    u64 cap = gc_enabled() ? gc_from_end() : thvm_heap_cells();
    aot_tl_chunk_lo = (u64)__atomic_fetch_add(
        &CURRENT_CTX->heap_next, chunk, __ATOMIC_RELAXED);
    // heap_alloc's bound: the cells past the space's end are no one's.
    if (aot_tl_chunk_lo + size > cap) thvm_fatal("aot_heap_alloc: the heap is exhausted");
    aot_tl_chunk_hi = aot_tl_chunk_lo + chunk < cap ? aot_tl_chunk_lo + chunk : cap;
  }
  u64 at = aot_tl_chunk_lo;
  aot_tl_chunk_lo += size;
  return at;
}

// Each run starts with this: the caller's chunk may predate a heap reset.
// Spawned workers start with zeroed bounds and end with their run.
static inline void aot_heap_tl_reset(void) {
  aot_tl_chunk_lo = 0;
  aot_tl_chunk_hi = 0;
}

#endif  // THVM_AOT_HALLOC_H
