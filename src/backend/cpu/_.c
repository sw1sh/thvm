// backend/cpu/_.c - wire the CPU Backend vtable.
//
// Defined here so every cpu_* helper above is already in scope.  The
// single instance CPU_BACKEND is installed as CURRENT_BACKEND by
// thvm_init.

Backend CPU_BACKEND = {
  .id              = 1,
  .view_aware      = 1,   // cpu_interpret pre-materializes non-contig inputs
  .init            = cpu_init,
  .shutdown        = cpu_shutdown,
  .buf_alloc       = cpu_buf_alloc,
  .buf_free        = cpu_buf_free,
  .buf_incref      = cpu_buf_incref,
  .buf_decref      = cpu_buf_decref,
  .buf_read        = cpu_buf_read,
  .buf_write       = cpu_buf_write,
  .buf_copy        = cpu_buf_copy,
  .dispatch_begin  = NULL,
  .dispatch_flush  = NULL,
  .dispatch_end    = NULL,
  .dispatch_kernel = cpu_dispatch_kernel,
};
