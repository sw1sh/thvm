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

// input_views-decouple session 2: DAG-side stride extractor.
//
// `lift_scalar_index` (kernel_lift.c:388-407) builds an INDEX_E.addr by
// looping over the buffer's view dims and accumulating per-dim terms:
//
//   for d in 0..ndim:
//     stride = view.strides[d]                  -- broadcast dims (stride
//                                                   == 0) are skipped
//     term   = (stride == 1) ? r_uop
//                            : IMUL(r_uop, ICONST(stride))
//     acc    = (acc == 0) ? term : IADD(acc, term)
//
// For the matmul-A case under a [M, K, N] expanded view, two dims survive
// (M and K -- the N dim is broadcast-zero) so the address is exactly
//   IADD(arm_m, arm_k)
// where each arm is either RANGE(axis) or IMUL(RANGE(axis), ICONST(c)).
// The non-reduce arm carries `ld` (the storage stride of the non-K axis);
// the reduce arm has coefficient 1 if A is row-major (transA == 0) or
// coefficient M if A is transposed (transA == 1).  Symmetric for B with
// {k, n} arms and GEMV-W with {m, k} arms.
//
// The structural classifier guarantees the address has exactly two
// distinct RANGE axes (it bails on conv-shaped addresses with 3+ ranges
// and on dot-shaped addresses with only 1).  This extractor relies on
// that and only inspects the IADD-of-arms shape.
//
// Returns 1 on success.  Returns 0 when:
//   - `addr` isn't a UOP_IADD,
//   - either arm doesn't decode as RANGE or IMUL(RANGE, ICONST),
//   - neither arm matches the requested reduce axis,
//   - both arms reference the same axis id.

// Decode an arm of the IADD into (axis_id, coeff).  Returns 1 on success.
// Accepts either operand order in the IMUL (RANGE on lhs+ICONST on rhs,
// or the reverse) -- the int simplifier `uop_match_const_mul` already
// canonicalises the two, but the lifter constructs RANGE-on-lhs.
static int udg_extract_addr_arm(Term arm, u32 *out_axis_id, u32 *out_coeff) {
  if (term_tag(arm) != TAG_UOP) return 0;
  u32 op = term_ext(arm);
  if (op == UOP_RANGE) {
    *out_axis_id = uop_range_axis_id(arm);
    *out_coeff   = 1;
    return 1;
  }
  if (op != UOP_IMUL) return 0;
  Term lhs = heap_read(term_val(arm) + 0);
  Term rhs = heap_read(term_val(arm) + 1);
  Term r = 0; Term c = 0;
  if (term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_RANGE
      && term_tag(rhs) == TAG_UOP && term_ext(rhs) == UOP_CONST) {
    r = lhs; c = rhs;
  } else if (term_tag(rhs) == TAG_UOP && term_ext(rhs) == UOP_RANGE
             && term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_CONST) {
    r = rhs; c = lhs;
  } else {
    return 0;
  }
  Term cnum = heap_read(term_val(c) + 0);
  if (term_tag(cnum) != TAG_NUM) return 0;
  u32 cv = (u32)term_val(cnum);
  if (cv == 0) return 0;
  *out_axis_id = uop_range_axis_id(r);
  *out_coeff   = cv;
  return 1;
}

int uop_dag_extract_matmul_strides_from_addr(Term addr, u32 red_axis_id,
                                             u32 *out_red_coeff,
                                             u32 *out_other_coeff,
                                             u32 *out_other_axis_id) {
  if (out_red_coeff != NULL)     *out_red_coeff = 0;
  if (out_other_coeff != NULL)   *out_other_coeff = 0;
  if (out_other_axis_id != NULL) *out_other_axis_id = 0;
  if (term_tag(addr) != TAG_UOP || term_ext(addr) != UOP_IADD) return 0;
  Term arm1 = heap_read(term_val(addr) + 0);
  Term arm2 = heap_read(term_val(addr) + 1);
  u32 axis1 = 0, axis2 = 0, c1 = 0, c2 = 0;
  if (!udg_extract_addr_arm(arm1, &axis1, &c1)) return 0;
  if (!udg_extract_addr_arm(arm2, &axis2, &c2)) return 0;
  if (axis1 == axis2) return 0;

  // Identify which arm is the reduce ("k") arm.  The other is the
  // ld-bearing axis (m for A, n for B, m for W).
  u32 k_coeff, other_coeff, other_axis;
  if (axis1 == red_axis_id) {
    k_coeff = c1; other_coeff = c2; other_axis = axis2;
  } else if (axis2 == red_axis_id) {
    k_coeff = c2; other_coeff = c1; other_axis = axis1;
  } else {
    return 0;
  }

  if (out_red_coeff != NULL)     *out_red_coeff = k_coeff;
  if (out_other_coeff != NULL)   *out_other_coeff = other_coeff;
  if (out_other_axis_id != NULL) *out_other_axis_id = other_axis;
  return 1;
}

