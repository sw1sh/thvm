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
  CPU_BUFS[id].data = calloc(1, nbytes);
  if (CPU_BUFS[id].data == NULL) {
    fprintf(stderr, "cpu_buf_alloc: calloc(%llu) failed\n", (unsigned long long)nbytes);
    exit(1);
  }
  CPU_BUFS[id].nbytes   = nbytes;
  CPU_BUFS[id].refcount = 1;
  return id;
}
