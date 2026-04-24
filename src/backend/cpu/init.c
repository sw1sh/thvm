// backend/cpu/init.c - lifecycle for the CPU backend + its buffer table.
//
// CPU_BUFS[] is a parallel table to TENS[]: each TenDesc.buf_id indexes
// into it for the actual bytes.  Multiple TenDescs can share the same
// buf_id (view aliasing); CPU_BUFS[id].refcount controls the storage
// lifetime separately from the TenDesc.refcount.

#define CPU_BUFS_CAP (1ULL << 16)

typedef struct {
  void *data;
  u64   nbytes;
  u32   refcount;
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
    if (CPU_BUFS[i].data) free(CPU_BUFS[i].data);
  }
  free(CPU_BUFS);
  CPU_BUFS      = NULL;
  CPU_BUFS_NEXT = 1;
}
