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
  CUDA_FREELIST[CUDA_FREELIST_LEN++] = buf_id;
  CUDA_BUFS[buf_id].refcount = 0;
}

fn u32 cuda_buf_freelist_try_pop(u64 nbytes) {
  for (u32 i = 0; i < CUDA_FREELIST_LEN; i++) {
    u32 bid = CUDA_FREELIST[i];
    if (bid == 0 || bid >= CUDA_BUFS_NEXT) continue;
    CudaBuf *b = &CUDA_BUFS[bid];
    if (b->nbytes != nbytes || b->dptr == 0) continue;
    CUDA_FREELIST[i] = CUDA_FREELIST[CUDA_FREELIST_LEN - 1];
    CUDA_FREELIST_LEN--;
    // Zero the recycled storage so it matches a fresh cuMemAlloc that
    // cuda_buf_alloc clears.
    cuMemsetD8(b->dptr, 0, (size_t)nbytes);
    b->refcount = 1;
    return bid;
  }
  return 0;
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
