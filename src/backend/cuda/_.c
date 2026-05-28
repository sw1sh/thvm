// backend/cuda/_.c - wire the CUDA Backend vtable + the standalone
// thvm_cuda_* entry points.
//
// Defined last so every cuda_* helper above is in scope.  The single
// instance CUDA_BACKEND mirrors CPU_BACKEND / METAL_BACKEND.  Its
// dispatch_kernel (Stage 3) routes a scheduled KernelEntry through the
// structural-lift CUDA path -- render -> nvrtc -> cuLaunchKernel.  The
// standalone thvm_cuda_* helpers below give a caller the same render +
// run path without the KernelEntry machinery, the way test_metal_real
// drives the Metal backend through thvm_metal_* helpers.

Backend CUDA_BACKEND = {
  .id              = 3,
  .view_aware      = 0,   // CUDA kernels read buffers flat (like Metal)
  .init            = cuda_init,
  .shutdown        = cuda_shutdown,
  .buf_alloc       = cuda_buf_alloc,
  .buf_free        = cuda_buf_free,
  .buf_incref      = cuda_buf_incref,
  .buf_decref      = cuda_buf_decref,
  .buf_jit_pin     = cuda_buf_jit_pin,
  .buf_jit_unpin   = cuda_buf_jit_unpin,
  .buf_read        = cuda_buf_read,
  .buf_write       = cuda_buf_write,
  .buf_copy        = cuda_buf_copy,
  .buf_refcount        = cuda_buf_refcount,
  .buf_freelist_push   = cuda_buf_freelist_push,
  .buf_freelist_remove = cuda_buf_freelist_remove,
  .buf_storage_root    = cuda_buf_storage_root,
  .dispatch_begin  = NULL,
  .dispatch_flush  = NULL,
  .dispatch_end    = NULL,
  .dispatch_kernel = cuda_dispatch_kernel,
};

// === Standalone render + run entry points ============================
// These give a caller (the e2e test today; the Stage-3 py bridge
// tomorrow) the full render -> nvrtc -> alloc -> launch -> read-back
// path without going through the schedule's KernelEntry machinery.

// Lifecycle.  thvm_cuda_available() lets a caller probe whether a CUDA
// device is present before committing to the backend.
fn int  thvm_cuda_init(void)      { return cuda_init(); }
fn void thvm_cuda_shutdown(void)  { cuda_shutdown(); }
fn int  thvm_cuda_available(void) { return CUDA_READY; }

// Render a UOp DAG to a .cu string via the structural CUDA renderer,
// returning a heap buffer the caller frees.  Thin wrapper around
// cg_render_uop_kernel_cuda_root + open_memstream.
fn char *thvm_cuda_render(Term root, const char *kernel_name) {
  char  *buf = NULL;
  size_t sz  = 0;
  FILE  *fp  = open_memstream(&buf, &sz);
  if (fp == NULL) return NULL;
  cg_render_uop_kernel_cuda_root(root, kernel_name, fp);
  fclose(fp);
  return buf;
}

// Compile a .cu source string with nvrtc + load the module; returns
// the resolved CUfunction, or NULL (reason in thvm_cuda_last_error).
fn CUfunction thvm_cuda_compile(const char *cu_src, const char *kernel_name) {
  return cuda_jit_compile(cu_src, kernel_name);
}

// Buffer ops (host-facing).  buf_alloc/write/read/free wrap the vtable
// entries; buf_dptr exposes the raw device pointer so the caller can
// build the cuLaunchKernel argument array.
fn u32  thvm_cuda_buf_alloc(u64 nbytes)                   { return cuda_buf_alloc(nbytes); }
fn int  thvm_cuda_buf_write(u32 b, const void *s, u64 n)  { return cuda_buf_write(b, s, n); }
fn int  thvm_cuda_buf_read (u32 b, void *d, u64 n)        { return cuda_buf_read(b, d, n); }
fn void thvm_cuda_buf_free (u32 b)                        { cuda_buf_free(b); }

// Launch a compiled kernel.  grid_x / block_x are 1-D; `args` is the
// cuLaunchKernel extra-params array (one pointer per kernel arg).
fn int thvm_cuda_launch(CUfunction func, u32 grid_x, u32 block_x,
                        void **args) {
  return cuda_jit_launch(func, grid_x, block_x, args);
}

// DAG-derived launch geometry.  The Python Cuda.dispatch takes an
// explicit flat grid/block and so cannot correctly launch a
// LOCAL-split kernel; the autotune sweep calls this to get the
// tg/tt geometry the structural renderer actually decodes.  Writes
// (grid_x, block_x) and returns 1 on success, 0 if the kernel has no
// lifted DAG / overflows / exceeds the block cap.
fn int thvm_cuda_dag_dispatch_shape(struct KernelEntry const *ke,
                                    u32 *grid_x, u32 *block_x) {
  return cuda_dag_dispatch_shape(ke, grid_x, block_x);
}

// Convenience: build the cuLaunchKernel arg array from a list of
// buffer ids.  The caller still owns the CUdeviceptr storage (it lives
// in CUDA_BUFS) -- `dptr_out` must outlive the launch.  Returns the
// arg-pointer array filled into `args_out` (caller-provided).
fn void thvm_cuda_pack_args(const u32 *buf_ids, u32 n_bufs,
                            CUdeviceptr *dptr_out, void **args_out) {
  for (u32 i = 0; i < n_bufs; i++) {
    dptr_out[i] = cuda_buf_dptr(buf_ids[i]);
    args_out[i] = &dptr_out[i];
  }
}
