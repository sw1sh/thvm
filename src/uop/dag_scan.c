// uop/dag_scan.c -- read-side scanners over a UOp DAG.
//
// Phase C slice 4: KProgOp-iterating consumers (metal_kernel_supported,
// metal_dispatch_kernel's pre-build dtype gate, propose_kprog_*) need
// structural facts about a kernel that today come from walking
// `ke->program[]`.  After kernel_lift_to_uop materialises `cached_lift`
// the same facts are recoverable from the lifted UOp DAG without re-
// running the lifter.  This file collects the small read-only walks.
//
// Helpers here treat `root` as a UOP_STORE (single-output) or UOP_AFTER
// chain of stores.  They return safe defaults (0 / "uniform") when
// `root` is 0 so callers can chain them with the legacy program[] read
// behind a `cached_lift.store_root != 0` gate.
//
// Coverage matches rmu_discover_bufs_rec in render_uop.c (every UOp
// shape that appears in lifted kernels: arithmetic, INDEX_E, REDUCE,
// IWHERE, OPT, CAST/BITCAST, STORE, AFTER).

// === recursion guard ===========================================
// Lifted DAGs are bounded (KERNEL_LIFT_MAX_INPUT inputs; one STORE
// chain per output) but cycles aren't possible -- the lifter constructs
// pure DAGs.  No memoisation; each helper is one bounded walk per
// dispatch.  Depth is naturally O(n_ops + n_inputs).

// Walk the DAG rooted at `t` and verify every dtype-carrying node has
// dtype `dt`.  Returns 1 on uniform-dtype, 0 on mismatch / unknown.
//
// Nodes that carry an explicit dtype:
//   UOP_BUFFER  -> uop_buffer_dtype(t)
//   UOP_CONST   -> term_ext on the inner NUM cell at heap_read(loc + 0)
//   UOP_CAST    -> heap_read(loc + 1) (NUM(dst_dtype))
//   UOP_BITCAST -> heap_read(loc + 1) (NUM(dst_dtype))
//
// Other ops (UOP_ADD/MUL/NEG/REDUCE/...) carry their dtype implicitly
// via their operands; the recursive walk catches them when it hits a
// BUFFER or CONST leaf.  UOP_RANGE / UOP_I* / UOP_INVALID are integer
// address terms with no float-dtype payload; they're skipped.
//
// External linkage so the metal backend (compiled as a separate
// translation unit, backend_metal.o) can call this without an
// inline-definition link error -- mirrors tile_anno_applied_opts_count.
int uop_dag_dtype_uniform(Term t, u32 dt) {
  if (t == 0) return 1;
  if (term_tag(t) != TAG_UOP) return 1;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_BUFFER:
      return uop_buffer_dtype(t) == dt;
    case UOP_CONST: {
      Term inner = heap_read(loc + 0);
      if (term_tag(inner) != TAG_NUM) return 0;
      return term_ext(inner) == dt;
    }
    case UOP_CAST: case UOP_BITCAST: {
      // [src, NUM(dst_dtype)].  The cast result dtype must equal `dt`,
      // and we still recurse into src so any BUFFER/CONST under it is
      // also typed (lifted kernels keep input buffer dtype == output
      // dtype today; the cast checks catch future int<->float kernels).
      Term ddtype = heap_read(loc + 1);
      if (term_tag(ddtype) != TAG_NUM) return 0;
      if (term_val(ddtype) != dt) return 0;
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt);
    }
    // === integer-address arithmetic (skip; they're index-domain) ====
    case UOP_RANGE: case UOP_INVALID:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
      return 1;
    // === one-operand recursion ======================================
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_REDUCE:
    case UOP_OPT:
    case UOP_LOAD:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt);
    // === two-operand recursion ======================================
    case UOP_ADD:  case UOP_MUL:
    case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt);
    // === three-operand recursion ====================================
    case UOP_IWHERE:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 2), dt);
    case UOP_STORE:
      // [buf, addr, value].  buf must match `dt`; addr is index-domain
      // (skipped); value must match `dt`.
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 2), dt);
    case UOP_AFTER:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt);
    default:
      // Unknown op: conservative -- treat as non-uniform so the caller
      // bails to legacy.
      return 0;
  }
}

