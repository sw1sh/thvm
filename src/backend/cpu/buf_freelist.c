// backend/cpu/buf_freelist.c - per-size-class free-list of
// recyclable cpu_buf slots (bm4a of the bench arc).
//
// `cpu_buf_freelist_push(buf_id)` records that `buf_id`'s storage
// is no longer needed.  The buf's data pointer survives in
// CPU_BUFS[buf_id].data (the underlying calloc'd memory is NOT
// freed), but downstream ops MUST treat the buf as dead until
// it's reissued by `cpu_buf_alloc`.
//
// `cpu_buf_alloc(nbytes)` (modified in buf_alloc.c) first scans
// CPU_FREELIST for a slot whose nbytes matches the request and
// pops it, returning the recycled buf_id with refcount/preserved/
// freeable cleared and data zeroed.  Only on a miss does it fall
// back to a fresh calloc.
//
// Linear scan is fine for our scale (LeNet generates < 500
// kernels per Adam step; beautiful_mnist a few hundred forward
// kernels; CPU_FREELIST_CAP is 4096).  A bucketed hash would help
// at higher fan-outs but isn't worth the bookkeeping today.
//
// Bigger picture: this complements the existing watermark pool
// (cpu_buf_pool_begin/rollback) -- those bound an entire
// thvm_realize call and free everything not preserved.  The
// free-list operates at finer grain (per-buffer) and is driven
// by the consumer_count + freeable signals from kernel_fire_by_id.

// Forward decl: cpu_buf_free is defined in buf_free.c (included later
// in src/thvm.c; this file lives BEFORE it because buf_alloc.c reads
// the freelist).  Arena views' parent decref path needs it.
fn void cpu_buf_free(u32 buf_id);

fn void cpu_buf_freelist_push(u32 buf_id) {
  if (CPU_BUFS == NULL) return;   // Metal-active: no CPU bufs.
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  if (CPU_FREELIST_LEN >= CPU_FREELIST_CAP) {
    // Free-list saturated; drop the slot on the floor.  The buf's
    // memory stays alive until cpu_shutdown, so we leak storage
    // until end-of-session but never lose correctness.
    return;
  }
  CPU_FREELIST[CPU_FREELIST_LEN++] = buf_id;
  // Drop refcount to 0 so the recycled-but-unallocated buf
  // doesn't keep counting toward TTotalBufBytes / the
  // memory-probe sums.  cpu_buf_freelist_try_pop resets
  // refcount = 1 on the next allocation.
  CpuBuf *b = &CPU_BUFS[buf_id];
  // Arena view: also drop the parent's refcount.  Otherwise an arena
  // CpuBuf parked here without its tied parent decref leaks the arena
  // bytes until end-of-session.
  u32 parent = b->parent_buf_id;
  if (parent != 0 && parent < CPU_BUFS_NEXT
      && CPU_BUFS[parent].refcount > 0) {
    if (--CPU_BUFS[parent].refcount == 0) cpu_buf_free(parent);
  }
  b->parent_buf_id = 0;
  b->refcount  = 0;
  b->preserved = 0;
  b->freeable  = 0;
}

fn u32 cpu_buf_freelist_try_pop(u64 nbytes) {
  if (CPU_BUFS == NULL) return 0;
  // Best-fit: reuse the smallest parked buffer whose storage is >=
  // nbytes.  Exact-match-only barely recycled across a net's varied
  // activation sizes (peak memory ~= sum of all activations); best-fit
  // lets a freed 10MB activation back a later 8MB one, so the live set
  // (not the sum) bounds memory -- tinygrad's reuse behaviour.  The
  // kernel only touches [0,nbytes); the slot keeps its larger nbytes.
  u32 best = 0; u64 best_nb = (u64)-1;
  for (u32 i = 0; i < CPU_FREELIST_LEN; i++) {
    u32 bid = CPU_FREELIST[i];
    if (bid == 0 || bid >= CPU_BUFS_NEXT) continue;
    CpuBuf *b = &CPU_BUFS[bid];
    if (!b->owns_data || b->data == NULL) continue;  // skip externals
    if (b->nbytes < nbytes) continue;
    if (b->nbytes < best_nb) { best_nb = b->nbytes; best = i; }
  }
  if (best_nb == (u64)-1) return 0;   // miss
  u32 bid = CPU_FREELIST[best];
  CPU_FREELIST[best] = CPU_FREELIST[CPU_FREELIST_LEN - 1];
  CPU_FREELIST_LEN--;
  CpuBuf *b = &CPU_BUFS[bid];
  memset(b->data, 0, (size_t)nbytes);
  b->refcount  = 1;
  b->preserved = 0;
  b->freeable  = 0;
  return bid;
}

// Undo a cpu_buf_freelist_push: if `buf_id` is still parked on the
// free-list (i.e. no intervening cpu_buf_alloc re-issued it), pull it
// off and restore refcount = 1 so the buf goes back to its prior
// "live" state.  Used by the per-realize memory planner to roll back
// speculative pushes at end-of-pass.  No-op if the buf isn't on the
// list (it was already recycled, or never pushed).
fn void cpu_buf_freelist_remove(u32 buf_id) {
  if (CPU_BUFS == NULL) return;
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  for (u32 i = 0; i < CPU_FREELIST_LEN; i++) {
    if (CPU_FREELIST[i] != buf_id) continue;
    CPU_FREELIST[i] = CPU_FREELIST[CPU_FREELIST_LEN - 1];
    CPU_FREELIST_LEN--;
    CPU_BUFS[buf_id].refcount = 1;
    return;
  }
}

fn u32 cpu_buf_refcount(u32 buf_id) {
  if (CPU_BUFS == NULL) return 0;
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return 0;
  return CPU_BUFS[buf_id].refcount;
}
