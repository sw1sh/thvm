// backend/cuda/buf_alloc.c - allocate a device-memory buffer.
//
// Mirrors backend/cpu/buf_alloc.c: returns a non-zero buffer id (0 is
// the "no buffer" sentinel) indexing CUDA_BUFS[].  The fresh path
// calls cuMemAlloc + cuMemsetD8(0) so a new buffer reads as zeros,
// matching cpu_buf_alloc's calloc.

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
  b->dptr     = dptr;
  b->nbytes   = nbytes;
  b->refcount = 1;
  return id;
}

// Expose the raw device pointer for a buffer id.  Used by jit.c when
// packing cuLaunchKernel argument pointers and by the test harness.
fn CUdeviceptr cuda_buf_dptr(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return 0;
  return CUDA_BUFS[buf_id].dptr;
}