// Walk the DAG rooted at `t` and find the first UOP_RANGE leaf whose
// axis_type == 1 (KAX_REDUCE).  Returns the extent (NUM at slot 1) if
// found, else 0.  Used by propose's reduce-axis-size heuristic when
// program[] / scalar_uops aren't available.
u32 uop_dag_reduce_axis_extent(Term t) {
  if (t == 0 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    Term axt = heap_read(loc + 1);
    Term ext = heap_read(loc + 2);
    if (term_tag(axt) != TAG_NUM || term_tag(ext) != TAG_NUM) return 0;
    if (term_val(axt) != 1 /*KAX_REDUCE*/) return 0;
    return term_val(ext);
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E: case UOP_AFTER: {
      u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 0));
      if (a) return a;
      return uop_dag_reduce_axis_extent(heap_read(loc + 1));
    }
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:   case UOP_REDUCE:
    case UOP_LOAD:
      return uop_dag_reduce_axis_extent(heap_read(loc + 0));
    case UOP_IWHERE:
      {
        u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 0));
        if (a) return a;
        a = uop_dag_reduce_axis_extent(heap_read(loc + 1));
        if (a) return a;
        return uop_dag_reduce_axis_extent(heap_read(loc + 2));
      }
    case UOP_STORE:
      {
        // STORE = [buf, addr, value]; the address tree carries the
        // RANGE leaves the renderer hoists into for-loops.
        u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 1));
        if (a) return a;
        return uop_dag_reduce_axis_extent(heap_read(loc + 2));
      }
    default:
      return 0;
  }
}

// Walk the DAG rooted at `t` and verify every UOP_REDUCE node carries
// a sum/max kind compatible with the reduce-unroll/group-reduce
// metal templates.  Today both KProgOp and UOp REDUCE encode kind in a
// child slot; we conservatively pass any REDUCE we find (the legacy
// path's per-op switch covered REDUCE only without further checks).
//
// Returns 1 if at least one UOP_REDUCE is reachable from `t` AND every
// reachable arithmetic op is one of the float-only set the metal
// reduce-unroll template emits.  Mirrors propose_metal_reduce_unroll
// _kernel's program[]-side check.
//
// `is_float_only` flag tracks whether we've seen any non-float-only
// ops so far (returned via *out_ok).  *out_has_reduce is set to 1 on
// any UOP_REDUCE encounter.
static void uop_dag_reduce_unroll_walk(Term t, int *out_ok,
                                       int *out_has_reduce) {
  if (t == 0 || term_tag(t) != TAG_UOP) return;
  if (!*out_ok) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  // Coverage of accepted ops mirrors propose_metal_reduce_unroll
  // _kernel's KProgOp switch: float arithmetic + REDUCE + index-domain
  // (RANGE / I* / INVALID) + leaves (BUFFER/CONST) + STORE/AFTER
  // structural.  CMPEQ/CMPLT and OPT pass through.  Anything else
  // (e.g. CAST to a non-fp32 dtype) flips out_ok.
  switch (op) {
    case UOP_REDUCE:
      *out_has_reduce = 1;
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      return;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2: case UOP_LOG2:
    case UOP_SQRT: case UOP_OPT: case UOP_LOAD:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    case UOP_IWHERE:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 2), out_ok, out_has_reduce);
      return;
    case UOP_STORE:
      // Skip the buf operand (leaf BUFFER) and walk addr + value.
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 2), out_ok, out_has_reduce);
      return;
    case UOP_AFTER:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      return;
    case UOP_BUFFER: case UOP_CONST:
    case UOP_RANGE: case UOP_INVALID:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
      // Leaves / index-domain: no-op.
      return;
    case UOP_CAST: case UOP_BITCAST:
      // The renderer handles BITCAST same-itemsize moves and CAST is
      // already in propose's accepted KProgOp set indirectly via the
      // renderer's coverage; conservative: treat as ok.
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    default:
      *out_ok = 0;
      return;
  }
}

