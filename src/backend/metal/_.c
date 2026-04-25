// backend/metal/_.c - Metal backend STUB.
//
// Placeholder while the real Objective-C backend (per docs/metal.md)
// gets built up kernel-by-kernel.  All entries return error sentinels
// so a runtime that gets switched here via `THVM_BACKEND=metal`
// fails LOUDLY at any compute-touching call -- much better than
// silently returning zeros.
//
// init() succeeds (no resources to allocate yet); shutdown() is a
// no-op.  buf_alloc returns 0 (the "no buffer" sentinel reserved by
// the CPU backend convention), so any subsequent buf_read/write/
// dispatch hits the error paths.
//
// Once src/backend/metal/init.m + the per-op .m files arrive, this
// file gets replaced with the .m glue (and CPU_BACKEND remains the
// other choice).

static int  metal_init(void)                                    { return 0; }
static void metal_shutdown(void)                                { /* nop */ }

static u32  metal_buf_alloc(u64 nbytes)                         { (void)nbytes;     return 0; }
static void metal_buf_free (u32 buf_id)                         { (void)buf_id;     /* nop */ }
static void metal_buf_incref(u32 buf_id)                        { (void)buf_id;     /* nop */ }
static void metal_buf_decref(u32 buf_id)                        { (void)buf_id;     /* nop */ }
static int  metal_buf_read (u32 buf_id, void *dst, u64 nbytes)  { (void)buf_id; (void)dst; (void)nbytes; return -1; }
static int  metal_buf_write(u32 buf_id, const void *src, u64 nbytes) { (void)buf_id; (void)src; (void)nbytes; return -1; }

static int  metal_dispatch_kernel(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  (void)ke; (void)in_buf_ids; (void)out_buf_id;
  fprintf(stderr, "thvm: metal backend stub -- dispatch_kernel not implemented\n");
  return -1;
}

Backend METAL_BACKEND = {
  .id              = 2,
  .init            = metal_init,
  .shutdown        = metal_shutdown,
  .buf_alloc       = metal_buf_alloc,
  .buf_free        = metal_buf_free,
  .buf_incref      = metal_buf_incref,
  .buf_decref      = metal_buf_decref,
  .buf_read        = metal_buf_read,
  .buf_write       = metal_buf_write,
  .dispatch_kernel = metal_dispatch_kernel,
};
