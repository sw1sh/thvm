// backend/cpu/buf_alloc.c - allocate a host-memory buffer.
//
// Returns a non-zero buffer id on success (0 reserved for "no buffer").
// The id indexes into a parallel CPU_BUFS[] table defined alongside the
// backend init code, which pairs each allocation with a refcount so the
// same buffer can be shared across multiple TenDesc aliases.

fn u32 cpu_buf_alloc(u64 nbytes) {
  if (CPU_BUFS_NEXT >= CPU_BUFS_CAP) {
    fprintf(stderr, "cpu_buf_alloc: out of slots (cap=%llu)\n", (unsigned long long)CPU_BUFS_CAP);
    exit(1);
  }
  u32 id = (u32)CPU_BUFS_NEXT++;
  CpuBuf *b = &CPU_BUFS[id];
  b->data = calloc(1, nbytes);
  if (b->data == NULL) {
    fprintf(stderr, "cpu_buf_alloc: calloc(%llu) failed\n", (unsigned long long)nbytes);
    exit(1);
  }
  b->nbytes     = nbytes;
  b->refcount   = 1;
  b->owns_data  = 1;
  b->handle     = NULL;
  b->on_release = NULL;
  return id;
}

// Borrow an existing buffer.  The caller owns `data` for the lifetime
// of the returned buf_id; when the refcount hits zero we invoke
// `on_release(handle)` instead of free().  Used by the WL bridge to
// wrap a Shared NumericArray's bytes without copying.
fn u32 cpu_buf_alloc_external(void *data, u64 nbytes,
                              void (*on_release)(void *), void *handle) {
  if (CPU_BUFS_NEXT >= CPU_BUFS_CAP) {
    fprintf(stderr, "cpu_buf_alloc_external: out of slots\n");
    exit(1);
  }
  u32 id = (u32)CPU_BUFS_NEXT++;
  CpuBuf *b = &CPU_BUFS[id];
  b->data       = data;
  b->nbytes     = nbytes;
  b->refcount   = 1;
  b->owns_data  = 0;
  b->handle     = handle;
  b->on_release = on_release;
  return id;
}