// Public surface: returns 1 iff the DAG rooted at `t` has at least one
// UOP_REDUCE and every reachable op is in the metal reduce-unroll
// accepted set.  Mirrors the KProgOp gate in propose.c.
int uop_dag_is_reduce_unroll_kernel(Term t) {
  int ok = 1;
  int has_reduce = 0;
  uop_dag_reduce_unroll_walk(t, &ok, &has_reduce);
  return ok && has_reduce;
}

// === Phase C slice 5: external-linkage decode helpers =================
//
// The Metal backend (src/backend/metal/_.m) lives in a separate TU
// from the main runtime, so the `fn`-prefixed (static inline) heap /
// term / uop_buffer accessors aren't visible.  Re-export the small
// subset the DAG-side per-op encoder needs as external-linkage shims.
// Mirrors the slice-4 pattern (uop_dag_dtype_uniform et al.).

// Decode the (op, loc) pair for a Term `t` that's expected to be a
// UOp.  Returns 1 on success with *out_op + *out_loc set; 0 if the
// term isn't TAG_UOP.
int uop_dag_decode_uop(Term t, u32 *out_op, u64 *out_loc) {
  if (term_tag(t) != TAG_UOP) return 0;
  if (out_op  != NULL) *out_op  = term_ext(t);
  if (out_loc != NULL) *out_loc = term_val(t);
  return 1;
}

// Decode a UOP_BUFFER's instance disambiguator.  Returns 0 (default
// instance) when `t` isn't a UOP_BUFFER -- callers that need to
// distinguish "not a buffer" from "instance==0" should pre-check via
// uop_dag_decode_uop first.
u32 uop_dag_buffer_instance(Term t) {
  return uop_buffer_inst_get(t);
}

// Decode a UOP_CONST node's underlying NUM payload.  Returns 1 on
// success with *out_dtype + *out_bits set; 0 if `t` isn't a CONST or
// the payload is malformed.
int uop_dag_const_payload(Term t, u32 *out_dtype, u32 *out_bits) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_NUM) return 0;
  if (out_dtype != NULL) *out_dtype = term_ext(inner);
  if (out_bits  != NULL) *out_bits  = (u32)term_val(inner);
  return 1;
}

// Read the i-th source slot of a UOp at heap loc `loc`.  Just a thin
// wrapper around heap_read that the metal TU can call directly.
Term uop_dag_heap_read(u64 loc, u32 offset) {
  return heap_read(loc + offset);
}

// Predicate shims: the UOp predicate functions live in the main TU
// as `fn` static inlines.  Re-export with external linkage.
int uop_dag_is_unary_ew (u32 op) { return uop_is_unary_elementwise ((u8)op); }
int uop_dag_is_binary_ew(u32 op) { return uop_is_binary_elementwise((u8)op); }

