// backend/cpu/interpret.c - CPU dispatch ladder + chained-input pre-materialise.
//
// The UOp DAG walker (backend/cpu/uop_walk.c) covers every kernel
// shape the surgical suite produces; BLAS / JIT sit above it for
// the patterns those paths handle faster.
//
// What stays here:
//   - cpu_dispatch_kernel: the dispatch ladder + chained-input
//     pre-materialise wrapper.


// Forward decls: defined in backend/cpu/{blas,jit,uop_walk}.c (included
// after this file in thvm.c, so declare here for the dispatcher).
fn int cpu_blas_dispatch       (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn int cpu_jit_dispatch        (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn int cpu_uop_walk            (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);

// === ShapeTracker-chained input pre-materialise (CPU side) ===
//
// cpu_tendesc_strided_index: map an output flat index to the
// underlying-buffer ELEMENT index through a TenDesc's full ShapeTracker
// (`view` + `prior_views[0..nviews)`).  Same composition as
// metal_tendesc_strided_index, just returning a host-pointer element
// offset (add to an f32*/i32* base).  Thin wrapper over the shared
// tendesc_strided_index in view/strided_index.c.
static u32 cpu_tendesc_strided_index(TenDesc const *t, u32 flat_idx) {
  return tendesc_strided_index(t, flat_idx);
}

// Per-slot save state for a pre-materialise swap (restore + release
// after dispatch).
typedef struct {
  int  swapped;
  u32  saved_tid;
  u32  saved_numel;
  View saved_view;
  u32  temp_tid;
} CpuPrematSlot;

// Pre-materialise a kernel-input slot whose TenDesc carries a
// ShapeTracker chain (nviews > 0) into a fresh contiguous CPU buffer.
// The dispatch ladder (BLAS shape classifier / UOp-walk / JIT) all
// read inputs through
// `ke->input_views[slot]`, which for a chained TenDesc mirrors only the
// OUTERMOST (public) view -- the inner prior_views aren't applied, so a
// direct read lands on the wrong bytes.  We gather the strided view
// once via cpu_tendesc_strided_index, then swap in a fresh
// canonical-contig offset-0 TenDesc (+ its buffer) for the dispatch,
// restoring the original afterwards.  The CPU counterpart of the
// strided pre-mat metal_dispatch_kernel does (and of the deleted
// cpu_interpret pre-mat loop).
//
// Returns 1 if a swap was performed (caller restores via
// cpu_premat_restore), 0 otherwise.  On allocation/lookup failure
// returns 0 (input left as-is -- the chain case is bounded by the
// buffer-byte ceiling so a huge gather is refused upstream).
static int cpu_premat_chained_input(KernelEntry *ke, u32 slot,
                                    u32 *in_buf_ids, CpuPrematSlot *st) {
  st->swapped = 0;
  if (ke->input_views == NULL || ke->input_tids == NULL) return 0;
  u32 tid = ke->input_tids[slot];
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  TenDesc const *td = &TENS[tid];
  if (td->nviews == 0) return 0;                 // simple view -- ladder handles it
  // rangeify already folded this chain into the kernel INDEX (the LOAD
  // composes the full chain over the raw buffer) -- no gather needed.
  if (ke->input_chain_composed != NULL && ke->input_chain_composed[slot]) return 0;
  if (dtype_is_packed(td->dtype)) return 0;      // packed nibbles: out of scope
  u32 esz = dtype_itemsize(td->dtype);
  if (esz == 0) return 0;
  u32 src_buf = (td->buf_id != 0 && td->buf_id < CPU_BUFS_NEXT) ? td->buf_id : 0;
  if (src_buf == 0) src_buf = in_buf_ids[slot];
  if (src_buf == 0 || src_buf >= CPU_BUFS_NEXT) return 0;
  if (CPU_BUFS[src_buf].data == NULL) return 0;
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

// Does the per-call gate let cpu_uop_walk fire?  Default-ON; the
// walker fires AHEAD of cpu_jit_dispatch in the dispatch ladder, so
// every kernel the lifter accepts goes through the UOp DAG evaluator
// -- no clang-compile, no warmup gate.
//
// Bisection knob `THVM_CPU_UOP_WALK=0` is retained for diagnosis.
// With the walker disabled, dispatch flows BLAS -> JIT only; if both
// decline, cpu_dispatch_kernel returns 0 with KDISPATCH_INTERPRETER
// recorded.
static int cpu_uop_walk_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_CPU_UOP_WALK");
    enabled = (e == NULL) ? 1 : (e[0] != '0');
    known = 1;
  }
  return enabled;
}

static int cpu_dispatch_kernel_inner(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // Recover kid by pointer arithmetic into KERNELS[].  Used for
  // per-kid profiling (cg_profile_record).
  u32 kid = (u32)(ke - KERNELS);
  u64 t0  = cg_now_us();
  // 1. BLAS first: matmul / matvec / dot patterns get cblas_*
  //    (Accelerate on macOS) -- 10-100x faster than anything we can
  //    JIT-compile or scalar-interpret near-term.  Pattern-matches
  //    on the lifted UOp DAG; UOp DAG walker runs only on no-match.
  int blas_kind = cpu_blas_dispatch(ke, in_buf_ids, out_buf_id);
  if (blas_kind) {
    cg_profile_record(kid, (KDispatchKind)blas_kind, cg_now_us() - t0);
    return 0;
  }
  // 2. CPU JIT: clang-compiled fused kernel, keyed by the canonicalized
  //    rendered source so a hot training-loop kernel crosses the warmup
  //    gate and runs compiled (10-50x faster than the walker).  During
  //    warmup (and for kernels the JIT declines) it returns 0 and the
  //    walker below handles the fire -- so one-shot kernels still pay no
  //    compile cost.  (Was AFTER the walker, which meant the walker
  //    handled every steady-state kernel and the JIT never ran.)
  if (cpu_jit_dispatch(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_JIT, cg_now_us() - t0);
    return 0;
  }
  // 3. UOp DAG walker (interpreter): warmup + JIT-declined kernels.
  if (cpu_uop_walk_enabled()) {
    if (cpu_uop_walk(ke, in_buf_ids, out_buf_id)) {
      cg_profile_record(kid, KDISPATCH_INTERPRETER, cg_now_us() - t0);
      return 0;
    }
  }
  cg_profile_record(kid, KDISPATCH_INTERPRETER, cg_now_us() - t0);
  return 0;
}

// Public dispatch entry: pre-materialise any ShapeTracker-chained
// input (nviews > 0) into a contiguous temp, run the dispatch ladder,
// then restore + release.  Conv lowered the tinygrad-`_pool` way
// reaches here -- xCol is a multi-view strided view over the layer
// input -- so the gather collapses it to a dense matmul operand.  The
// gather is O(numel * nviews), done once per fire, freed right after.
fn int cpu_dispatch_kernel(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  CpuPrematSlot premat[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) premat[i].swapped = 0;
  for (u32 i = 0; i < ke->n_inputs; i++)
    cpu_premat_chained_input(ke, i, in_buf_ids, &premat[i]);
  int rc = cpu_dispatch_kernel_inner(ke, in_buf_ids, out_buf_id);
  for (u32 i = 0; i < ke->n_inputs; i++) cpu_premat_restore(ke, i, &premat[i]);
  return rc;
}
