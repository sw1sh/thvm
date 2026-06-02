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
  if (CUDA_BUFS[buf_id].jit_pinned) return;            // held by JIT capture
  if (CUDA_FREELIST_LEN >= CUDA_FREELIST_CAP) return;  // saturated; leak to shutdown
  // Non-owning bufs (arena views, future external imports) borrow
  // another slot's dptr; recycling them through the freelist would
  // hand the same dptr to a fresh alloc while the parent arena still
  // backs it.  Mirrors backend/cpu/buf_freelist.c's owns_data guard.
  if (!CUDA_BUFS[buf_id].owns_data) return;
  if (CUDA_BUFS[buf_id].dptr == 0) return;             // already dead
  // Note: we used to reject refcount > 0 here as a defense against the
  // following hazard: a stray push would orphan a still-live TenDesc,
  // and the next cuda_buf_alloc(same nbytes) would best-fit-pop the
  // slot and hand its dptr to a fresh writer, so the original TenDesc's
  // numpy() read returned whatever the popper wrote.  The concrete
  // pre-fix repro was beautiful_mnist BS=128: loss read=2.76 before opt
  // step then loss=3.00 after a single opt-step realize -- the loss's
  // 4-byte scalar buf was on the freelist somehow and a small
  // intermediate stole its dptr.  But the guard made the freelist
  // unreachable: the only producer (cuda_buf_pool_rollback_with_preserve)
  // pushes refcount>0 bufs.  Net effect: every cuMemAlloc was fresh
  // (~92/step on beautiful_mnist BS=128, ~100us each).  The hazard is
  // now neutralized by cuda_buf_freelist_try_pop's slot identity swap:
  // the donor slot is zeroed (dptr=0) and a FRESH slot id wraps the
  // recycled dptr.  Any stale TenDesc reading the donor sees dptr==0
  // (dead -> per-fire re-alloc in kernel_fire_by_id), not aliased
  // bytes.  CPU's cpu_buf_freelist_push has no refcount guard either.
  CUDA_FREELIST[CUDA_FREELIST_LEN++] = buf_id;
  CUDA_BUFS[buf_id].refcount  = 0;
  CUDA_BUFS[buf_id].preserved = 0;
}

fn u32 cuda_buf_freelist_try_pop(u64 nbytes) {
  // Best-fit: reuse the smallest parked device buffer >= nbytes (see
  // cpu_buf_freelist_try_pop -- exact-match barely recycles a net's
  // varied activation sizes, so peak device memory ~= sum-of-activations
  // instead of the live set; best-fit fixes that).  cuMemFree/Alloc are
  // costly so recycling matters even more here.
  //
  // Slot identity: a recycled storage block gets handed to a FRESH
  // CUDA_BUFS slot id.  The donor slot is left with dptr=0 (dead).  This
  // matters because the planner pushes a buf to the freelist while the
  // OWNING TenDesc still holds buf_id == donor.  Without the slot swap,
  // a same-pass alloc that best-fit-pops the donor would alias two
  // TenDescs onto the same slot id; the next realize's kernel re-fire
  // would write to the donor's TenDesc.buf_id and overwrite the new
  // tensor stored there.  Handing the storage to a new slot keeps each
  // TenDesc.buf_id pointing at distinct logical buffers; the donor's
  // dptr==0 then drives the per-fire re-alloc in kernel_fire_by_id.
  // Tinygrad parity: their schedule/memory.py + runtime Buffer object
  // separates logical Buffer identity from underlying storage; recycle
  // creates a new Buffer wrapping reused bytes.
  u32 best_i = 0; u64 best_nb = (u64)-1;
  for (u32 i = 0; i < CUDA_FREELIST_LEN; i++) {
    u32 bid = CUDA_FREELIST[i];
    if (bid == 0 || bid >= CUDA_BUFS_NEXT) continue;
    CudaBuf *b = &CUDA_BUFS[bid];
    if (b->dptr == 0 || b->nbytes < nbytes) continue;
    if (!b->owns_data) continue;       // skip arena views / externals
    if (b->nbytes < best_nb) { best_nb = b->nbytes; best_i = i; }
  }
  if (best_nb == (u64)-1) return 0;
  u32 donor_bid = CUDA_FREELIST[best_i];
  CUDA_FREELIST[best_i] = CUDA_FREELIST[CUDA_FREELIST_LEN - 1];
  CUDA_FREELIST_LEN--;
  // Transfer storage from donor slot to a fresh slot.  Donor goes dead.
  CudaBuf *donor = &CUDA_BUFS[donor_bid];
  CUdeviceptr dptr = donor->dptr;
  u64         nb   = donor->nbytes;
  donor->dptr      = 0;
  donor->nbytes    = 0;
  donor->refcount  = 0;
  donor->preserved = 0;
  donor->owns_data = 0;
  donor->skip_freelist = 0;
  donor->parent_buf_id = 0;
  if (CUDA_BUFS_NEXT >= CUDA_BUFS_CAP) {
    // Out of slot table -- restore donor and bail; caller will fall back
    // to cuMemAlloc (cost: an extra fresh allocation, never aliasing).
    donor->dptr      = dptr;
    donor->nbytes    = nb;
    donor->refcount  = 0;
    donor->owns_data = 1;
    cuda_buf_freelist_push(donor_bid);
    return 0;
  }
  u32 new_id = (u32)CUDA_BUFS_NEXT++;
  CudaBuf *nb_slot = &CUDA_BUFS[new_id];
  nb_slot->dptr          = dptr;
  nb_slot->nbytes        = nb;
  nb_slot->refcount      = 1;
  nb_slot->preserved     = 0;
  nb_slot->owns_data     = 1;
  nb_slot->skip_freelist = 0;
  nb_slot->parent_buf_id = 0;
  cuMemsetD8(dptr, 0, (size_t)nbytes);
  if (getenv("THVM_CUDA_ALLOC_TRACE")) {
    fprintf(stderr,
            "[freelist] req=%llu -> donor_bid=%u (nbytes=%llu) -> new buf_id=%u dptr=%p\n",
            (unsigned long long)nbytes, donor_bid,
            (unsigned long long)nb, new_id, (void*)dptr);
  }
  return new_id;
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