// === Slice 8: DAG-side GEMM-shape extractor ===========================
//
// `cpu_blas_dispatch` (backend/cpu/blas.c) historically read M/N/K and
// the input-slot mapping out of `ke->program[]` via
// `tile_analyze_gemm`.  Under default `THVM_PHASE_C7_FREE_PROGRAM=1`
// the program[] array is freed at materialize time, so the legacy
// path early-bails and matmul kernels regress to the slower
// per-element render_uop_c triple-loop.  This helper recovers the
// same TileGemmInfo-shaped facts from `ke->cached_lift.store_root`
// (the lifted UOp DAG) plus `ke->input_views[]` (which the lifter
// already used to compute matmul addresses, and which survives the
// program[] free).
//
// Strategy:
//   1. Peel an optional UOP_OPT(_, TC, _) wrapper from STORE.value
//      (the dedicated GEMM-only path `kernel_lift_from_gemm`
//      synthesises STORE(C, addrC, OPT(REDUCE, TC, 0)); the
//      rangeify-driven path leaves STORE(C, addrC, REDUCE(...))).
//   2. Run `uop_classify_matmul` on the (possibly-rebuilt) STORE to
//      validate matmul shape + recover K_extent.
//   3. Read M/N from the output buffer's dims (matmul output is 2-D
//      with shape [M, N] when the lifter or kernel_lift_from_gemm
//      builds it).
//   4. Recover input slot indices from the UOP_BUFFER instance field
//      on the matmul A/B operands (`instance == slot + 1` for inputs;
//      see lift_input_buffer in schedule/kernel_lift.c).
//   5. Derive ldA / ldB / transpose flags from
//      `ke->input_views[slot]` exactly as `tile_gemm_views_ok` does.
//
// Returns 1 iff the DAG is matmul-shaped AND every output is
// recoverable.  On 0, callers should fall back to the legacy
// program[] path (or skip BLAS dispatch entirely).
//
// Type `UopDagGemmShape` lives in src/thvm.h alongside the function
// declaration so the legacy fallback in cpu/blas.c can reference it
// without needing dag_scan.c's static typedef.

// Peel a UOP_OPT(_, TC, _) wrapper if present, returning the bare
// inner REDUCE-shaped value.  Otherwise returns `value` unchanged.
static Term udg_peel_tc_opt(Term value) {
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_OPT) return value;
  if (uop_opt_kind(value) != UOP_OPT_TC) return value;
  return uop_opt_target(value);
}

// Re-run uop_classify_matmul on a STORE whose value may be wrapped
// in UOP_OPT(_, TC, _).  When the wrapper is present we synthesise a
// transient STORE without the wrapper so the existing classifier
// (which expects the STORE.value to be a bare REDUCE) sees the
// canonical shape.  Returns 1 + K_extent on match.
static int udg_classify_matmul_store(Term store, u32 *out_k_extent,
                                     Term *out_a_idx, Term *out_b_idx) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  Term buf_out  = heap_read(term_val(store) + 0);
  Term addr_out = heap_read(term_val(store) + 1);
  Term value    = heap_read(term_val(store) + 2);
  Term peeled   = udg_peel_tc_opt(value);
  Term canon = (peeled == value) ? store : uop_store(buf_out, addr_out, peeled);
  if (!uop_classify_matmul(canon, out_k_extent)) return 0;
  // Reach into the canonicalised STORE for the matmul operands.
  // Layout: STORE(buf_out, addr_out, REDUCE(MUL(INDEX_E(A, addrA),
  //                                             INDEX_E(B, addrB)))).
  u64 sloc = term_val(canon);
  Term reduce = heap_read(sloc + 2);
  Term mul    = heap_read(term_val(reduce) + 0);
  if (out_a_idx != NULL) *out_a_idx = heap_read(term_val(mul) + 0);
  if (out_b_idx != NULL) *out_b_idx = heap_read(term_val(mul) + 1);
  return 1;
}

