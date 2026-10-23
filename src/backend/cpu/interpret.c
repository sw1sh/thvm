// backend/cpu/interpret.c - the CPU dispatch ladder: BLAS, then the C JIT, then
// the UOp walker (uop_walk.c) as the last rung.

// Defined in blas.c, jit.c, custom.c and uop_walk.c, included after this file.
fn int cpu_blas_dispatch       (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn int cpu_jit_dispatch        (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn int cpu_custom_dispatch     (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn int cpu_uop_walk            (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn char const *cpu_uop_walk_last_decline(void);

// Dispatches no rung completed, whose output holds no result (a walk that
// declined partway may have written part of it).
static u64 CPU_DISPATCH_NO_RUNG;

fn u64 cpu_dispatch_no_rung_count(void) {
  return CPU_DISPATCH_NO_RUNG;
}

static u32 cpu_tendesc_strided_index(TenDesc const *t, u32 flat_idx) {
  return tendesc_strided_index(t, flat_idx);
}

typedef struct {
  int  swapped;
  u32  saved_tid;
  u32  saved_numel;
  View saved_view;
  u32  temp_tid;
} CpuPrematSlot;

// The rungs read an input through ke->input_views[slot], only the outermost
// view, so a ShapeTracker-chained input must arrive composed into its INDEX.
static int cpu_premat_chained_input(KernelEntry *ke, u32 slot,
                                    u32 *in_buf_ids, CpuPrematSlot *st) {
  st->swapped = 0;
  if (ke->input_views == NULL || ke->input_tids == NULL) return 0;
  u32 tid = ke->input_tids[slot];
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  TenDesc const *td = &TENS[tid];
  if (td->nviews == 0) return 0;                 // simple view -- ladder handles it
  // rangeify folded the chain into the kernel INDEX over the raw buffer.
  if (ke->input_chain_composed != NULL && ke->input_chain_composed[slot]) return 0;
  if (dtype_is_packed(td->dtype)) return 0;      // packed nibbles: out of scope
  u32 esz = dtype_itemsize(td->dtype);
  if (esz == 0) return 0;
  // The dispatch's buffer, not td->buf_id: at JIT replay in_buf_ids holds the
  // substituted input while td still names the capture-time buffer.
  u32 src_buf = in_buf_ids[slot];
  if (src_buf == 0 || src_buf >= CPU_BUFS_NEXT
      || CPU_BUFS[src_buf].data == NULL) {
    src_buf = (td->buf_id != 0 && td->buf_id < CPU_BUFS_NEXT) ? td->buf_id : 0;
  }
  if (src_buf == 0 || src_buf >= CPU_BUFS_NEXT) return 0;
  if (CPU_BUFS[src_buf].data == NULL) return 0;
  thvm_fatal("CPU dispatch: a ShapeTracker-chained input reached the kernel uncomposed");
  u32 numel = td->view.numel;
  u32 new_tid = tensor_alloc(td->backend, td->view.shape, td->dtype);
  if (new_tid == 0) return 0;
  u32 new_buf = TENS[new_tid].buf_id;
  if (new_buf == 0 || new_buf >= CPU_BUFS_NEXT || CPU_BUFS[new_buf].data == NULL) {
    tensor_release(new_tid); return 0;
  }
  void const *src = CPU_BUFS[src_buf].data;
  void       *dst = CPU_BUFS[new_buf].data;
  switch (esz) {
    case 4: { u32 const *s = (u32 const *)src; u32 *o = (u32 *)dst;
              for (u32 k = 0; k < numel; k++) o[k] = s[cpu_tendesc_strided_index(td, k)]; break; }
    case 1: { u8  const *s = (u8  const *)src; u8  *o = (u8  *)dst;
              for (u32 k = 0; k < numel; k++) o[k] = s[cpu_tendesc_strided_index(td, k)]; break; }
    case 2: { u16 const *s = (u16 const *)src; u16 *o = (u16 *)dst;
              for (u32 k = 0; k < numel; k++) o[k] = s[cpu_tendesc_strided_index(td, k)]; break; }
    case 8: { u64 const *s = (u64 const *)src; u64 *o = (u64 *)dst;
              for (u32 k = 0; k < numel; k++) o[k] = s[cpu_tendesc_strided_index(td, k)]; break; }
    default: tensor_release(new_tid); return 0;
  }
  st->swapped     = 1;
  st->saved_tid   = ke->input_tids[slot];
  st->saved_numel = ke->input_numels ? ke->input_numels[slot] : numel;
  st->saved_view  = ke->input_views[slot];
  st->temp_tid    = new_tid;
  ke->input_tids[slot]  = new_tid;
  ke->input_views[slot] = TENS[new_tid].view;       // canonical contig
  if (ke->input_numels) ke->input_numels[slot] = numel;
  in_buf_ids[slot] = new_buf;
  return 1;
}

static void cpu_premat_restore(KernelEntry *ke, u32 slot, CpuPrematSlot *st) {
  if (!st->swapped) return;
  if (ke->input_tids)   ke->input_tids[slot]   = st->saved_tid;
  if (ke->input_views)  ke->input_views[slot]  = st->saved_view;
  if (ke->input_numels) ke->input_numels[slot] = st->saved_numel;
  if (st->temp_tid != 0) tensor_release(st->temp_tid);
  st->swapped = 0;
}

static int cpu_dispatch_kernel_inner(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  u32 kid = (u32)(ke - KERNELS);
  u64 t0  = cg_now_us();
  // BLAS first: cblas runs a matmul-shaped kernel 10-100x faster than the
  // JIT-compiled loops.
  int blas_kind = cpu_blas_dispatch(ke, in_buf_ids, out_buf_id);
  if (blas_kind) {
    cg_profile_record(kid, (KDispatchKind)blas_kind, cg_now_us() - t0);
    return 0;
  }
  // The C JIT runs a kernel 10-50x faster than the walker, which takes the
  // kernels it declines.
  if (cpu_jit_dispatch(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_JIT, cg_now_us() - t0);
    return 0;
  }
  if (cpu_uop_walk(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_INTERPRETER, cg_now_us() - t0);
    return 0;
  }
  cg_profile_record(kid, KDISPATCH_NONE, cg_now_us() - t0);
  return -1;
}

// Returns -1 when no rung ran.  The inner ladder only reports it: the
// swapped input_tids must be restored on the cached KernelEntry first.
fn int cpu_dispatch_kernel(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // A raw injected kernel has no lift to classify, and its arguments are
  // contiguous leaves.
  if (kernel_is_custom(ke)) {
    return cpu_custom_dispatch(ke, in_buf_ids, out_buf_id);
  }
  CpuPrematSlot premat[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) premat[i].swapped = 0;
  for (u32 i = 0; i < ke->n_inputs; i++)
    cpu_premat_chained_input(ke, i, in_buf_ids, &premat[i]);
  int rc = cpu_dispatch_kernel_inner(ke, in_buf_ids, out_buf_id);
  for (u32 i = 0; i < ke->n_inputs; i++) cpu_premat_restore(ke, i, &premat[i]);
  if (rc < 0) {
    CPU_DISPATCH_NO_RUNG++;
    fprintf(stderr,
            "thvm: CPU kernel kid=%u n_inputs=%u ran no dispatch rung "
            "(walker: %s); its output holds no result\n",
            (u32)(ke - KERNELS), ke->n_inputs, cpu_uop_walk_last_decline());
  }
  return rc;
}
