// backend/cpu/buf_alloc.c - allocate a host-memory buffer.
//
// Returns a non-zero buffer id on success (0 reserved for "no buffer").
// The id indexes into a parallel CPU_BUFS[] table defined alongside the
// backend init code, which pairs each allocation with a refcount so the
// same buffer can be shared across multiple TenDesc aliases.

// Process-global resident buffer-byte accounting (device-agnostic
// memory metric, the analog of tinygrad's GlobalCounters live mem).
// Incremented only on a fresh calloc and decremented only on a real
// free(); free-list reuse is invisible because the recycled storage was
// already counted at its original calloc and never freed while parked.
u64 CPU_MEM_LIVE = 0;
u64 CPU_MEM_PEAK = 0;

fn u64 cpu_buf_peak_bytes(void) { return CPU_MEM_PEAK; }
fn u64 cpu_buf_live_bytes(void) { return CPU_MEM_LIVE; }
fn void cpu_buf_peak_reset(void) { CPU_MEM_PEAK = CPU_MEM_LIVE; }

fn u32 cpu_buf_alloc(u64 nbytes) {
  // Refuse a pathologically large single allocation rather than
  // calloc() it and thrash the host.  See thvm_buf_byte_ceiling.
  u64 ceiling = thvm_buf_byte_ceiling();
  if (ceiling != 0 && nbytes > ceiling) {
    fprintf(stderr,
      "cpu_buf_alloc: refusing %llu-byte allocation (> THVM_MAX_BUF_BYTES "
      "ceiling %llu); a kernel program is asking for a buffer far larger "
      "than any legitimate tensor -- likely an unfused im2col/EXPAND "
      "intermediate.  Raise THVM_MAX_BUF_BYTES (bytes; 0 = unlimited) if "
      "this is intentional.\n",
      (unsigned long long)nbytes, (unsigned long long)ceiling);
    exit(1);
  }
  // bm4a: free-list lookup first.  Recycles a slot whose nbytes
  // matches; reset to a clean state by cpu_buf_freelist_try_pop.
  // No-op when the list is empty or no size match -- falls
  // through to the fresh-calloc path below.
  u32 recycled = cpu_buf_freelist_try_pop(nbytes);
  if (recycled != 0) return recycled;

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
  b->skip_freelist = 0;
  b->parent_buf_id = 0;
  b->handle     = NULL;
  b->on_release = NULL;
  CPU_MEM_LIVE += nbytes;
  if (CPU_MEM_LIVE > CPU_MEM_PEAK) CPU_MEM_PEAK = CPU_MEM_LIVE;
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
  b->parent_buf_id = 0;
  return id;
}

// Allocate an arena view: external buf at (data, nbytes) whose lifetime
// is tied to `parent_buf_id` (the arena CpuBuf).  Each incref of this
// view increments the parent; each decref-to-zero decrements the parent
// and frees the view cell, but leaves the parent's bytes alive until
// the parent's own refcount drops to zero.  Mirror: tinygrad
// schedule/memory.py:60 emits BUFFER_VIEW(arena, nbytes, offset) whose
// arena edge keeps the underlying buffer alive while any view persists.
fn u32 cpu_buf_alloc_arena_view(void *data, u64 nbytes, u32 parent_buf_id) {
  u32 id = cpu_buf_alloc_external(data, nbytes, NULL, NULL);
  if (id != 0 && parent_buf_id != 0 && parent_buf_id < CPU_BUFS_NEXT) {
    CPU_BUFS[id].parent_buf_id = parent_buf_id;
    CPU_BUFS[parent_buf_id].refcount++;
  }
  return id;
}