// View-strides matcher: matmul kernels run inside a 3-axis range
// nest [M, K, N], with one operand broadcast-zero on N (operand A)
// and the other broadcast-zero on M (operand B).  Mirrors
// tile_gemm_views_ok in src/schedule/tile.c exactly so the BLAS
// dispatcher accepts the same set of layouts the legacy
// tile_analyze_gemm path accepted.
//
// Returns 1 with *out_ld + *out_trans on success; 0 otherwise.
// `is_a` selects the A vs B stride pattern (different broadcast axis).
static int udg_match_view_strides(View const *v, u32 M, u32 N, u32 K,
                                  int is_a, u32 *out_ld, u32 *out_trans) {
  if (v == NULL || v->shape.ndim != 3) return 0;
  if (v->shape.dims[0] != M || v->shape.dims[1] != K
      || v->shape.dims[2] != N) return 0;
  if (is_a) {
    // A: 3rd axis (N) is broadcast => stride[2] == 0.
    if (v->strides[2] != 0) return 0;
    if (v->strides[0] == (i32)K && v->strides[1] == 1) {
      *out_ld = K; *out_trans = 0; return 1;
    }
    if (v->strides[0] == 1 && v->strides[1] == (i32)M) {
      *out_ld = M; *out_trans = 1; return 1;
    }
    return 0;
  } else {
    // B: 1st axis (M) is broadcast => stride[0] == 0.
    if (v->strides[0] != 0) return 0;
    if (v->strides[1] == (i32)N && v->strides[2] == 1) {
      *out_ld = N; *out_trans = 0; return 1;
    }
    if (v->strides[1] == 1 && v->strides[2] == (i32)K) {
      *out_ld = K; *out_trans = 1; return 1;
    }
    return 0;
  }
}

// Public entry: extract M/N/K + input slot mapping + ldA/ldB +
// transpose flags from the lifted UOp DAG `root` and the kernel's
// `input_views[]`.  Returns 1 on success.  All five UOp-DAG-side
// gates (matmul classify, output buf shape, input slot recovery,
// per-input view stride pattern) must succeed; any single gate
// failure returns 0 and lets the caller fall back to the legacy
// program[] path.
int uop_dag_classify_matmul_shape(Term root,
                                  struct KernelEntry const *ke,
                                  UopDagGemmShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->input_views == NULL || ke->n_inputs < 2) return 0;

  Term a_idx = 0, b_idx = 0;
  u32 k_extent = 0;
  if (!udg_classify_matmul_store(root, &k_extent, &a_idx, &b_idx)) return 0;
  if (k_extent == 0) return 0;

  // Output buf: STORE.src[0].  Must be a 2-D BUFFER with shape [M, N].
  Term buf_out = heap_read(term_val(root) + 0);
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  if (uop_buffer_ndim(buf_out) != 2) return 0;
  u32 M = uop_buffer_dim(buf_out, 0);
  u32 N = uop_buffer_dim(buf_out, 1);
  if (M == 0 || N == 0) return 0;

  // Recover input slots from BUFFER.instance.  INDEX_E.src[0] is the
  // buffer; instance == slot + 1 for inputs (see lift_input_buffer).
  Term buf_a = heap_read(term_val(a_idx) + 0);
  Term buf_b = heap_read(term_val(b_idx) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) return 0;
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) return 0;
  u32 inst_a = uop_buffer_inst_get(buf_a);
  u32 inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) return 0;
  u32 a_input = inst_a - 1;
  u32 b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs) return 0;
  if (a_input == b_input) return 0;

  // Per-input stride pattern (from input_views[], set by emit_kernel
  // _for_boundary alongside the lift) determines ldA / ldB / trans.
  u32 ldA = 0, transA = 0;
  u32 ldB = 0, transB = 0;
  if (!udg_match_view_strides(&ke->input_views[a_input], M, N, k_extent,
                              /*is_a=*/1, &ldA, &transA)) return 0;
  if (!udg_match_view_strides(&ke->input_views[b_input], M, N, k_extent,
                              /*is_a=*/0, &ldB, &transB)) return 0;

  // Uniform dtype: every BUFFER reachable from the store_root must
  // share dtype; mirrors tile_gemm_uniform_dtype's KProgOp gate.
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32 && dt != DT_FP64) return 0;
  if (uop_buffer_dtype(buf_a) != dt) return 0;
  if (uop_buffer_dtype(buf_b) != dt) return 0;

  out->dtype   = dt;
  out->M       = M;
  out->N       = N;
  out->K       = k_extent;
  out->a_input = a_input;
  out->b_input = b_input;
  out->ldA     = ldA;
  out->ldB     = ldB;
  out->flags   = (transA ? 1u : 0u) | (transB ? 2u : 0u);
  return 1;
}

