// backend/cpu/init.c - lifecycle for the CPU backend + its buffer table.
//
// CPU_BUFS[] is a parallel table to TENS[]: each TenDesc.buf_id indexes
// into it for the actual bytes.  Multiple TenDescs can share the same
// buf_id (view aliasing); CPU_BUFS[id].refcount controls the storage
// lifetime separately from the TenDesc.refcount.
//
// A buffer can be "owned" (we malloc'd it; free on release) or
// "external" (the bytes belong to someone else; we call a per-buffer
// on_release callback).  External buffers let the WL bridge construct
// a tensor over a NumericArray's bytes without copying, holding the
// NumericArray alive for the tensor's lifetime.

#define CPU_BUFS_CAP (1ULL << 16)

typedef struct {
  void *data;
  u64   nbytes;
  u32   refcount;
  u8    owns_data;                  // 1 = free(data) on release; 0 = call on_release
  u8    preserved;                  // 1 = pool_rollback_with_preserve skips
                                    // (set by mark_preserved_buf during the
                                    // result-chain walk; cleared by
                                    // pool_clear_preserved after rollback)
  u8    freeable;                   // 1 = refcount-driven free has detected
                                    // that this buf's last consumer has read
                                    // it; pool_rollback_freeable will free.
                                    // Set by cpu_buf_mark_freeable from the
                                    // decref hook in kernel_fire_by_id.
  void *handle;                     // opaque, passed to on_release
  void (*on_release)(void *handle); // cleanup for !owns_data buffers
} CpuBuf;

static CpuBuf *CPU_BUFS      = NULL;
static u64     CPU_BUFS_NEXT = 1;   // start at 1; 0 reserved for "no buffer"

fn int cpu_init(void) {
  CPU_BUFS = (CpuBuf *)calloc(CPU_BUFS_CAP, sizeof(CpuBuf));
  CPU_BUFS_NEXT = 1;
  return CPU_BUFS == NULL ? -1 : 0;
}

fn void cpu_shutdown(void) {
  if (CPU_BUFS == NULL) return;
  for (u64 i = 1; i < CPU_BUFS_NEXT; i++) {
    CpuBuf *b = &CPU_BUFS[i];
    if (b->refcount == 0) continue;
    if (b->owns_data) {
      if (b->data) free(b->data);
    } else if (b->on_release) {
      b->on_release(b->handle);
    }
  }
  free(CPU_BUFS);
  CPU_BUFS      = NULL;
  CPU_BUFS_NEXT = 1;
}
