// backend/cpu/buf_freelist.c - parked CPU buffers for reuse.  A pushed slot is
// dead but keeps its storage until cpu_buf_alloc pops the block into a fresh
// slot id; finer-grained than the per-realize watermark pool (buf_pool.c).

fn void cpu_buf_freelist_push(u32 buf_id) {
  if (CPU_BUFS == NULL) return;
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  if (CPU_BUFS[buf_id].jit_pinned) return;   // held by an active JIT capture
  if (CPU_FREELIST_LEN >= CPU_FREELIST_CAP) {
    // Full: the slot stays live and its storage is held until shutdown.
    return;
  }
  CPU_FREELIST[CPU_FREELIST_LEN++] = buf_id;
  // Refcount 0 keeps a parked slot out of the live-byte probes.  try_pop never
  // recycles an external (an arena view); cpu_buf_free drops its parent.
  CpuBuf *b = &CPU_BUFS[buf_id];
  b->refcount  = 0;
  b->preserved = 0;
  b->freeable  = 0;
}

fn u32 cpu_buf_freelist_try_pop(u64 nbytes) {
  if (CPU_BUFS == NULL) return 0;
  // With no slot id left for the block, miss before touching the donor; the
  // caller's fresh-alloc path then reports the full table.
  if (CPU_BUFS_NEXT >= CPU_BUFS_CAP) {
    return 0;
  }
  // Best fit: the smallest parked block of at least nbytes, so the live set,
  // not the sum of all activations, bounds memory.
  u32 best = 0; u64 best_nb = (u64)-1;
  for (u32 i = 0; i < CPU_FREELIST_LEN; i++) {
    u32 bid = CPU_FREELIST[i];
    if (bid == 0 || bid >= CPU_BUFS_NEXT) continue;
    CpuBuf *b = &CPU_BUFS[bid];
    if (!b->owns_data || b->data == NULL) continue;  // skip externals
    if (b->nbytes < nbytes) continue;
    if (b->nbytes < best_nb) { best_nb = b->nbytes; best = i; }
  }
  if (best_nb == (u64)-1) return 0;
  // The block moves to a fresh slot id: the donor's TenDesc may still name the
  // old one, and a re-fire of its producer would overwrite the new tensor.
  u32 donor_bid = CPU_FREELIST[best];
  CPU_FREELIST[best] = CPU_FREELIST[CPU_FREELIST_LEN - 1];
  CPU_FREELIST_LEN--;
  CpuBuf *donor = &CPU_BUFS[donor_bid];
  void *data = donor->data;
  u64   nb   = donor->nbytes;
  donor->data       = NULL;
  donor->nbytes     = 0;
  donor->req_nbytes = 0;
  donor->refcount   = 0;
  donor->preserved  = 0;
  donor->freeable   = 0;
  donor->owns_data  = 0;
  donor->jit_pinned = 0;
  donor->handle     = NULL;
  donor->on_release = NULL;
  u32 new_id = (u32)CPU_BUFS_NEXT++;
  CpuBuf *nb_slot = &CPU_BUFS[new_id];
  nb_slot->data       = data;
  nb_slot->nbytes     = nb;
  nb_slot->req_nbytes = nbytes;
  nb_slot->refcount   = 1;
  nb_slot->preserved  = 0;
  nb_slot->freeable   = 0;
  nb_slot->owns_data  = 1;
  nb_slot->jit_pinned = 0;
  nb_slot->handle     = NULL;
  nb_slot->on_release = NULL;
  memset(data, 0, (size_t)nbytes);
  return new_id;
}

// Undo a cpu_buf_freelist_push whose block no pop took: the memory planner
// rolls back its speculative pushes at the end of a pass.
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