// === Slice 8 session 3: DAG-side DOT-shape extractor =================
//
// `cpu_blas_dispatch` historically read DOT shape facts (K, slot
// indices, dtype) out of `ke->program[]` via a hand-rolled matcher.
// Under default `THVM_PHASE_C7_FREE_PROGRAM=1` the program[] is freed
// and the legacy gate early-bails -- regressing what would be a single
// `cblas_sdot` call to the per-element render_uop_c triple-loop.
//
// Strategy mirrors the GEMM extractor: classify via
// `uop_classify_dot`, then read M=1 / N=1 / output buffer rank from
// the lifted DAG.  Buffer slot indices come from BUFFER.instance.
int uop_dag_classify_dot_shape(Term root,
                               struct KernelEntry const *ke,
                               UopDagDotShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  u32 k_extent = 0;
  if (!uop_classify_dot(root, &k_extent)) return 0;
  if (k_extent == 0) return 0;

  // Output buffer must be rank-0 (scalar) OR rank-1 numel=1.
  Term buf_out = heap_read(term_val(root) + 0);
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  u32 ond = uop_buffer_ndim(buf_out);
  if (ond > 1) return 0;
  if (ond == 1 && uop_buffer_dim(buf_out, 0) != 1) return 0;

  // Reach into the STORE for the MUL operands.
  Term reduce = heap_read(term_val(root) + 2);
  Term mul    = heap_read(term_val(reduce) + 0);
  Term a_idx  = heap_read(term_val(mul) + 0);
  Term b_idx  = heap_read(term_val(mul) + 1);

  Term buf_a = heap_read(term_val(a_idx) + 0);
  Term buf_b = heap_read(term_val(b_idx) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) return 0;
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) return 0;
  u32 inst_a = uop_buffer_inst_get(buf_a);
  u32 inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) return 0;
  u32 a_input = inst_a - 1;
  u32 b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs) return 0;
  if (a_input == b_input) return 0;

  // Uniform dtype check.
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32 && dt != DT_FP64) return 0;
  if (uop_buffer_dtype(buf_a) != dt) return 0;
  if (uop_buffer_dtype(buf_b) != dt) return 0;

  // Per-input view: must be contiguous rank-1 K elements (or rank-1
  // strides {1}).  Mirrors the legacy DOT input-tid contiguity gate.
  if (ke->input_views != NULL) {
    View const *va = &ke->input_views[a_input];
    View const *vb = &ke->input_views[b_input];
    if (va->shape.ndim == 1) {
      if (va->shape.dims[0] != k_extent || va->strides[0] != 1) return 0;
    } else {
      // Higher-rank views must still be contiguous K elements; bail
      // for now (the legacy DOT path only saw rank-1 inputs).
      return 0;
    }
    if (vb->shape.ndim == 1) {
      if (vb->shape.dims[0] != k_extent || vb->strides[0] != 1) return 0;
    } else {
      return 0;
    }
  }

  out->dtype   = dt;
  out->K       = k_extent;
  out->a_input = a_input;
  out->b_input = b_input;
  return 1;
}