// View-strides matcher: matmul kernels run inside a 3-axis range
// nest [M, K, N], with one operand broadcast-zero on N (operand A)
// and the other broadcast-zero on M (operand B).  Mirrors
// tile_gemm_views_ok in src/schedule/tile.c exactly so the BLAS
// dispatcher accepts the same set of layouts the legacy
// tile_analyze_gemm path accepted.
//
// Kept as the input_views-based fallback for the hybrid path: when the
// DAG-side extractor (uop_dag_extract_matmul_strides_from_addr) declines
// because the address doesn't match the canonical IADD-of-(IMUL+RANGE)
// shape (e.g. a future affine-coalesced layout), the matmul classifier
// falls back to this view reader.  Once every shape `lift_scalar_index`
// can produce decodes via the DAG path, this helper retires.
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
  if (ke->n_inputs < 2) return 0;

  Term a_idx = 0, b_idx = 0;
  u32 k_extent = 0;
  if (!udg_classify_matmul_store(root, &k_extent, &a_idx, &b_idx)) return 0;
  if (k_extent == 0) return 0;
  // Reject K=1: the address arm collapses to bare RANGE(m) so transA is
  // ambiguous (and BLAS reduces to outer product anyway -- degenerate).
  if (k_extent == 1) return 0;

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

  // Recover the reduce axis id from the (possibly-OPT-wrapped) REDUCE
  // node so the stride extractor can identify the k-arm of each addr.
  Term value = heap_read(term_val(root) + 2);
  Term reduce = udg_peel_tc_opt(value);
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  u32 red_axis_id = (u32)term_val(heap_read(term_val(reduce) + 2));

  // INDEX_E.addr lives at heap_read(term_val(idx_term) + 1).
  Term addr_a = heap_read(term_val(a_idx) + 1);
  Term addr_b = heap_read(term_val(b_idx) + 1);

  // DAG-side stride extraction (preferred).  The IADD-of-(IMUL+RANGE)
  // shape encodes ldA/ldB/transA/transB without needing input_views[].
  u32 ldA = 0, ldB = 0;
  u32 transA = 0, transB = 0;
  u32 a_red_coeff = 0, a_other_coeff = 0, axis_m = 0;
  u32 b_red_coeff = 0, b_other_coeff = 0, axis_n = 0;
  int dag_a_ok = uop_dag_extract_matmul_strides_from_addr(
      addr_a, red_axis_id, &a_red_coeff, &a_other_coeff, &axis_m);
  int dag_b_ok = uop_dag_extract_matmul_strides_from_addr(
      addr_b, red_axis_id, &b_red_coeff, &b_other_coeff, &axis_n);

  if (!dag_a_ok || !dag_b_ok) {
    // Hybrid fallback: if the DAG path declined (unsupported addr shape)
    // and input_views[] is populated, fall back to the legacy view-stride
    // matcher.  Once every lift_scalar_index output decodes via the DAG,
    // this fallback retires.
    if (ke->input_views == NULL) return 0;
    if (!udg_match_view_strides(&ke->input_views[a_input], M, N, k_extent,
                                /*is_a=*/1, &ldA, &transA)) return 0;
    if (!udg_match_view_strides(&ke->input_views[b_input], M, N, k_extent,
                                /*is_a=*/0, &ldB, &transB)) return 0;
  } else {
    // Sanity: each arm's other_axis must be distinct (m != n).
    if (axis_m == axis_n) return 0;
    // BLAS convention (see uop_dag_extract_matmul_strides_from_addr docs):
    //   A: ldA = max(red, other), transA = (red_coeff != 1)
    //   B: ldB = max(red, other), transB = (other_coeff != 1)
    // For A non-trans: red=1, other=K -> ldA=K, transA=0.
    // For A trans:     red=M, other=1 -> ldA=M, transA=1.
    // For B non-trans: red=N, other=1 -> ldB=N, transB=0.
    // For B trans:     red=1, other=K -> ldB=K, transB=1.
    ldA    = (a_red_coeff > a_other_coeff) ? a_red_coeff : a_other_coeff;
    ldB    = (b_red_coeff > b_other_coeff) ? b_red_coeff : b_other_coeff;
    transA = (a_red_coeff   != 1) ? 1 : 0;
    transB = (b_other_coeff != 1) ? 1 : 0;
    // Validate ld values match the output buffer dims when not transposed.
    if (transA) { if (ldA != M) return 0; }
    else        { if (ldA != k_extent) return 0; }
    if (transB) { if (ldB != k_extent) return 0; }
    else        { if (ldB != N) return 0; }
  }

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

  // input_views-decouple session 2: DOT addresses are bare RANGE(k)
  // leaves -- the structural classifier `uop_classify_dot` already
  // verified each INDEX_E.addr references exactly one range and that
  // it's the reduce axis.  No stride extraction is needed: a bare
  // RANGE address is implicitly stride-1 contiguous.  Buffer rank
  // (rank-1 K elements) is implied by the buffer dim:
  Term addr_a = heap_read(term_val(a_idx) + 1);
  Term addr_b = heap_read(term_val(b_idx) + 1);
  if (term_tag(addr_a) != TAG_UOP || term_ext(addr_a) != UOP_RANGE) return 0;
  if (term_tag(addr_b) != TAG_UOP || term_ext(addr_b) != UOP_RANGE) return 0;
  if (uop_buffer_ndim(buf_a) != 1 || uop_buffer_dim(buf_a, 0) != k_extent) return 0;
  if (uop_buffer_ndim(buf_b) != 1 || uop_buffer_dim(buf_b, 0) != k_extent) return 0;

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

  // input_views-decouple session 2: DAG-side stride extraction.
  //
  // W's address is `IADD(arm_m, arm_k)` -- the same matmul-A shape the
  // session 2 extractor handles (the "ld" arm is the m-arm, the reduce
  // arm is the k-arm).  x's address is bare RANGE(k) -- the structural
  // classifier already verified it references precisely the reduce axis,
  // so no stride extraction is needed for x.
  Term addr_w = heap_read(term_val(w_idx) + 1);
  Term addr_x = heap_read(term_val(x_idx) + 1);
  if (term_tag(addr_x) != TAG_UOP || term_ext(addr_x) != UOP_RANGE) return 0;

  u32 ldW = 0;
  u32 transW = 0;
  u32 w_red_coeff = 0, w_other_coeff = 0, axis_m = 0;
  // x's bare RANGE leaf carries the reduce axis (classifier invariant).
  u32 red_axis_id = uop_range_axis_id(addr_x);
  int dag_w_ok = uop_dag_extract_matmul_strides_from_addr(
      addr_w, red_axis_id, &w_red_coeff, &w_other_coeff, &axis_m);
  (void)axis_m;

  if (!dag_w_ok) {
    // Hybrid fallback to legacy view reader.  Same shape validations as
    // the original implementation; retained until every lift output
    // decodes via the DAG path.
    if (ke->input_views == NULL) return 0;
    View const *vw = &ke->input_views[w_input];
    View const *vx = &ke->input_views[x_input];
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
  } else {
    // GEMV-W follows the matmul-A convention: m-arm carries ld in the
    // natural layout, k-arm coefficient signals transpose.
    //   non-trans: red=1,   other=K -> ldW=K, transW=0
    //   trans:     red=M,   other=1 -> ldW=M, transW=1
    ldW    = (w_red_coeff > w_other_coeff) ? w_red_coeff : w_other_coeff;
    transW = (w_red_coeff != 1) ? 1 : 0;
    if (transW) { if (ldW != M) return 0; }
    else        { if (ldW != k_extent) return 0; }
  }

  out->dtype   = dt;
  out->M       = M;
  out->K       = k_extent;
  out->w_input = w_input;
  out->x_input = x_input;
  out->ldW     = ldW;
  out->flags   = transW ? 1u : 0u;
  return 1;
}

