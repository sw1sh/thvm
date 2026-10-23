// backend/cpu/buf_alloc.c - allocate a host-memory buffer; id 0 means none.

// Resident buffer bytes (tinygrad helpers.py GlobalCounters.mem_used).  Parked
// freelist storage stays counted, so only a fresh calloc or a real free moves it.
u64 CPU_MEM_LIVE = 0;
u64 CPU_MEM_PEAK = 0;

fn u64 cpu_buf_peak_bytes(void) { return CPU_MEM_PEAK; }
fn u64 cpu_buf_live_bytes(void) { return CPU_MEM_LIVE; }
fn void cpu_buf_peak_reset(void) { CPU_MEM_PEAK = CPU_MEM_LIVE; }

// Set around arena_ensure's alloc: the per-realize arena is the memory
// planner's working set (tinygrad schedule/memory.py), legitimately multi-GB.
int CPU_ARENA_ALLOC_INFLIGHT = 0;
void cpu_arena_alloc_inflight_set(int on) { CPU_ARENA_ALLOC_INFLIGHT = on; }
int  cpu_arena_alloc_inflight(void) { return CPU_ARENA_ALLOC_INFLIGHT; }

fn u32 cpu_buf_alloc(u64 nbytes) {
  // The per-buffer ceiling catches a runaway im2col or EXPAND before calloc
  // thrashes the host; the arena answers to the total-live ceiling instead.
  if (CPU_ARENA_ALLOC_INFLIGHT) {
    u64 live_ceiling = thvm_live_byte_ceiling();
    if (live_ceiling != 0 && CPU_MEM_LIVE + nbytes > live_ceiling) {
      fprintf(stderr,
        "cpu_buf_alloc: refusing %llu-byte ARENA allocation (live %llu + req > "
        "THVM_MAX_LIVE_BYTES ceiling %llu); the per-realize working set exceeds "
        "the total-live budget.  Raise THVM_MAX_LIVE_BYTES (bytes; 0 = "
        "unlimited) if this is intentional.\n",
        (unsigned long long)nbytes, (unsigned long long)CPU_MEM_LIVE,
        (unsigned long long)live_ceiling);
      exit(1);
    }
  } else {
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
  }
  u32 recycled = cpu_buf_freelist_try_pop(nbytes);
  if (recycled != 0) {
    return recycled;
  }

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
  b->req_nbytes = nbytes;
  b->refcount   = 1;
  b->owns_data  = 1;
  b->skip_freelist = 0;
  b->jit_pinned = 0;
  b->parent_buf_id = 0;
  b->handle     = NULL;
  b->on_release = NULL;
  CPU_MEM_LIVE += nbytes;
  if (CPU_MEM_LIVE > CPU_MEM_PEAK) CPU_MEM_PEAK = CPU_MEM_LIVE;
  return id;
}

// Logical size of buf_id (Backend.buf_nbytes): the bytes its allocation
// requested, which a recycled block's nbytes can exceed.
fn u64 cpu_buf_nbytes(u32 buf_id) {
  if (CPU_BUFS == NULL || buf_id == 0 || buf_id >= CPU_BUFS_NEXT) {
    return 0;
  }
  return CPU_BUFS[buf_id].req_nbytes;
}

// Wrap caller-owned bytes (a WL Shared NumericArray) without copying; the
// last decref calls on_release(handle) instead of free().
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
  b->req_nbytes = nbytes;
  b->refcount   = 1;
  b->owns_data  = 0;
  b->skip_freelist = 0;
  b->jit_pinned = 0;
  b->handle     = handle;
  b->on_release = on_release;
  b->parent_buf_id = 0;
  return id;
}

// An external view into the arena parent_buf_id that holds one parent ref
// until the view is freed.  tinygrad schedule/memory.py:59 (arena SLICE).
fn u32 cpu_buf_alloc_arena_view(void *data, u64 nbytes, u32 parent_buf_id) {
  u32 id = cpu_buf_alloc_external(data, nbytes, NULL, NULL);
  if (id != 0 && parent_buf_id != 0 && parent_buf_id < CPU_BUFS_NEXT) {
    CPU_BUFS[id].parent_buf_id = parent_buf_id;
    CPU_BUFS[parent_buf_id].refcount++;
  }
  return id;
}
