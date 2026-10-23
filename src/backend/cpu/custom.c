// backend/cpu/custom.c - compile and dispatch a raw injected kernel
// (KernelEntry.custom_program) with the CPU JIT's compiler, caching the entry
// point by content hash.  The ABI is documented in schedule/kernel_custom.c.

// bufs[i] is argument slot i's buffer; grid and block are advisory on CPU,
// where the kernel loops itself.
typedef void (*CpuCustomFn)(void *const *bufs, unsigned const *grid,
                            unsigned const *block);

// Defined in schedule/kernel_custom.c, included later in thvm.c.
fn u32 kernel_custom_primary_out_slot(KernelEntry const *ke);

#define CPU_CUSTOM_CACHE_CAP 64
typedef struct {
  u64          key;        // 0 = empty
  CpuCustomFn  func;
  void        *dl_handle;
} CpuCustomSlot;
static CpuCustomSlot CPU_CUSTOM_CACHE[CPU_CUSTOM_CACHE_CAP];

fn void cpu_custom_cache_reset(void) {
  for (u32 i = 0; i < CPU_CUSTOM_CACHE_CAP; i++) {
    if (CPU_CUSTOM_CACHE[i].dl_handle != NULL) {
      dlclose(CPU_CUSTOM_CACHE[i].dl_handle);
    }
    CPU_CUSTOM_CACHE[i].key       = 0;
    CPU_CUSTOM_CACHE[i].func      = NULL;
    CPU_CUSTOM_CACHE[i].dl_handle = NULL;
  }
}

static CpuCustomSlot *cpu_custom_lookup_slot(u64 key) {
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < CPU_CUSTOM_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (CPU_CUSTOM_CACHE_CAP - 1);
    if (CPU_CUSTOM_CACHE[i].key == key) return &CPU_CUSTOM_CACHE[i];
    if (CPU_CUSTOM_CACHE[i].key == 0)   return &CPU_CUSTOM_CACHE[i];
  }
  return NULL;
}

// The source hash mixed with the entry name: one source body may export
// several entry points.
static u64 cpu_custom_key(KernelEntry const *ke) {
  u64 h = cpu_src_hash(ke->custom_program);
  for (char const *p = ke->custom_name; *p; p++) {
    h ^= (u64)(unsigned char)*p;
    h *= 0x100000001b3ULL;
  }
  return h | (1ULL << 63);
}

// cpu_jit_build's flow with the custom entry name in place of the
// renderer's "k".
static CpuCustomFn cpu_custom_build(KernelEntry const *ke, u64 key) {
  char src_path[256], dl_path[256];
  snprintf(src_path, sizeof src_path, "/tmp/thvm_custom_%016llx.c",
           (unsigned long long)key);
  snprintf(dl_path,  sizeof dl_path,  "/tmp/thvm_custom_%016llx_%s.dylib",
           (unsigned long long)key, cpu_jit_opt_tag());

  struct stat st;
  if (stat(dl_path, &st) != 0) {
    FILE *f = fopen(src_path, "w");
    if (f == NULL) return NULL;
    fputs(ke->custom_program, f);
    fclose(f);
    if (!cpu_jit_compile_shared(src_path, dl_path, 1)) {
      fprintf(stderr, "thvm: cpu_custom_build: %s failed for custom "
                      "kernel '%s' (source at %s)\n", cpu_cc(), ke->custom_name,
              src_path);
      return NULL;
    }
  }

  void       *h   = NULL;
  CpuCustomFn jfn = (CpuCustomFn)cpu_jit_load(dl_path, ke->custom_name, &h);
  if (jfn == NULL) {
    fprintf(stderr, "thvm: cpu_custom_build: could not load entry point '%s' "
                    "from %s\n", ke->custom_name, dl_path);
    return NULL;
  }
  CpuCustomSlot *s = cpu_custom_lookup_slot(key);
  if (s != NULL) { s->key = key; s->func = jfn; s->dl_handle = h; }
  return jfn;
}

// out_buf_id overrides the primary out slot: at JIT replay it is the rebound or
// planned buffer, while the mirrored input slot may name the capture-time one.
fn int cpu_custom_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  u64 t0  = cg_now_us();
  u32 kid = (u32)(ke - KERNELS);
  u64 key = cpu_custom_key(ke);
  CpuCustomSlot *s = cpu_custom_lookup_slot(key);
  CpuCustomFn fn_ptr = (s != NULL && s->key == key) ? s->func : NULL;
  if (fn_ptr == NULL) fn_ptr = cpu_custom_build(ke, key);
  if (fn_ptr == NULL) return -1;

  u32 n = ke->n_inputs;
  void *bufs[n ? n : 1];
  for (u32 i = 0; i < n; i++) {
    u32 ib = in_buf_ids[i];
    if (ib == 0 || ib >= CPU_BUFS_NEXT || CPU_BUFS[ib].data == NULL) {
      fprintf(stderr, "thvm: cpu_custom_dispatch: kid=%u arg %u buf %u has "
                      "no storage\n", kid, i, ib);
      return -1;
    }
    bufs[i] = CPU_BUFS[ib].data;
  }
  u32 out_slot = kernel_custom_primary_out_slot(ke);
  if (out_buf_id != 0 && out_slot < n) {
    if (out_buf_id >= CPU_BUFS_NEXT || CPU_BUFS[out_buf_id].data == NULL) {
      fprintf(stderr, "thvm: cpu_custom_dispatch: kid=%u out buf %u has no "
                      "storage\n", kid, out_buf_id);
      return -1;
    }
    bufs[out_slot] = CPU_BUFS[out_buf_id].data;
  }

  fn_ptr((void *const *)bufs, ke->custom_grid, ke->custom_block);
  cg_profile_record(kid, KDISPATCH_CUSTOM, cg_now_us() - t0);
  return 0;
}