// === Slice 8 (conv2d-flat session): DAG-side structural gate ===========
//
// `tile_analyze_conv2d_flat` (src/schedule/tile.c) reads the conv shape
// almost entirely from `ke->input_views[]`, `ke->output_shape`,
// `ke->input_dtypes[]`, and `ke->n_inputs` -- all of which survive
// program[] free under default `THVM_PHASE_C7_FREE_PROGRAM=1`.  The
// ONE program[]-side gate it actually needs is "the kernel's last op
// is UOP_REDUCE with REDUCE_SUM kind" (program[ke->n_ops - 1]).
//
// This DAG-side classifier replaces that gate.  Strategy:
//   1. `root` must be UOP_STORE.
//   2. STORE.value must be UOP_REDUCE (bare) OR UOP_OPT(_, CONV, 0)
//      wrapping a UOP_REDUCE (F4's recogniser may have wrapped it).
//   3. The REDUCE's kind must be REDUCE_SUM.
//
// The shape extraction for the actual conv fields (kh, kw, c_in,
// c_out, h_out, w_out, batch, w_offset, w_stride0/1, x_offset,
// x_stride_b/0/1/2, w_input, x_input, patch_input_base/count,
// threads, outputs_per_thread, reduce_unroll) stays in
// tile_analyze_conv2d_flat -- it reads `ke->input_views[]` /
// `ke->output_shape` / opt-derived counters, none of which need the
// DAG.  So this classifier returns just 1/0 with no out struct.
int uop_dag_classify_conv2d_flat_shape(Term root,
                                       struct KernelEntry const *ke) {
  (void)ke;  // unused -- view/shape extraction stays in the caller
  if (root == 0) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  Term value = heap_read(term_val(root) + 2);
  // Peel an optional UOP_OPT(_, CONV, 0) wrapper.  F4's
  // uop_recognise_conv installs this when the conv2d shape is detected;
  // the bare-REDUCE case fires when the recogniser hasn't run (e.g. the
  // dedicated `kernel_lift_from_conv2d` path that synthesises the DAG
  // directly, without going through rangeify+recognise_conv).
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT) {
    if (uop_opt_kind(value) != UOP_OPT_CONV) return 0;
    value = uop_opt_target(value);
  }
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  // REDUCE kind is at heap_read(loc + 1).
  Term kind = heap_read(term_val(value) + 1);
  if (term_tag(kind) != TAG_NUM) return 0;
  if (term_val(kind) != REDUCE_SUM) return 0;
  return 1;
}
