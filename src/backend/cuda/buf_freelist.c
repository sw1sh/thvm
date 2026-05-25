// backend/cuda/buf_freelist.c - per-size-class free-list of recyclable
// CUDA buffer slots.  Mirrors backend/cpu/buf_freelist.c.
//
// cuMemAlloc / cuMemFree are far costlier than a host calloc/free, so
// recycling device allocations matters more here than on the CPU
// backend.  A freelist-pushed slot keeps its CUdeviceptr live; the
// next cuda_buf_alloc with a matching nbytes pops it (refcount reset
// to 1) instead of round-tripping the driver.

fn void cuda_buf_freelist_push(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  if (CUDA_FREELIST_LEN >= CUDA_FREELIST_CAP) return;  // saturated; leak to shutdown
  // Defensive: refuse to freelist a buf still referenced by a TenDesc.
  // Without this, a stray push (from a buggy planner decision or a
  // mis-tracked rollback) would orphan the TenDesc: its buf_id still
  // names this slot but the next cuda_buf_alloc(same nbytes) pops it
  // and hands the dptr to a different writer.  The original TenDesc's
  // numpy() read then returns whatever the popper wrote.  Concrete
  // repro pre-fix: beautiful_mnist BS=128 loss read=2.76 before opt
  // step then loss=3.00 after a single opt-step realize -- the loss's
  // 4-byte scalar buf was on the freelist somehow, the opt step's
  // best-fit pop hands its dptr to a small intermediate, intermediate
  // writes 3.00 to it, caller reads.  Mirrors CPU's cpu_buf_freelist's
  // own refcount-aware skip (cpu_buf_freelist_push has owns_data + the
  // mem_plan caller's refcount>1 check; CUDA was the missing twin).
  if (CUDA_BUFS[buf_id].refcount > 0) return;
  CUDA_FREELIST[CUDA_FREELIST_LEN++] = buf_id;
  CUDA_BUFS[buf_id].refcount = 0;
}

fn u32 cuda_buf_freelist_try_pop(u64 nbytes) {
  // Best-fit: reuse the smallest parked device buffer >= nbytes (see
  // cpu_buf_freelist_try_pop -- exact-match barely recycles a net's
  // varied activation sizes, so peak device memory ~= sum-of-activations
  // instead of the live set; best-fit fixes that).  cuMemFree/Alloc are
  // costly so recycling matters even more here.
  u32 best_i = 0; u64 best_nb = (u64)-1;
  for (u32 i = 0; i < CUDA_FREELIST_LEN; i++) {
    u32 bid = CUDA_FREELIST[i];
    if (bid == 0 || bid >= CUDA_BUFS_NEXT) continue;
    CudaBuf *b = &CUDA_BUFS[bid];
    if (b->dptr == 0 || b->nbytes < nbytes) continue;
    if (b->nbytes < best_nb) { best_nb = b->nbytes; best_i = i; }
  }
  if (best_nb == (u64)-1) return 0;
  u32 bid = CUDA_FREELIST[best_i];
  CUDA_FREELIST[best_i] = CUDA_FREELIST[CUDA_FREELIST_LEN - 1];
  CUDA_FREELIST_LEN--;
  cuMemsetD8(CUDA_BUFS[bid].dptr, 0, (size_t)nbytes);
  CUDA_BUFS[bid].refcount = 1;
  return bid;
}

fn void cuda_buf_freelist_remove(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  for (u32 i = 0; i < CUDA_FREELIST_LEN; i++) {
    if (CUDA_FREELIST[i] != buf_id) continue;
    CUDA_FREELIST[i] = CUDA_FREELIST[CUDA_FREELIST_LEN - 1];
    CUDA_FREELIST_LEN--;
    CUDA_BUFS[buf_id].refcount = 1;
    return;
  }
}

fn u32 cuda_buf_refcount(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return 0;
  return CUDA_BUFS[buf_id].refcount;
}