// === Slice 8 session 3: DAG-side GEMV-shape extractor ================
//
// GEMV: W:{M,K} @ x:{K} -> {M}.  In the WL/lift representation the
// vector x is broadcast to {M,K} via EXPAND so the MUL is elementwise.
// W's lift address is m*K + k (2 distinct ranges); x's lift address is
// just k (1 distinct range, because the broadcast m-axis has stride 0
// and is dropped by lift_scalar_index).  REDUCE_SUM along the k axis
// gives an {M} output.
//
// Strategy: classify the structural shape via `uop_classify_gemv`,
// pick out which MUL operand is W (carries 2 ranges) vs x (carries 1),
// recover M from the output buffer's rank-1 dim and from W's view, and
// derive ldW + transpose flag from the W input view.
int uop_dag_classify_gemv_shape(Term root,
                                struct KernelEntry const *ke,
                                UopDagGemvShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  u32 k_extent = 0;
  int w_first  = 0;
  if (!uop_classify_gemv(root, &k_extent, &w_first)) return 0;
  if (k_extent == 0) return 0;

  // Output buffer must be rank-1 with shape {M}.
  Term buf_out = heap_read(term_val(root) + 0);
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  if (uop_buffer_ndim(buf_out) != 1) return 0;
  u32 M = uop_buffer_dim(buf_out, 0);
  if (M == 0) return 0;

  // Reach into the STORE for the MUL operands.
  Term reduce = heap_read(term_val(root) + 2);
  Term mul    = heap_read(term_val(reduce) + 0);
  Term mul_a  = heap_read(term_val(mul) + 0);
  Term mul_b  = heap_read(term_val(mul) + 1);
  Term w_idx  = w_first ? mul_a : mul_b;
  Term x_idx  = w_first ? mul_b : mul_a;

  Term buf_w = heap_read(term_val(w_idx) + 0);
  Term buf_x = heap_read(term_val(x_idx) + 0);
  if (term_tag(buf_w) != TAG_UOP || term_ext(buf_w) != UOP_BUFFER) return 0;
  if (term_tag(buf_x) != TAG_UOP || term_ext(buf_x) != UOP_BUFFER) return 0;
  u32 inst_w = uop_buffer_inst_get(buf_w);
  u32 inst_x = uop_buffer_inst_get(buf_x);
  if (inst_w == 0 || inst_x == 0) return 0;
  u32 w_input = inst_w - 1;
  u32 x_input = inst_x - 1;
  if (w_input >= ke->n_inputs || x_input >= ke->n_inputs) return 0;
  if (w_input == x_input) return 0;

  // Uniform dtype check.
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32 && dt != DT_FP64) return 0;
  if (uop_buffer_dtype(buf_w) != dt) return 0;
  if (uop_buffer_dtype(buf_x) != dt) return 0;

  // Per-input view stride pattern.  W must be a {M,K} matrix with
  // strides {K,1} (untransposed) or {1,M} (transposed).  x must be a
  // {M,K} broadcast view (strides {0,1}), or a rank-1 {K} contig
  // {1}, or a rank-2 {1,K} row {?,1}.  Mirrors tile_gemm_views_ok's
  // N==1 path.
  if (ke->input_views == NULL) return 0;
  View const *vw = &ke->input_views[w_input];
  View const *vx = &ke->input_views[x_input];

  u32 ldW = 0, transW = 0;
  if (vw->shape.ndim == 2 && vw->shape.dims[0] == M
      && vw->shape.dims[1] == k_extent
      && vw->strides[0] == (i32)k_extent && vw->strides[1] == 1) {
    ldW = k_extent;
    transW = 0;
  } else if (vw->shape.ndim == 2 && vw->shape.dims[0] == M
             && vw->shape.dims[1] == k_extent
             && vw->strides[0] == 1 && vw->strides[1] == (i32)M) {
    ldW = M;
    transW = 1;
  } else {
    return 0;
  }

  int x_ok = 0;
  if (vx->shape.ndim == 1 && vx->shape.dims[0] == k_extent
      && vx->strides[0] == 1) {
    x_ok = 1;
  } else if (vx->shape.ndim == 2 && vx->shape.dims[0] == 1
             && vx->shape.dims[1] == k_extent && vx->strides[1] == 1) {
    x_ok = 1;
  } else if (vx->shape.ndim == 2 && vx->shape.dims[0] == M
             && vx->shape.dims[1] == k_extent
             && vx->strides[0] == 0 && vx->strides[1] == 1) {
    x_ok = 1;
  }
  if (!x_ok) return 0;

  out->dtype   = dt;
  out->M       = M;
  out->K       = k_extent;
  out->w_input = w_input;
  out->x_input = x_input;
  out->ldW     = ldW;
  out->flags   = transW ? 1u : 0u;
  return 1;
}
