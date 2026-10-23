// backend/cpu/_.c - the CPU Backend vtable, defined after every cpu_* helper
// it names.

Backend CPU_BACKEND = {
  .id              = 1,
  .view_aware      = 1,   // every rung reads a strided input view in place
  .init            = cpu_init,
  .shutdown        = cpu_shutdown,
  .buf_alloc       = cpu_buf_alloc,
  .buf_free        = cpu_buf_free,
  .buf_incref      = cpu_buf_incref,
  .buf_decref      = cpu_buf_decref,
  .buf_jit_pin     = cpu_buf_jit_pin,
  .buf_jit_unpin   = cpu_buf_jit_unpin,
  .buf_read        = cpu_buf_read,
  .buf_write       = cpu_buf_write,
  .buf_copy        = cpu_buf_copy,
  .buf_write_at    = cpu_buf_write_at,
  .buf_copy_at     = cpu_buf_copy_at,
  .buf_refcount        = cpu_buf_refcount,
  .buf_freelist_push   = cpu_buf_freelist_push,
  .buf_freelist_remove = cpu_buf_freelist_remove,
  .buf_nbytes          = cpu_buf_nbytes,
  .dispatch_begin  = NULL,
  .dispatch_flush  = NULL,
  .dispatch_end    = NULL,
  .dispatch_kernel = cpu_dispatch_kernel,
};
