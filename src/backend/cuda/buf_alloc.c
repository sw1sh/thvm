// backend/cuda/buf_alloc.c - allocate a device-memory buffer.
//
// Mirrors backend/cpu/buf_alloc.c: returns a non-zero buffer id (0 is
// the "no buffer" sentinel) indexing CUDA_BUFS[].  The fresh path
// calls cuMemAlloc + cuMemsetD8(0) so a new buffer reads as zeros,
// matching cpu_buf_alloc's calloc.

// Process-global resident device-buffer-byte accounting (mirrors the
// CPU counters in backend/cpu/buf_alloc.c).  Incremented only on a fresh
// cuMemAlloc and decremented only on a real cuMemFree; freelist reuse is
// invisible (recycled storage was already counted and never freed while
// parked).
u64 CUDA_MEM_LIVE = 0;
u64 CUDA_MEM_PEAK = 0;

fn u64 cuda_buf_peak_bytes(void) { return CUDA_MEM_PEAK; }
fn u64 cuda_buf_live_bytes(void) { return CUDA_MEM_LIVE; }
fn void cuda_buf_peak_reset(void) { CUDA_MEM_PEAK = CUDA_MEM_LIVE; }

fn u32 cuda_buf_alloc(u64 nbytes) {
  if (!CUDA_READY) {
    fprintf(stderr, "cuda_buf_alloc: backend not initialised\n");
    return 0;
  }
  u64 ceiling = thvm_buf_byte_ceiling();
  if (ceiling != 0 && nbytes > ceiling) {
    fprintf(stderr,
      "cuda_buf_alloc: refusing %llu-byte allocation (> THVM_MAX_BUF_BYTES "
      "ceiling %llu)\n",
      (unsigned long long)nbytes, (unsigned long long)ceiling);
    return 0;
  }
  if (nbytes == 0) nbytes = 1;   // cuMemAlloc rejects 0

  u32 recycled = cuda_buf_freelist_try_pop(nbytes);
  if (recycled != 0) return recycled;

  if (CUDA_BUFS_NEXT >= CUDA_BUFS_CAP) {
    fprintf(stderr, "cuda_buf_alloc: out of slots (cap=%u)\n", CUDA_BUFS_CAP);
    return 0;
  }
  CUdeviceptr dptr = 0;
  CUresult r = cuMemAlloc(&dptr, (size_t)nbytes);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuMemAlloc", r);
    fprintf(stderr, "cuda_buf_alloc: %s (%llu bytes)\n",
            CUDA_LAST_ERROR, (unsigned long long)nbytes);
    return 0;
  }
  cuMemsetD8(dptr, 0, (size_t)nbytes);
  u32 id = (u32)CUDA_BUFS_NEXT++;
  CudaBuf *b = &CUDA_BUFS[id];
  b->dptr          = dptr;
  b->nbytes        = nbytes;
  b->refcount      = 1;
  b->preserved     = 0;
  b->owns_data     = 1;
  b->skip_freelist = 0;
  b->parent_buf_id = 0;
  CUDA_MEM_LIVE += nbytes;
  if (CUDA_MEM_LIVE > CUDA_MEM_PEAK) CUDA_MEM_PEAK = CUDA_MEM_LIVE;
  return id;
}

// Borrow an existing device pointer for the lifetime of the returned
// buf_id.  No cuMemAlloc, no cuMemFree -- the caller owns `dptr`.
// Mirror of backend/cpu/buf_alloc.c::cpu_buf_alloc_external; today the
// only caller is cuda_buf_alloc_arena_view, but having the symbol
// available keeps the CPU/CUDA shape identical and admits a future
// py-bridge device-pointer import path.
fn u32 cuda_buf_alloc_external(CUdeviceptr dptr, u64 nbytes) {
  if (!CUDA_READY) {
    fprintf(stderr, "cuda_buf_alloc_external: backend not initialised\n");
    return 0;
  }
  if (CUDA_BUFS_NEXT >= CUDA_BUFS_CAP) {
    fprintf(stderr, "cuda_buf_alloc_external: out of slots\n");
    return 0;
  }
  u32 id = (u32)CUDA_BUFS_NEXT++;
  CudaBuf *b = &CUDA_BUFS[id];
  b->dptr          = dptr;
  b->nbytes        = nbytes;
  b->refcount      = 1;
  b->preserved     = 0;
  b->owns_data     = 0;
  b->skip_freelist = 0;
  b->parent_buf_id = 0;
  return id;
}

// Arena view: external buf at (parent's dptr + offset, nbytes) whose
// lifetime is tied to `parent_buf_id` (the arena CudaBuf).  Each incref
// of this view increments the parent; each decref-to-zero decrements
// the parent and frees the view cell, but leaves the parent's bytes
// alive until the parent's own refcount drops to zero.  Mirror:
// tinygrad schedule/memory.py:60 emits BUFFER_VIEW(arena, nbytes,
// offset) whose arena edge keeps the underlying buffer alive while any
// view persists.  Matches backend/cpu/buf_alloc.c:93-100.
fn u32 cuda_buf_alloc_arena_view(CUdeviceptr dptr, u64 nbytes,
                                 u32 parent_buf_id) {
  u32 id = cuda_buf_alloc_external(dptr, nbytes);
  if (id != 0 && parent_buf_id != 0 && parent_buf_id < CUDA_BUFS_NEXT) {
    CUDA_BUFS[id].parent_buf_id = parent_buf_id;
    CUDA_BUFS[parent_buf_id].refcount++;
  }
  return id;
}

// Expose the raw device pointer for a buffer id.  Used by jit.c when
// packing cuLaunchKernel argument pointers and by the test harness.
fn CUdeviceptr cuda_buf_dptr(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return 0;
  return CUDA_BUFS[buf_id].dptr;
}
