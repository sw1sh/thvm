// uop/dag_scan.c -- read-side scanners over a lifted UOp DAG.
//
// Consumers (metal_kernel_supported, metal_dispatch_kernel's pre-build
// dtype gate, propose_*) need structural facts about a kernel.
// kernel_lift_to_uop materialises cached_lift.store_root; these
// helpers recover the same facts from that DAG.
//
// Helpers here treat `root` as a UOP_STORE (single-output) or UOP_AFTER
// chain of stores.  They return safe defaults (0 / "uniform") when
// `root` is 0 so callers can early-bail when the lift declined.
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
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
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
// found, else 0.  Used by propose's reduce-axis-size heuristic over
// the lifted UOp DAG.
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
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
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

// Look up the extent of the UOP_RANGE leaf at `axis_id` anywhere in the
// DAG rooted at `t`.  Returns 0 if no such leaf is reachable.  Used by
// uop_dag_max_reduce_extent_product to resolve a REDUCE node's named
// reduce axes (which carry only axis_ids) to concrete extents.
static u32 uop_dag_range_extent_by_axis(Term t, u32 axis_id) {
  if (t == 0 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    if (uop_range_axis_id(t) == axis_id) return uop_range_extent(t);
    return 0;
  }
  if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) return 0;
  if (op == UOP_OPT) return uop_dag_range_extent_by_axis(uop_opt_target(t), axis_id);
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    u32 e = uop_dag_range_extent_by_axis(heap_read(loc + i), axis_id);
    if (e) return e;
  }
  return 0;
}

// Walk the DAG rooted at `t` for UOP_REDUCE nodes; for each, compute the
// product of its reduce-axis extents (resolving each named axis_id to
// the matching UOP_RANGE leaf's extent).  Writes the LARGEST such
// product across all reduces into *out_product and the n_axes of that
// reduce into *out_n_axes.  Returns 1 if at least one reduce with a
// resolvable extent was found, 0 otherwise.
//
// Used by the CONV-BWD REDUCE TILING trigger (codegen/hand_opts.c) to
// recognise a reduce-heavy kernel (e.g. the conv weight-grad sum over
// B*oh*ow) without a "conv" classifier -- the decision is purely on the
// reduce-extent product, so a small forward-conv K=25 reduce (below the
// trigger's threshold) is left untouched.
int uop_dag_max_reduce_extent_product(Term t, u64 *out_product,
                                      u32 *out_n_axes) {
  if (t == 0 || term_tag(t) != TAG_UOP) return 0;
  u64 best = 0;
  u32 best_n = 0;
  int found = 0;
  // Iterative DFS over the whole DAG (no memo: lifted DAGs are bounded).
  Term stack[512];
  u32 sp = 0;
  stack[sp++] = t;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) != TAG_UOP) continue;
    u32 op = term_ext(cur);
    if (op == UOP_REDUCE) {
      u32 n_axes = uop_reduce_n_axes(cur);
      u64 prod = 1;
      int ok = (n_axes > 0);
      for (u32 i = 0; i < n_axes; i++) {
        u32 aid = uop_reduce_axis(cur, i);
        u32 ext = uop_dag_range_extent_by_axis(t, aid);
        if (ext == 0) { ok = 0; break; }
        prod *= (u64)ext;
      }
      if (ok && prod > best) { best = prod; best_n = n_axes; found = 1; }
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      Term tgt = uop_opt_target(cur);
      if (term_tag(tgt) == TAG_UOP && sp < 512) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(cur);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 512; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  if (found) {
    if (out_product) *out_product = best;
    if (out_n_axes)  *out_n_axes  = best_n;
  }
  return found;
}

// Walk the DAG rooted at `t` and verify every UOP_REDUCE node carries
// a sum/max kind compatible with the reduce-unroll/group-reduce
// metal templates.  UOP_REDUCE encodes kind in a child slot; we
// conservatively pass any REDUCE we find.
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
  // _kernel: float arithmetic + REDUCE + index-domain
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
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
      // Leaves / index-domain: no-op.
      return;
    case UOP_CAST: case UOP_BITCAST:
      // The renderer handles BITCAST same-itemsize moves and CAST is
      // already in propose's accepted set indirectly via the
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
// accepted set.  Mirrors the per-op gate in propose.c.
int uop_dag_is_reduce_unroll_kernel(Term t) {
  int ok = 1;
  int has_reduce = 0;
  uop_dag_reduce_unroll_walk(t, &ok, &has_reduce);
  return ok && has_reduce;
}

// === External-linkage decode helpers =================================
//
// The Metal backend (src/backend/metal/_.m) lives in a separate TU
// from the main runtime, so the `fn`-prefixed (static inline) heap /
// term / uop_buffer accessors aren't visible.  Re-export the small
// subset the DAG-side per-op encoder needs as external-linkage shims.
// Mirrors the same pattern as uop_dag_dtype_uniform et al.

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

// === DAG-side GEMM-shape extractor ====================================
//
// `cpu_blas_dispatch` (backend/cpu/blas.c) needs M/N/K and the
// input-slot mapping for matmul-shaped kernels so it can issue an
// Accelerate dgemm.  This helper recovers a TileGemmInfo-shaped
// summary from `ke->cached_lift.store_root` (the lifted UOp DAG)
// plus `ke->input_views[]` (which the lifter already used to
// compute matmul addresses).
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
                                     Term *out_a_idx, Term *out_b_idx,
                                     u32 *out_unit_axis) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  Term buf_out  = heap_read(term_val(store) + 0);
  Term addr_out = heap_read(term_val(store) + 1);
  Term value    = heap_read(term_val(store) + 2);
  Term peeled   = udg_peel_tc_opt(value);
  Term canon = (peeled == value) ? store : uop_store(buf_out, addr_out, peeled);
  if (!uop_classify_matmul(canon, out_k_extent, out_unit_axis)) return 0;
  // Reach into the canonicalised STORE for the matmul operands.
  // Layout: STORE(buf_out, addr_out, REDUCE(MUL(INDEX_E(A, addrA),
  //                                             INDEX_E(B, addrB)))).
  u64 sloc = term_val(canon);
  Term reduce = heap_read(sloc + 2);
  Term mul    = heap_read(term_val(reduce) + 0);
  // Peel a CAST(INDEX_E) operand wrapper (bf16->f32 widen / int8->bf16
  // dequant) so callers receive the bare INDEX_E -- matching the
  // peel uop_classify_matmul does for the structural classification.
  if (out_a_idx != NULL) *out_a_idx = rec_tc_peel_cast(heap_read(term_val(mul) + 0));
  if (out_b_idx != NULL) *out_b_idx = rec_tc_peel_cast(heap_read(term_val(mul) + 1));
  return 1;
}

// DAG-side stride extractor.
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
  u32 unit_axis = 0;
  if (!udg_classify_matmul_store(root, &k_extent, &a_idx, &b_idx, &unit_axis))
    return 0;
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
  // A collapsed-unit operand axis (unit_axis != 0) is only consistent with
  // a unit output dim: M==1 for A's collapsed M (the GPT-2 decode LM-head
  // {1,K}@{K,N}={1,N}), N==1 for B's collapsed N.  Confirm against the
  // output buffer so a genuine dense matmul whose addr merely lost an arm
  // to some other rewrite can't slip through with a bogus stride.
  if (unit_axis == 1 && M != 1) return 0;
  if (unit_axis == 2 && N != 1) return 0;

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
  // Multi-axis REDUCE: this single-K matmul classifier expects exactly
  // one reduce axis; the multi-axis K compound case is handled by
  // uop_dag_classify_contraction_shape (C6).
  Term value = heap_read(term_val(root) + 2);
  Term reduce = udg_peel_tc_opt(value);
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis_id = uop_reduce_axis(reduce, 0);

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

  if (unit_axis != 0) {
    // Collapsed-unit GEMM: one operand's addr is a bare RANGE(k) (its
    // unit M/N axis collapsed to CONST(0) at rangeify), so its arm
    // extractor declined.  The collapsed operand is contiguous row-major
    // over (1, K) / (K, 1) -- a non-transposed read with ld = K / N.  The
    // surviving operand still decodes via the DAG arm extractor; require
    // it and synthesise the collapsed side's stride directly.
    if (unit_axis == 1) {
      // A's M-axis collapsed (M==1).  A = {1,K} row-major: ldA=K, transA=0.
      // B = {K,N} must decode normally and carry the {k,n} arms.
      if (!dag_b_ok) return 0;
      if (axis_n == red_axis_id) return 0;     // B's other arm must be N, not K
      ldB    = (b_red_coeff > b_other_coeff) ? b_red_coeff : b_other_coeff;
      transB = (b_other_coeff != 1) ? 1 : 0;
      if (transB) { if (ldB != k_extent) return 0; }
      else        { if (ldB != N) return 0; }
      ldA    = k_extent;
      transA = 0;
    } else {
      // B's N-axis collapsed (N==1).  B = {K,1} row-major: ldB=1.  A =
      // {M,K} decodes normally.  (BLAS GEMM rejects N<=1 downstream, so
      // this arm exists mainly for completeness/symmetry; the value is
      // still routed correctly should the N<=1 guard be relaxed.)
      if (!dag_a_ok) return 0;
      if (axis_m == red_axis_id) return 0;     // A's other arm must be M, not K
      ldA    = (a_red_coeff > a_other_coeff) ? a_red_coeff : a_other_coeff;
      transA = (a_red_coeff != 1) ? 1 : 0;
      if (transA) { if (ldA != M) return 0; }
      else        { if (ldA != k_extent) return 0; }
      ldB    = N;
      transB = 0;
    }
  } else if (!dag_a_ok || !dag_b_ok) {
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
    // Decode the OUTPUT STORE addr to identify the TRUE M-axis_id (the
    // arm with coefficient N) and N-axis_id (the arm with coefficient
    // 1).  udg_classify_matmul_store blindly assigns MUL operand 0 ->
    // A, operand 1 -> B; if the caller wrote MUL(B, A) (e.g. via
    // WL's Orderless Times reordering), `axis_m` and `axis_n` here
    // come back swapped.  We then need to swap a_input/b_input + ldA
    // /ldB + transA/transB so BLAS sees the canonical A @ B layout.
    Term addr_out = heap_read(term_val(root) + 1);
    u32 out_axis_M = 0xFFFFFFFFu, out_axis_N = 0xFFFFFFFFu;
    if (term_tag(addr_out) == TAG_UOP && term_ext(addr_out) == UOP_IADD) {
      Term oa1 = heap_read(term_val(addr_out) + 0);
      Term oa2 = heap_read(term_val(addr_out) + 1);
      u32 oax1 = 0, oax2 = 0, oc1 = 0, oc2 = 0;
      if (udg_extract_addr_arm(oa1, &oax1, &oc1)
          && udg_extract_addr_arm(oa2, &oax2, &oc2)
          && oax1 != oax2) {
        // Row-major output of shape {M, N}: m-arm carries coeff N,
        // n-arm carries coeff 1.
        if (oc1 == N && oc2 == 1)        { out_axis_M = oax1; out_axis_N = oax2; }
        else if (oc2 == N && oc1 == 1)   { out_axis_M = oax2; out_axis_N = oax1; }
      }
    }
    if (out_axis_M != 0xFFFFFFFFu) {
      // Detect mis-mapped operand assignment: axis_m should land on
      // the M axis, axis_n on the N axis.  When swapped, swap A/B.
      if (axis_m == out_axis_N && axis_n == out_axis_M) {
        u32 tmp_input  = a_input;     a_input     = b_input;     b_input     = tmp_input;
        u32 tmp_red    = a_red_coeff; a_red_coeff = b_red_coeff; b_red_coeff = tmp_red;
        u32 tmp_other  = a_other_coeff; a_other_coeff = b_other_coeff; b_other_coeff = tmp_other;
        u32 tmp_axis   = axis_m;      axis_m      = axis_n;      axis_n      = tmp_axis;
      } else if (axis_m != out_axis_M || axis_n != out_axis_N) {
        // Neither canonical nor swap-able -- bail so BLAS declines
        // and the walker handles the kernel via the generic path.
        return 0;
      }
    }
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
  // share dtype; mirrors tile_gemm_uniform_dtype's per-op gate.  bf16 is
  // admitted for the Metal simdgroup tensor-core path (the threadgroup
  // staging load upconverts bfloat->float); CPU BLAS re-rejects it (blas.c).
  //
  // q8 weight-only quant is the ONE non-uniform case: B (the weight) may be
  // INT8 while A and the output share the compute dtype (bf16/f32).
  // rec_tc_peel_cast (in udg_classify_matmul_store) already stripped the
  // int8->compute dequant CAST so buf_b is the raw int8 buffer.  Accepting
  // it here keeps the kernel on the matmul fast path: hand_opts Section 1
  // applies KOP_TC + KOP_GLOBAL on the WHOLE M/N/K axes (no generic
  // UPCAST/LOCAL split), so recognise_tc later stamps OPT(TC) and
  // rmu_emit_matmul_tc takes the tiled simdgroup path (its b_is_int8 branch
  // stages char->bfloat in-kernel -- half the weight bandwidth at TC rate).
  // out->dtype stays the COMPUTE dtype, so the CPU BLAS dispatch still
  // re-rejects (blas.c gates FP32/FP64 only) -- no int8 GEMM hits Accelerate.
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32 && dt != DT_FP64 && dt != DT_BF16) return 0;
  if (uop_buffer_dtype(buf_a) != dt) return 0;
  u32 dt_b = uop_buffer_dtype(buf_b);
  if (dt_b != dt && dt_b != DT_INT8) return 0;

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

// === DAG-side DOT-shape extractor =====================================
//
// cpu_blas_dispatch reads DOT shape facts (K, slot indices, dtype)
// from the lifted UOp DAG so the matmul-shaped kernel dispatches via
// a single cblas_sdot call instead of the per-element triple-loop.
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

  // DOT addresses are bare RANGE(k)
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

// === DAG-side GEMV-shape extractor ====================================
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

  // DAG-side stride extraction.
  //
  // W's address is `IADD(arm_m, arm_k)` -- the matmul-A shape the
  // stride extractor handles (the "ld" arm is the m-arm, the reduce
  // arm is the k-arm).  x's address is bare RANGE(k) -- the
  // structural classifier already verified it references precisely
  // the reduce axis, so no stride extraction is needed for x.
  Term addr_w = heap_read(term_val(w_idx) + 1);
  Term addr_x = heap_read(term_val(x_idx) + 1);
  if (term_tag(addr_x) != TAG_UOP || term_ext(addr_x) != UOP_RANGE) return 0;

  u32 ldW = 0;
  u32 transW = 0;
  u32 w_red_coeff = 0, w_other_coeff = 0;
  // x's bare RANGE leaf carries the reduce axis (classifier invariant).
  u32 red_axis_id = uop_range_axis_id(addr_x);
  int dag_w_ok = uop_dag_extract_matmul_strides_from_addr(
      addr_w, red_axis_id, &w_red_coeff, &w_other_coeff, NULL);

  if (!dag_w_ok) {
    // Fallback to view-stride reader for lift outputs the DAG addr-arm
    // extractor can't decode (e.g. non-IADD or non-RANGE addr leaves).
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

// === DAG-side VECMAT-shape extractor ==================================
//
// out{1,N} = a{1,K} @ B{K,N}.  The single-token-decode projection and
// the LM-head matvec produce this: the leading M==1 row is elided by
// the lift, so the A operand's address collapses to a bare RANGE(k)
// (1 range) while B keeps an IADD(k,n) (2 ranges).  The matmul
// classifier (which needs 2 ranges per operand) declines, and the gemv
// emit can't express a {K,N}-matrix orientation -- so this is routed to
// a cblas_sgemm with M=1, which cblas handles natively.
//
// Structurally identical to GEMV (one 2-range matrix operand + one
// 1-range bare-k vector operand) but with a {1,N} 2-D output instead of
// a {M} 1-D output, and the matrix's NON-reduce axis is the output N
// axis (vs GEMV where the matrix's non-reduce axis is the output M).
int uop_dag_classify_vecmat_shape(Term root,
                                  struct KernelEntry const *ke,
                                  UopDagVecmatShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  // Output buffer must be rank-2 {1, N} -- the elided unit leading row.
  Term buf_out = heap_read(term_val(root) + 0);
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  if (uop_buffer_ndim(buf_out) != 2) return 0;
  if (uop_buffer_dim(buf_out, 0) != 1) return 0;
  u32 N = uop_buffer_dim(buf_out, 1);
  if (N == 0) return 0;

  // Structural shape: matrix (2 ranges m/k... here k/n) * vector (1
  // bare-k range), single reduce axis.  Reuse the GEMV recogniser; it
  // picks the 2-range operand as "W" (our matrix B) and the 1-range
  // bare-k operand as "x" (our vector a).
  u32 k_extent = 0;
  int b_first  = 0;
  if (!uop_classify_gemv(root, &k_extent, &b_first)) return 0;
  if (k_extent == 0 || k_extent == 1) return 0;

  // Reach into the STORE for the MUL operands.  (b = matrix, a = vector.)
  Term reduce = heap_read(term_val(root) + 2);
  Term mul    = heap_read(term_val(reduce) + 0);
  Term mul_a  = heap_read(term_val(mul) + 0);
  Term mul_b  = heap_read(term_val(mul) + 1);
  Term b_idx  = b_first ? mul_a : mul_b;
  Term a_idx  = b_first ? mul_b : mul_a;

  Term buf_b = heap_read(term_val(b_idx) + 0);
  Term buf_a = heap_read(term_val(a_idx) + 0);
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) return 0;
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) return 0;
  u32 inst_b = uop_buffer_inst_get(buf_b);
  u32 inst_a = uop_buffer_inst_get(buf_a);
  if (inst_b == 0 || inst_a == 0) return 0;
  u32 b_input = inst_b - 1;
  u32 a_input = inst_a - 1;
  if (b_input >= ke->n_inputs || a_input >= ke->n_inputs) return 0;
  if (b_input == a_input) return 0;

  // Uniform dtype check.
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32 && dt != DT_FP64) return 0;
  if (uop_buffer_dtype(buf_b) != dt) return 0;
  if (uop_buffer_dtype(buf_a) != dt) return 0;

  // a's address is bare RANGE(k) -- the contiguous {1,K} vector, so its
  // k-stride is 1 (ldA = K, transA = 0 at the cblas_sgemm M=1 call).
  Term addr_a = heap_read(term_val(a_idx) + 1);
  Term addr_b = heap_read(term_val(b_idx) + 1);
  if (term_tag(addr_a) != TAG_UOP || term_ext(addr_a) != UOP_RANGE) return 0;
  u32 red_axis_id = uop_range_axis_id(addr_a);

  // B's address is IADD(k-arm, n-arm).  Row-major {K,N}: k-stride N
  // (= ldB), n-stride 1 (transB = 0).  Transposed {N,K}: n-stride K
  // (= ldB), k-stride 1 (transB = 1).
  u32 b_red_coeff = 0, b_other_coeff = 0, n_axis = 0;
  if (!uop_dag_extract_matmul_strides_from_addr(
          addr_b, red_axis_id, &b_red_coeff, &b_other_coeff, &n_axis)) {
    return 0;
  }
  u32 ldB    = (b_red_coeff > b_other_coeff) ? b_red_coeff : b_other_coeff;
  u32 transB = (b_other_coeff != 1) ? 1u : 0u;
  if (transB) { if (ldB != k_extent) return 0; }
  else        { if (ldB != N)        return 0; }

  out->dtype   = dt;
  out->N       = N;
  out->K       = k_extent;
  out->a_input = a_input;
  out->b_input = b_input;
  out->ldB     = ldB;
  out->flags   = transB;
  return 1;
}

// === DAG-side structural gate for conv2d-flat =========================
//
// `tile_analyze_conv2d_flat` (src/schedule/tile.c) reads the conv shape
// almost entirely from `ke->input_views[]`, `ke->output_shape`,
// `ke->input_dtypes[]`, and `ke->n_inputs` -- all of which survive
// DAG-side classifier for "the kernel's last op is UOP_REDUCE with
// REDUCE_SUM kind".  Strategy:
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
int uop_dag_classify_conv2d_flat_shape(Term root) {
  if (root == 0) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  Term value = heap_read(term_val(root) + 2);
  // Peel an optional UOP_OPT(_, CONV, 0) wrapper.  uop_recognise_conv
  // installs this when the conv2d shape is detected; the bare-REDUCE
  // case fires when the recogniser hasn't run yet on this root.
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

// === DAG-side full-shape extractor (conv2d-flat) ======================
//
// Inverts the conv2d-flat address tree shape (IDIV/IMOD decomposition of
// r_out + r_q into batch/output-spatial/channel/kernel-spatial axes) so
// the conv shape facts
// (c_out, c_in, kh, kw, batch, h_out, w_out, w_offset, w_stride0/1,
// x_offset, x_stride_b/0/1/2) flow from the lifted UOp DAG instead of
// from `ke->input_views[]`.  The IDIV/IMOD decomposition is deterministic
// in the forward direction; given knowledge of which divisors mean what,
// the inverse is a pattern match on the address tree.
//
// The simplifier (src/uop/index_simplify.c) rewrites the construction-
// time tree:
//   * `(c*x)/c -> x`, `(c*x+y)/c -> x` (when y < c)
//   * `(r % (k*c))/c -> (r/c) % k` (nested div-mod)
//   * `(r % (k*c)) % c -> r % c`   (nested mod-mod)
//   * `IMOD by 1 -> 0`, `IDIV by 1 -> a`, `RANGE % bound -> RANGE`
//   * IADD/IMUL absorbing 0/1 identities
// so we match BOTH the lifter-produced shape AND the canonical
// post-simplifier shape:
//   * lifter: `co = r_out / patches`, `bi = (r_out % patches) / spatial`
//             `oh = ((r_out%patches)%spatial) / w_out`,
//             `ow = ((r_out%patches)%spatial) % w_out`
//   * canonical (after simplification, batch>=1, spatial = h_out*w_out):
//             `co = r_out / patches`,
//             `bi = (r_out / spatial) % batch` (or 0 if batch=1),
//             `oh = (r_out / w_out) % h_out`,
//             `ow = r_out % w_out`
// Same for r_q -> ci, kh_v, kw_v.
//
// Out-of-scope for this session (deferred):
//   * Multi-input IWHERE chain (lifter itself reads input_views at
//     kernel_lift.c:1070 so the lift is fundamentally tied; the DAG
//     extractor falls back to input_views in this case).
//   * Degenerate kh==kw==1 (kh_v / kw_v collapse to 0 leaving no IMOD
//     marker for k-axis dims).  KRED == r_q.extent fully determines
//     these but the addr-only path can't disambiguate; we fall back.
//
// Returns 1 with `out` filled iff the DAG shape decoded cleanly; 0
// otherwise -- callers fall back to the input_views[] reader.

// Match `IDIV(t, ICONST(*divisor))` (constant divisor only).
static int udg_match_idiv_const(Term t, Term *out_num, u32 *out_div) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IDIV) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_CONST) return 0;
  Term bnum = heap_read(term_val(b) + 0);
  if (term_tag(bnum) != TAG_NUM) return 0;
  *out_num = a;
  *out_div = (u32)term_val(bnum);
  return 1;
}

// Match `IMOD(t, ICONST(*divisor))` (constant divisor only).
static int udg_match_imod_const(Term t, Term *out_num, u32 *out_div) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IMOD) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_CONST) return 0;
  Term bnum = heap_read(term_val(b) + 0);
  if (term_tag(bnum) != TAG_NUM) return 0;
  *out_num = a;
  *out_div = (u32)term_val(bnum);
  return 1;
}

// Match `IMUL(t, ICONST(*coeff))` either operand order (mirrors
// uop_match_const_mul).  Returns 1 with the non-const half in *out_t.
static int udg_match_imul_const(Term t, Term *out_t, u32 *out_coeff) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IMUL) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  Term other = 0; u32 coeff = 0;
  if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) {
    Term anum = heap_read(term_val(a) + 0);
    if (term_tag(anum) != TAG_NUM) return 0;
    coeff = (u32)term_val(anum); other = b;
  } else if (term_tag(b) == TAG_UOP && term_ext(b) == UOP_CONST) {
    Term bnum = heap_read(term_val(b) + 0);
    if (term_tag(bnum) != TAG_NUM) return 0;
    coeff = (u32)term_val(bnum); other = a;
  } else {
    return 0;
  }
  *out_t = other; *out_coeff = coeff;
  return 1;
}

// Match `RANGE` and pull its axis_id (caller may check vs expected).
static int udg_is_range_axis(Term t, u32 want_axis_id) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  return uop_range_axis_id(t) == want_axis_id;
}

// Read a top-level signed CONST out of a Term.  Used to recover offsets.
static int udg_match_iconst_signed(Term t, i32 *out_v) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term num = heap_read(term_val(t) + 0);
  if (term_tag(num) != TAG_NUM) return 0;
  *out_v = (i32)term_val(num);
  return 1;
}

// Flatten a left-leaning IADD chain into up to N leaves.  Returns the
// leaf count (capped at cap; returns 0 if the cap would be exceeded so
// callers can bail).
#define UDG_CONV_ADDR_MAX_LEAVES 12
static u32 udg_flatten_iadd(Term t, Term *leaves, u32 cap) {
  // Recursive: descend through IADD nodes, stash non-IADD as leaves.
  if (cap == 0) return 0;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IADD) {
    leaves[0] = t;
    return 1;
  }
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  u32 na = udg_flatten_iadd(a, leaves, cap);
  if (na == 0) return 0;
  if (na >= cap) return 0;
  u32 nb = udg_flatten_iadd(b, leaves + na, cap - na);
  if (nb == 0) return 0;
  return na + nb;
}

// Decode the W INDEX_E.addr.
//
// Lifter shape (after simplification):
//   [IADD(k_w_off,)] IADD( IMUL(IDIV(r_out, patches), w_s0),
//                           [IMUL(r_q, w_s1) | r_q if w_s1==1] )
//
// Recovers: patches (= u32 divisor), w_stride0, w_stride1, w_offset.
// Returns 1 on success, fills out_axis_r_out + out_axis_r_q so the
// caller can plumb them into the X decode + cross-checks.
static int udg_decode_conv_w_addr(Term addr,
                                  u32 *out_axis_r_out,
                                  u32 *out_axis_r_q,
                                  u32 *out_patches,
                                  u32 *out_w_stride0,
                                  u32 *out_w_stride1,
                                  i32 *out_w_offset) {
  Term leaves[UDG_CONV_ADDR_MAX_LEAVES] = {0};
  u32  n_leaves = udg_flatten_iadd(addr, leaves, UDG_CONV_ADDR_MAX_LEAVES);
  if (n_leaves == 0 || n_leaves > 3) return 0;

  i32 w_off = 0;
  Term arm_co = 0, arm_q = 0;
  for (u32 i = 0; i < n_leaves; i++) {
    Term t = leaves[i];
    i32 cv;
    if (udg_match_iconst_signed(t, &cv)) {
      w_off = cv;
      continue;
    }
    // Check if this leaf decodes as `(IDIV(r_out, patches)) * w_s0` or
    // bare IDIV (when w_s0 == 1).
    Term idiv_num = 0; u32 idiv_div = 0;
    Term mul_inner = 0; u32 mul_coeff = 1;
    if (udg_match_imul_const(t, &mul_inner, &mul_coeff)
        && udg_match_idiv_const(mul_inner, &idiv_num, &idiv_div)) {
      // co-arm: IMUL(IDIV(r_out, patches), w_s0).  Check r_out is RANGE.
      if (term_tag(idiv_num) != TAG_UOP || term_ext(idiv_num) != UOP_RANGE) continue;
      arm_co = t;
      *out_axis_r_out = uop_range_axis_id(idiv_num);
      *out_patches    = idiv_div;
      *out_w_stride0  = mul_coeff;
      continue;
    }
    if (udg_match_idiv_const(t, &idiv_num, &idiv_div)) {
      // co-arm with w_s0 == 1: IDIV(r_out, patches).
      if (term_tag(idiv_num) != TAG_UOP || term_ext(idiv_num) != UOP_RANGE) continue;
      arm_co = t;
      *out_axis_r_out = uop_range_axis_id(idiv_num);
      *out_patches    = idiv_div;
      *out_w_stride0  = 1;
      continue;
    }
    // q-arm: bare RANGE or IMUL(RANGE, w_s1).
    if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_RANGE) {
      arm_q = t;
      *out_axis_r_q   = uop_range_axis_id(t);
      *out_w_stride1  = 1;
      continue;
    }
    Term qmul_inner = 0; u32 qmul_coeff = 0;
    if (udg_match_imul_const(t, &qmul_inner, &qmul_coeff)
        && term_tag(qmul_inner) == TAG_UOP
        && term_ext(qmul_inner) == UOP_RANGE) {
      arm_q = t;
      *out_axis_r_q   = uop_range_axis_id(qmul_inner);
      *out_w_stride1  = qmul_coeff;
      continue;
    }
    return 0;  // unrecognised leaf
  }
  if (arm_co == 0 || arm_q == 0) return 0;
  if (*out_patches == 0 || *out_w_stride0 == 0 || *out_w_stride1 == 0) return 0;
  if (*out_axis_r_out == *out_axis_r_q) return 0;
  *out_w_offset = w_off;
  return 1;
}

// Recogniser for an X-address arm in a conv2d_flat single-input lift.
// Each leaf decodes into one of the slots below; an unrecognised arm
// causes the whole decode to bail.
typedef enum {
  UDG_CONV_ARM_NONE   = 0,
  UDG_CONV_ARM_CONST  = 1,  // x_offset
  UDG_CONV_ARM_CI     = 2,  // ci * x_stride2
  UDG_CONV_ARM_BI     = 3,  // bi * x_stride_b
  UDG_CONV_ARM_H      = 4,  // (oh + kh_v) * x_stride0   -- oh, h_out, x_s0
  UDG_CONV_ARM_W      = 5,  // (ow + kw_v) * x_stride1   -- ow, w_out, x_s1
} UdgConvArmKind;

// Match the canonical `oh = (r_out / w_out) % h_out` shape OR the
// uncollapsed lifter shape `oh = ((r_out%patches)%spatial) / w_out` (rare;
// the simplifier normally rewrites it to the canonical).  Sets *out_w_out
// + *out_h_out + *out_axis on success.
static int udg_match_oh(Term t, u32 axis_r_out,
                        u32 *out_w_out, u32 *out_h_out) {
  // Canonical shape (after simplifier): IMOD(IDIV(r_out, w_out), h_out).
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  Term div_num = 0; u32 div_div = 0;
  if (!udg_match_idiv_const(mod_num, &div_num, &div_div)) return 0;
  if (!udg_is_range_axis(div_num, axis_r_out)) return 0;
  *out_w_out = div_div;
  *out_h_out = mod_div;
  return 1;
}

// Match `ow = r_out % w_out` (canonical).
static int udg_match_ow(Term t, u32 axis_r_out, u32 *out_w_out) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  if (!udg_is_range_axis(mod_num, axis_r_out)) return 0;
  *out_w_out = mod_div;
  return 1;
}

// Match `bi = (r_out / spatial) % batch` (canonical, batch>=2) OR
// `bi = IDIV(IMOD(r_out, patches), spatial)` (uncollapsed; rare).
static int udg_match_bi(Term t, u32 axis_r_out,
                        u32 *out_spatial, u32 *out_batch) {
  // Canonical: IMOD(IDIV(r_out, spatial), batch).
  Term mod_num = 0; u32 mod_div = 0;
  if (udg_match_imod_const(t, &mod_num, &mod_div)) {
    Term div_num = 0; u32 div_div = 0;
    if (udg_match_idiv_const(mod_num, &div_num, &div_div)
        && udg_is_range_axis(div_num, axis_r_out)) {
      *out_spatial = div_div;
      *out_batch   = mod_div;
      return 1;
    }
  }
  return 0;
}

// Match `ci = (r_q / kw) / kh` (canonical-ish; lifter writes the two
// IDIVs separately and the simplifier doesn't fuse them).  Recovers
// kw + kh.
static int udg_match_ci(Term t, u32 axis_r_q, u32 *out_kw, u32 *out_kh) {
  // Outer IDIV: (qk / kh) where qk = (r_q / kw).
  Term outer_num = 0; u32 outer_div = 0;
  if (!udg_match_idiv_const(t, &outer_num, &outer_div)) return 0;
  Term inner_num = 0; u32 inner_div = 0;
  if (!udg_match_idiv_const(outer_num, &inner_num, &inner_div)) return 0;
  if (!udg_is_range_axis(inner_num, axis_r_q)) return 0;
  *out_kw = inner_div;
  *out_kh = outer_div;
  return 1;
}

// Match `kh_v = (r_q / kw) % kh`.
static int udg_match_kh_v(Term t, u32 axis_r_q, u32 *out_kw, u32 *out_kh) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  Term div_num = 0; u32 div_div = 0;
  if (!udg_match_idiv_const(mod_num, &div_num, &div_div)) return 0;
  if (!udg_is_range_axis(div_num, axis_r_q)) return 0;
  *out_kw = div_div;
  *out_kh = mod_div;
  return 1;
}

// Match `kw_v = r_q % kw`.
static int udg_match_kw_v(Term t, u32 axis_r_q, u32 *out_kw) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  if (!udg_is_range_axis(mod_num, axis_r_q)) return 0;
  *out_kw = mod_div;
  return 1;
}

// Decode the H-axis arm: `IMUL(IADD(oh, kh_v), x_stride0)` (kh>1) OR
// bare `IMUL(oh, x_stride0)` (kh==1, kh_v==0 collapsed) OR the
// degenerate `IADD(oh, kh_v)` (x_stride0==1) -- but for now we require
// x_stride0 != 1 (typical conv has x_stride0 == w which is rarely 1).
static int udg_decode_h_arm(Term t, u32 axis_r_out, u32 axis_r_q,
                            u32 *out_x_s0, u32 *out_w_out, u32 *out_h_out,
                            u32 *out_kw, u32 *out_kh) {
  Term inner = 0; u32 coeff = 0;
  // Outer IMUL with stride.
  if (!udg_match_imul_const(t, &inner, &coeff)) return 0;
  if (coeff == 0) return 0;
  *out_x_s0 = coeff;
  // Inner is IADD(oh, kh_v) -- both must match.
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_IADD) {
    // kh==1 case: bare oh.  We don't support degenerate here.
    return 0;
  }
  Term lhs = heap_read(term_val(inner) + 0);
  Term rhs = heap_read(term_val(inner) + 1);
  // Try (oh, kh_v) and (kh_v, oh) orderings.
  u32 kw1 = 0, kh1 = 0;
  if (udg_match_oh(lhs, axis_r_out, out_w_out, out_h_out)
      && udg_match_kh_v(rhs, axis_r_q, &kw1, &kh1)) {
    *out_kw = kw1; *out_kh = kh1;
    return 1;
  }
  if (udg_match_oh(rhs, axis_r_out, out_w_out, out_h_out)
      && udg_match_kh_v(lhs, axis_r_q, &kw1, &kh1)) {
    *out_kw = kw1; *out_kh = kh1;
    return 1;
  }
  return 0;
}

// Decode the CI-axis arm: `IMUL(ci, x_s2)` OR bare `ci` when x_s2==1.
static int udg_decode_ci_arm(Term t, u32 axis_r_q,
                             u32 *out_x_s2, u32 *out_kw, u32 *out_kh) {
  // bare ci case (x_s2==1): IDIV(IDIV(r_q, kw), kh).
  u32 kw1 = 0, kh1 = 0;
  if (udg_match_ci(t, axis_r_q, &kw1, &kh1)) {
    *out_x_s2 = 1; *out_kw = kw1; *out_kh = kh1; return 1;
  }
  Term inner = 0; u32 coeff = 0;
  if (!udg_match_imul_const(t, &inner, &coeff)) return 0;
  if (coeff == 0) return 0;
  if (!udg_match_ci(inner, axis_r_q, &kw1, &kh1)) return 0;
  *out_x_s2 = coeff; *out_kw = kw1; *out_kh = kh1;
  return 1;
}

// Decode the BI-axis arm: `IMUL(bi, x_sb)` OR bare `bi` when x_sb==1.
static int udg_decode_bi_arm(Term t, u32 axis_r_out,
                             u32 *out_x_sb, u32 *out_spatial, u32 *out_batch) {
  u32 sp = 0, bt = 0;
  if (udg_match_bi(t, axis_r_out, &sp, &bt)) {
    *out_x_sb = 1; *out_spatial = sp; *out_batch = bt; return 1;
  }
  Term inner = 0; u32 coeff = 0;
  if (!udg_match_imul_const(t, &inner, &coeff)) return 0;
  if (coeff == 0) return 0;
  if (!udg_match_bi(inner, axis_r_out, &sp, &bt)) return 0;
  *out_x_sb = coeff; *out_spatial = sp; *out_batch = bt;
  return 1;
}

// === 1x1 conv detection + dedicated parser ===
//
// When kh == kw == 1, the simplifier collapses `r_q / 1 -> r_q`,
// `r_q % 1 -> 0`, and `IADD(x, 0) -> x` so the X address loses ALL
// IDIV/IMOD on the r_q axis.  The CI arm degenerates to either a bare
// `RANGE(r_q)` or `IMUL(RANGE(r_q), x_s2)`; the H arm becomes
// `IMUL(IMOD(IDIV(r_out, w_out), h_out), x_s0)`; the W arm becomes
// `IMUL(IMOD(r_out, w_out), x_s1)` (or bare IMOD when x_s1==1); and the
// optional BI arm becomes `IMUL(IMOD(IDIV(r_out, spatial), batch), x_sb)`.
//
// This collides STRUCTURALLY with the standard parser's BI matcher: the
// H arm and BI arm both look like `IMUL(IMOD(IDIV(r_out, X), Y), C)`,
// distinguishable only by divisor values.  Rather than teach the
// existing matchers a divisor disambiguator (greedy mis-classification
// risk on canonical >=2x2 conv), we run a dedicated 1x1 parser that
// is invoked ONLY when the X address is positively identified as 1x1.

// Walk `t` and return 1 iff any descendant is `IDIV(RANGE(axis_r_q), c)`
// or `IMOD(RANGE(axis_r_q), c)` with c >= 2.  Used by `udg_x_addr_is_1x1`
// to positively detect "no kh/kw decomposition exists on r_q in the X
// address."  Capped depth.
static int udg_x_has_rq_divmod(Term t, u32 axis_r_q, int depth) {
  if (depth > 16) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_IDIV || op == UOP_IMOD) {
    Term num = heap_read(term_val(t) + 0);
    Term den = heap_read(term_val(t) + 1);
    if (term_tag(num) == TAG_UOP && term_ext(num) == UOP_RANGE
        && uop_range_axis_id(num) == axis_r_q
        && term_tag(den) == TAG_UOP && term_ext(den) == UOP_CONST) {
      Term dnum = heap_read(term_val(den) + 0);
      if (term_tag(dnum) == TAG_NUM && (u32)term_val(dnum) >= 2) return 1;
    }
  }
  // Recurse into binary / ternary / unary ops.  RANGE / CONST / BUFFER
  // have no relevant sub-terms.
  if (op == UOP_RANGE || op == UOP_CONST || op == UOP_BUFFER) return 0;
  if (op == UOP_IWHERE) {
    if (udg_x_has_rq_divmod(heap_read(term_val(t) + 0), axis_r_q, depth + 1)) return 1;
    if (udg_x_has_rq_divmod(heap_read(term_val(t) + 1), axis_r_q, depth + 1)) return 1;
    return udg_x_has_rq_divmod(heap_read(term_val(t) + 2), axis_r_q, depth + 1);
  }
  // Most other UOps store 2 operand terms; INDEX_E stores [buf, addr]
  // (we only need to descend into addr but recursing into both is safe).
  // Defensive: read 2 children.  Walking pure-leaf ops (CONST/RANGE)
  // already returned 0 above.
  if (op == UOP_IADD || op == UOP_IMUL || op == UOP_ISUB || op == UOP_IDIV
      || op == UOP_IMOD || op == UOP_ILT  || op == UOP_IAND
      || op == UOP_IOR  || op == UOP_IXOR || op == UOP_ISHR) {
    Term a = heap_read(term_val(t) + 0);
    Term b = heap_read(term_val(t) + 1);
    if (udg_x_has_rq_divmod(a, axis_r_q, depth + 1)) return 1;
    return udg_x_has_rq_divmod(b, axis_r_q, depth + 1);
  }
  return 0;
}

// Returns 1 iff the X address has NO `IDIV(RANGE(r_q), c)` and NO
// `IMOD(RANGE(r_q), c)` for c >= 2 anywhere in the tree.  This is the
// positive 1x1 detector.
static int udg_x_addr_is_1x1(Term addr_x, u32 axis_r_q) {
  return !udg_x_has_rq_divmod(addr_x, axis_r_q, 0);
}

// Match `IMOD(IDIV(RANGE(r_out), inner), outer)`.  Recovers (inner,
// outer).  Used to identify both the H arm (`IMOD(IDIV(r_out, w_out),
// h_out)`) and the BI arm (`IMOD(IDIV(r_out, spatial), batch)`) shape
// without committing to which is which (disambiguation done later).
static int udg_match_imod_idiv_range(Term t, u32 axis_r_out,
                                     u32 *out_inner, u32 *out_outer) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  Term div_num = 0; u32 div_div = 0;
  if (!udg_match_idiv_const(mod_num, &div_num, &div_div)) return 0;
  if (!udg_is_range_axis(div_num, axis_r_out)) return 0;
  *out_inner = div_div;
  *out_outer = mod_div;
  return 1;
}

// Match bare `IMOD(RANGE(r_out), c)` -- the W-arm shape when x_s1==1
// in the 1x1 case.  Recovers c (= w_out).
static int udg_match_imod_range(Term t, u32 axis_r_out, u32 *out_div) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  if (!udg_is_range_axis(mod_num, axis_r_out)) return 0;
  *out_div = mod_div;
  return 1;
}

// 1x1 conv parser.  Mirrors the standard parser's signature; called as
// a delegate from the standard parser's entry point when the X address
// has no IDIV/IMOD on r_q (i.e. kh==kw==1 collapsed).
//
// Strategy:
//   1. W addr decode (reuses `udg_decode_conv_w_addr`).  Recovers
//      patches, w_s0, w_s1, w_offset, axis_r_out, axis_r_q.
//   2. Flatten X addr into IADD leaves.  Each leaf is one of:
//        * ICONST -> x_offset
//        * IMUL(IMOD(IDIV(r_out, A), B), C) or bare IMOD(IDIV(...), B)
//          -- candidate H or BI arm (both look the same!)
//        * IMUL(IMOD(r_out, w_out), C) or bare IMOD(r_out, w_out)
//          -- W arm (x_s1>1 vs ==1)
//        * IMUL(RANGE(r_q), C) or bare RANGE(r_q)
//          -- CI arm (x_s2>1 vs ==1)
//   3. Disambiguate H vs BI by trying both assignments; the consistent
//      one (spatial == h_out * w_out, batch * spatial == patches) wins.
//   4. Cross-validate w_out from H arm against w_out from W arm; CI arm
//      must witness c_in via r_q.extent (since kh*kw=1, c_in=KRED).
static int uop_dag_extract_conv2d_flat_shape_1x1(
    Term addr_w, Term addr_x,
    UopDagConv2dFlatShape *out) {
  // --- W decode (same as standard parser) ---
  u32 axis_r_out = 0, axis_r_q = 0;
  u32 patches = 0, w_s0 = 0, w_s1 = 0;
  i32 w_off = 0;
  if (!udg_decode_conv_w_addr(addr_w, &axis_r_out, &axis_r_q,
                              &patches, &w_s0, &w_s1, &w_off)) {
    return 0;
  }

  // --- X flatten ---
  Term x_leaves[UDG_CONV_ADDR_MAX_LEAVES] = {0};
  u32 n_x = udg_flatten_iadd(addr_x, x_leaves, UDG_CONV_ADDR_MAX_LEAVES);
  if (n_x == 0) return 0;

  // --- Classify ---
  i32 x_off = 0;
  // CI arm (RANGE(r_q) variants):
  int got_ci = 0; u32 x_s2 = 0;
  // W arm (IMOD(r_out, w_out) variants):
  int got_w = 0; u32 x_s1 = 0; u32 w_out_w = 0;
  // Candidate H/BI arms (IMOD-of-IDIV(r_out, ...)).  Up to 2.
  struct { Term t; u32 inner; u32 outer; u32 coeff; int has_coeff; } cand[2];
  u32 n_cand = 0;

  for (u32 i = 0; i < n_x; i++) {
    Term t = x_leaves[i];
    i32 cv;
    if (udg_match_iconst_signed(t, &cv)) { x_off = cv; continue; }

    // CI arm: bare RANGE(r_q) or IMUL(RANGE(r_q), x_s2).
    if (!got_ci) {
      if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_RANGE
          && uop_range_axis_id(t) == axis_r_q) {
        x_s2 = 1; got_ci = 1; continue;
      }
      Term inner_t = 0; u32 inner_c = 0;
      if (udg_match_imul_const(t, &inner_t, &inner_c)
          && term_tag(inner_t) == TAG_UOP && term_ext(inner_t) == UOP_RANGE
          && uop_range_axis_id(inner_t) == axis_r_q) {
        x_s2 = inner_c; got_ci = 1; continue;
      }
    }

    // W arm: IMOD(r_out, w_out) bare (x_s1==1) or wrapped in IMUL.
    if (!got_w) {
      u32 w_out_t = 0;
      if (udg_match_imod_range(t, axis_r_out, &w_out_t)) {
        x_s1 = 1; w_out_w = w_out_t; got_w = 1; continue;
      }
      Term inner_t = 0; u32 inner_c = 0;
      if (udg_match_imul_const(t, &inner_t, &inner_c)
          && udg_match_imod_range(inner_t, axis_r_out, &w_out_t)) {
        x_s1 = inner_c; w_out_w = w_out_t; got_w = 1; continue;
      }
    }

    // H or BI candidate: IMOD(IDIV(r_out, inner), outer) bare or wrapped
    // in IMUL.  We can't classify yet -- stash and disambiguate later.
    {
      u32 inner_d = 0, outer_d = 0;
      if (udg_match_imod_idiv_range(t, axis_r_out, &inner_d, &outer_d)) {
        if (n_cand >= 2) return 0;  // too many; bail
        cand[n_cand].t         = t;
        cand[n_cand].inner     = inner_d;
        cand[n_cand].outer     = outer_d;
        cand[n_cand].coeff     = 1;
        cand[n_cand].has_coeff = 0;
        n_cand++;
        continue;
      }
      Term inner_t = 0; u32 inner_c = 0;
      if (udg_match_imul_const(t, &inner_t, &inner_c)
          && udg_match_imod_idiv_range(inner_t, axis_r_out,
                                       &inner_d, &outer_d)) {
        if (n_cand >= 2) return 0;
        cand[n_cand].t         = t;
        cand[n_cand].inner     = inner_d;
        cand[n_cand].outer     = outer_d;
        cand[n_cand].coeff     = inner_c;
        cand[n_cand].has_coeff = 1;
        n_cand++;
        continue;
      }
    }

    // Unrecognised leaf -- bail.
    return 0;
  }

  // The H arm must be present.  W arm absent only if w_out==1 (which
  // would have collapsed `IMOD(r_out, 1)` to 0); that case is rare and
  // out of scope -- bail.
  if (!got_w) return 0;
  if (n_cand == 0) return 0;
  if (!got_ci) return 0;

  // --- Disambiguate H vs BI via divisor values ---
  // H arm: inner == w_out, outer == h_out.
  // BI arm: inner == spatial == h_out * w_out, outer == batch.
  //
  // Constraints:
  //   * h_out > 0, w_out > 0, batch > 0
  //   * w_out_w (from W arm) == w_out (from H arm)
  //   * batch * spatial == patches  AND  spatial == h_out * w_out
  //
  // For batch == 1 there is no BI arm, so n_cand == 1.
  // For batch >= 2 the lifter emits both, so n_cand == 2.
  u32 batch = 0, spatial = 0, h_out = 0, w_out = 0, x_sb = 0;
  int got_bi = 0;

  if (n_cand == 1) {
    // batch == 1 case: the single candidate is the H arm.
    // inner == w_out, outer == h_out.  spatial = patches.
    h_out = cand[0].outer;
    w_out = cand[0].inner;
    if (w_out != w_out_w) return 0;
    if (h_out == 0 || w_out == 0) return 0;
    if ((u64)h_out * (u64)w_out != patches) return 0;
    batch = 1;
    spatial = patches;
    x_sb = 0;
  } else {
    // batch >= 2 case: try both pairings.  Exactly one should be
    // self-consistent.
    int hits = 0;
    int h_idx = -1;
    for (int try_h = 0; try_h < 2; try_h++) {
      int b_idx = 1 - try_h;
      u32 try_w_out = cand[try_h].inner;
      u32 try_h_out = cand[try_h].outer;
      u32 try_spatial = cand[b_idx].inner;
      u32 try_batch   = cand[b_idx].outer;
      if (try_w_out == 0 || try_h_out == 0 || try_batch == 0) continue;
      if (try_w_out != w_out_w) continue;
      if ((u64)try_h_out * (u64)try_w_out != try_spatial) continue;
      if ((u64)try_batch * (u64)try_spatial != patches) continue;
      // Consistent.
      hits++;
      h_idx = try_h;
      h_out = try_h_out;
      w_out = try_w_out;
      spatial = try_spatial;
      batch = try_batch;
    }
    if (hits != 1) return 0;  // ambiguous or no fit
    int b_idx = 1 - h_idx;
    // Determine x_sb (BI coeff) and x_s0 (H coeff).
    x_sb = cand[b_idx].has_coeff ? cand[b_idx].coeff : 1;
    if (cand[h_idx].has_coeff) {
      // x_s0 stored in cand[h_idx].coeff; we'll read below.
    }
    got_bi = 1;
  }

  // Recover x_s0 from the H candidate (whichever index we chose).
  u32 x_s0 = 0;
  if (n_cand == 1) {
    x_s0 = cand[0].has_coeff ? cand[0].coeff : 1;
  } else {
    // Re-derive H index by matching inner==w_out.
    int h_idx = (cand[0].inner == w_out && cand[0].outer == h_out) ? 0 : 1;
    x_s0 = cand[h_idx].has_coeff ? cand[h_idx].coeff : 1;
  }

  // --- Pull r_out / r_q extents to derive c_out and c_in ---
  u32 r_out_ext = rec_tc_find_range_extent(addr_w, axis_r_out, 0);
  u32 r_q_ext   = rec_tc_find_range_extent(addr_w, axis_r_q,   0);
  if (r_out_ext == 0 || r_q_ext == 0) return 0;
  if (r_out_ext % patches != 0) return 0;
  u32 c_out = r_out_ext / patches;
  // For 1x1 conv: KRED == c_in * 1 * 1 == c_in.
  u32 c_in  = r_q_ext;

  // --- Fill out ---
  out->c_out     = c_out;
  out->c_in      = c_in;
  out->kh        = 1;
  out->kw        = 1;
  out->h_out     = h_out;
  out->w_out     = w_out;
  out->batch     = batch;
  out->patches   = patches;
  out->spatial_patches = spatial;
  out->w_offset  = w_off;
  out->w_stride0 = (i32)w_s0;
  out->w_stride1 = (i32)w_s1;
  out->x_offset  = x_off;
  out->x_stride_b = (i32)x_sb;
  out->x_stride0  = (i32)x_s0;
  out->x_stride1  = (i32)x_s1;
  out->x_stride2  = (i32)x_s2;
  out->axis_r_out = axis_r_out;
  out->axis_r_q   = axis_r_q;
  (void)got_bi;
  return 1;
}

// Public extractor: run on `addr_w` + `addr_x` (single-input INDEX_E
// addresses) plus the lift's input slots, and fill the conv shape facts
// into `out`.  Returns 1 on success.  On 0 the caller must fall back to
// the input_views[] reader.  All four "X arms" must decode (BI / CI / H
// / W).  Out-of-scope: x_offset==0 is fine (just no const arm); but
// degenerate `kh==kw==1` collapses ci to bare RANGE -- not handled yet.
int uop_dag_extract_conv2d_flat_shape(Term addr_w, Term addr_x,
                                      UopDagConv2dFlatShape *out) {
  if (out == NULL) return 0;
  memset(out, 0, sizeof(*out));

  // --- W decode ---
  u32 axis_r_out = 0, axis_r_q = 0;
  u32 patches = 0, w_s0 = 0, w_s1 = 0;
  i32 w_off = 0;
  if (!udg_decode_conv_w_addr(addr_w, &axis_r_out, &axis_r_q,
                              &patches, &w_s0, &w_s1, &w_off)) {
    return 0;
  }

  // 1x1 conv detection: if X address has no IDIV/IMOD on r_q, the kh/kw
  // decomposition collapsed.  Delegate to the dedicated 1x1 parser
  // (which uses divisor-value disambiguation since the H and BI arms are
  // structurally identical).
  if (udg_x_addr_is_1x1(addr_x, axis_r_q)) {
    return uop_dag_extract_conv2d_flat_shape_1x1(addr_w, addr_x, out);
  }

  // --- X decode ---
  Term x_leaves[UDG_CONV_ADDR_MAX_LEAVES] = {0};
  u32 n_x = udg_flatten_iadd(addr_x, x_leaves, UDG_CONV_ADDR_MAX_LEAVES);
  if (n_x == 0) return 0;

  i32 x_off    = 0;
  u32 x_sb     = 0,  batch     = 0,  spatial = 0;
  u32 x_s2     = 0,  kw_ci     = 0,  kh_ci   = 0;
  u32 x_s0     = 0,  h_out_h   = 0,  w_out_h = 0,  kw_h = 0,  kh_h = 0;
  u32 x_s1     = 0,  w_out_w   = 0,  kw_w    = 0;
  int got_h    = 0,  got_w     = 0,  got_ci  = 0,  got_bi = 0;

  // First pass: classify each leaf into one of:
  //   CONST -> x_offset
  //   H_ARM (IMUL with IADD inside)            -- distinctive: IMUL(IADD(...), C)
  //   CI_ARM (IDIV-of-IDIV; coefficient maybe) -- distinctive: IMUL(IDIV(IDIV(r_q,..),..), C) or bare
  //   BI_ARM (IMOD-of-IDIV)                    -- IMUL(IMOD(IDIV(r_out,..),..), C) or bare
  //   "ow" leaf  (IMOD(r_out, w_out))          -- when x_s1==1 the W arm dissolved
  //   "kw_v" leaf (IMOD(r_q, kw))              -- partner of "ow"
  // Then pair the ow + kw_v leaves into a synthetic w-arm.
  Term ow_leaf = 0, kw_v_leaf = 0;
  u32 ow_w_out = 0, kw_v_kw = 0;

  for (u32 i = 0; i < n_x; i++) {
    Term t = x_leaves[i];
    i32 cv;
    if (udg_match_iconst_signed(t, &cv)) { x_off = cv; continue; }
    if (!got_h && udg_decode_h_arm(t, axis_r_out, axis_r_q,
                                   &x_s0, &w_out_h, &h_out_h, &kw_h, &kh_h)) {
      got_h = 1; continue;
    }
    // Full IMUL-wrapped W-arm (x_s1>1 case).
    if (!got_w) {
      Term inner_imul = 0; u32 inner_coeff = 0;
      if (udg_match_imul_const(t, &inner_imul, &inner_coeff)
          && term_tag(inner_imul) == TAG_UOP
          && term_ext(inner_imul) == UOP_IADD) {
        u32 wo = 0, kw1 = 0;
        Term lhs = heap_read(term_val(inner_imul) + 0);
        Term rhs = heap_read(term_val(inner_imul) + 1);
        if ((udg_match_ow(lhs, axis_r_out, &wo)
             && udg_match_kw_v(rhs, axis_r_q, &kw1))
            || (udg_match_ow(rhs, axis_r_out, &wo)
                && udg_match_kw_v(lhs, axis_r_q, &kw1))) {
          x_s1 = inner_coeff; w_out_w = wo; kw_w = kw1; got_w = 1; continue;
        }
      }
    }
    if (!got_ci && udg_decode_ci_arm(t, axis_r_q, &x_s2, &kw_ci, &kh_ci)) {
      got_ci = 1; continue;
    }
    if (!got_bi && udg_decode_bi_arm(t, axis_r_out,
                                     &x_sb, &spatial, &batch)) {
      got_bi = 1; continue;
    }
    // Disaggregated W-arm (x_s1==1): a bare IMOD(r_out, w_out) "ow" leaf
    // OR a bare IMOD(r_q, kw) "kw_v" leaf.  Stash; pair them up after.
    if (ow_leaf == 0 && udg_match_ow(t, axis_r_out, &ow_w_out)) {
      ow_leaf = t; continue;
    }
    if (kw_v_leaf == 0 && udg_match_kw_v(t, axis_r_q, &kw_v_kw)) {
      kw_v_leaf = t; continue;
    }
    return 0;  // unrecognised arm
  }

  // Pair up ow + kw_v into a synthetic W-arm if the IMUL collapsed
  // (x_s1==1 case).
  if (!got_w && ow_leaf != 0 && kw_v_leaf != 0) {
    x_s1 = 1; w_out_w = ow_w_out; kw_w = kw_v_kw; got_w = 1;
  } else if (ow_leaf != 0 || kw_v_leaf != 0) {
    // Only one of the pair was found -- inconsistent; bail.
    return 0;
  }

  // Required: H and W arms.  CI optional only if c_in==1+kh*kw==KRED
  // (out of scope for this session); BI optional only if batch==1.
  if (!got_h || !got_w) return 0;

  // Cross-checks: w_out from H arm == w_out from W arm; kw consistent;
  // kh consistent (if both got_ci and got_h supply it).
  if (w_out_h != w_out_w) return 0;
  if (kw_h != kw_w) return 0;
  if (got_ci) {
    if (kw_ci != kw_h) return 0;
    if (kh_ci != kh_h) return 0;
  }

  // Recover c_in / batch defaults when arms collapsed.
  u32 kw = kw_h;
  u32 kh = kh_h;
  u32 h_out = h_out_h;
  u32 w_out = w_out_h;
  if (kw == 0 || kh == 0) return 0;
  if (h_out == 0 || w_out == 0) return 0;

  // Pull r_out / r_q extents to derive c_out and KRED.
  // The W addr's IDIV-of-RANGE referenced r_out via axis id; recover
  // the actual extent by walking back into the IADD leaves.
  // Simpler: read extents from the lifted RANGE nodes inside the addr.
  u32 r_out_ext = 0, r_q_ext = 0;
  // r_out lives inside the W addr's co-arm as IDIV(r_out, patches).
  // r_q lives as either bare RANGE leaf or IMUL(RANGE, w_s1).
  // Both are easy to find via rec_tc_find_range_extent.
  r_out_ext = rec_tc_find_range_extent(addr_w, axis_r_out, 0);
  r_q_ext   = rec_tc_find_range_extent(addr_w, axis_r_q,   0);
  if (r_out_ext == 0 || r_q_ext == 0) return 0;
  if (r_out_ext % patches != 0) return 0;
  u32 c_out = r_out_ext / patches;
  u32 KRED  = r_q_ext;

  // c_in derivation.  KRED == c_in * kh * kw, so c_in = KRED / (kh*kw).
  if (KRED == 0 || kh * kw == 0) return 0;
  if (KRED % (kh * kw) != 0) return 0;
  u32 c_in = KRED / (kh * kw);
  if (c_in == 0) return 0;
  // Out of scope: c_in==1 ci-arm collapsed; reject for now.
  if (c_in > 1 && !got_ci) return 0;

  // batch derivation: if BI arm absent, batch == 1 and the spatial
  // factor equals patches.
  if (!got_bi) {
    batch   = 1;
    spatial = patches;
  } else {
    if (spatial == 0 || batch == 0) return 0;
    if (spatial * batch != patches) return 0;
  }
  if (spatial != h_out * w_out) return 0;

  // x_stride_b: for batch==1 we have no info; default 0 (matches the
  // input_views reader behavior for ndim==3 inputs).
  if (!got_bi) x_sb = 0;

  // Recover input slot indices from BUFFER.instance.  The W INDEX_E
  // points to W's BUFFER, and the X INDEX_E points to X's BUFFER.
  // Caller provides addr_w/addr_x at the level INSIDE INDEX_E so we
  // can't read BUFFER directly here -- but the slot can be deduced
  // by the caller.  Leave w_input/x_input zero here; callers fill.

  out->c_out     = c_out;
  out->c_in      = c_in;
  out->kh        = kh;
  out->kw        = kw;
  out->h_out     = h_out;
  out->w_out     = w_out;
  out->batch     = batch;
  out->patches   = patches;
  out->spatial_patches = spatial;
  out->w_offset  = w_off;
  out->w_stride0 = (i32)w_s0;
  out->w_stride1 = (i32)w_s1;
  out->x_offset  = x_off;
  out->x_stride_b = (i32)x_sb;
  out->x_stride0  = (i32)x_s0;
  out->x_stride1  = (i32)x_s1;
  out->x_stride2  = (i32)x_s2;
  out->axis_r_out = axis_r_out;
  out->axis_r_q   = axis_r_q;
  return 1;
}

// === Hand-coded-opts helper: DAG axis enumeration ====================
// Walk the DAG rooted at `root` and collect every distinct UOP_RANGE
// leaf, ordered by ascending axis_id.  Writes (axis_id, axis_type,
// extent) triples into the parallel out_* arrays.  Returns the count
// (clipped to `cap`); 0 if `root` is 0 / not a UOp / no ranges.
//
// Used by kernel_hand_coded_opts (src/codegen/hand_opts.c) to inspect
// the current axis structure of a lifted kernel between successive
// kernel_apply_opt calls (each apply mutates the DAG, so the axis list
// must be re-queried).  Mirrors `apply_opt_dag_collect_ranges` in
// uop/apply_opt_dag.c but de-dups + sorts by axis_id and exposes the
// decoded fields rather than the raw Terms.
u32 uop_dag_collect_axes(Term root, u32 *out_axis_id, u32 *out_axis_type,
                         u32 *out_extent, u32 cap) {
  if (root == 0 || term_tag(root) != TAG_UOP || cap == 0) return 0;
  Term stack[512];
  u32  sp = 0;
  stack[sp++] = root;
  // Collect distinct RANGE terms (dedup by axis_id; the lifter emits
  // one RANGE per axis_id and apply_opt rebuilds them, so axis_id is a
  // safe identity here -- two RANGEs with the same axis_id but
  // different extent shouldn't co-exist in a well-formed DAG).
  u32 n = 0;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      u32 aid  = uop_range_axis_id(t);
      u32 at   = uop_range_axis_type(t);
      u32 ext  = uop_range_extent(t);
      int seen = 0;
      for (u32 i = 0; i < n; i++) if (out_axis_id[i] == aid) { seen = 1; break; }
      if (!seen && n < cap) {
        out_axis_id[n]   = aid;
        out_axis_type[n] = at;
        out_extent[n]    = ext;
        n++;
      }
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      Term tgt = uop_opt_target(t);
      if (term_tag(tgt) == TAG_UOP && sp < 512) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 512; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  // Insertion-sort by axis_id (n is tiny -- <= MAX_AXES).
  for (u32 i = 1; i < n; i++) {
    u32 aid = out_axis_id[i], at = out_axis_type[i], ext = out_extent[i];
    i32 j = (i32)i - 1;
    while (j >= 0 && out_axis_id[j] > aid) {
      out_axis_id[j + 1]   = out_axis_id[j];
      out_axis_type[j + 1] = out_axis_type[j];
      out_extent[j + 1]    = out_extent[j];
      j--;
    }
    out_axis_id[j + 1]   = aid;
    out_axis_type[j + 1] = at;
    out_extent[j + 1]    = ext;
  }
  return n;
}

// === DAG-side generalized-contraction extractor =======================
//
// Walks an INDEX_E's symbolic address and, for every distinct UOP_RANGE
// axis_id it references, accumulates the linear coefficient that range
// contributes to the address.  Supports the lift_scalar_index shape
// where the address is a left-leaning IADD chain whose leaves are one
// of {RANGE, IMUL(RANGE, ICONST), IMUL(ICONST, RANGE), ICONST}.  Bare
// ICONST leaves are accumulated as an absolute offset.
//
// On success:
//   *n_out                = number of distinct axes (0..cap)
//   out_axis_ids[k]       = axis_id of the k-th distinct axis
//   out_coeffs[k]         = total linear coefficient (sum of arms with
//                           that axis_id; typically 1 arm per axis)
//   *out_offset           = sum of bare ICONST leaves (signed-safe via i32 cast)
//   *out_ok = 1 on full decode; 0 on bail (unrecognized leaf shape).
//
// Used by the contraction classifier to map per-operand address shapes
// to (M, N, K)-axis classification + stride extraction.
#define UDG_CONTRACT_MAX_AXES 8
typedef struct {
  u32 axis_ids[UDG_CONTRACT_MAX_AXES];
  u32 coeffs  [UDG_CONTRACT_MAX_AXES];
  u32 n_axes;
  i32 offset;
  int ok;
} UdgAddrCoeffs;

static int udg_addr_accumulate(UdgAddrCoeffs *a, u32 axis_id, u32 coeff) {
  for (u32 i = 0; i < a->n_axes; i++) {
    if (a->axis_ids[i] == axis_id) {
      // Same axis appearing twice in the address (e.g. lifter merged two
      // arms): accumulate coefficients.
      a->coeffs[i] += coeff;
      return 1;
    }
  }
  if (a->n_axes >= UDG_CONTRACT_MAX_AXES) return 0;
  a->axis_ids[a->n_axes] = axis_id;
  a->coeffs  [a->n_axes] = coeff;
  a->n_axes++;
  return 1;
}

// Recursively accumulate `coeff * leaf` into the coeff table.  Supports
// the leaf shapes that survive thvm's index_simplify pass for conv im2col:
//   RANGE                                    -> accumulate axis @ coeff
//   CONST                                    -> add to offset
//   IADD(L, R)                               -> recurse(L, coeff) + recurse(R, coeff)
//   IMUL(X, CONST(c)) or IMUL(CONST(c), X)   -> recurse(X, coeff * c)
//
// IMUL distribution over IADD mirrors tinygrad symbolic.py:236
//   (UPat.cvar("y") * (UPat.var("x", weakint) + UPat.cvar("c")),
//    lambda x,y,c: (y*x)+(y*c))                          # y*(x+c) -> y*x + y*c
// which is the canonical-form rewrite the index simplifier applies.  thvm's
// index_simplify doesn't currently push that rule, so we apply the
// distribution lazily here while walking the symbolic address.  The conv
// weight-grad im2col fingerprint
//   in[a4*18432 + a1*576 + (a5+a2)*24 + (a6+a3)]
// then decodes to coeffs {a4:18432, a1:576, a5:24, a2:24, a6:1, a3:1} with
// the same coefficient appearing on the K-axis (a5,a6) and the M-axis
// (a2,a3) -- i.e. the strided-im2col signature the contraction classifier
// recognises.  Returns 1 on success, 0 on bail.
static int udg_addr_decode_leaf(Term t, u32 coeff, UdgAddrCoeffs *out) {
  if (term_tag(t) == TAG_UOP) {
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      return udg_addr_accumulate(out, uop_range_axis_id(t), coeff);
    }
    if (op == UOP_IADD) {
      Term a = heap_read(term_val(t) + 0);
      Term b = heap_read(term_val(t) + 1);
      if (!udg_addr_decode_leaf(a, coeff, out)) return 0;
      if (!udg_addr_decode_leaf(b, coeff, out)) return 0;
      return 1;
    }
    if (op == UOP_OPT) {
      // OPT(inner, kind, k) wraps the inner subtree with a renderer hint
      // (UPCAST / UNROLL / GROUP_REDUCE / TC / ...).  For address-coeff
      // decoding the wrapper is transparent: the underlying RANGE leaf
      // contributes its axis stride exactly as before.  Without this
      // unwrap, decoding bails on every post-UPCAST address (because
      // apply_opt_dag wraps the new inner RANGE in OPT(_, UPCAST, k)),
      // which breaks the multi-UPCAST stride heuristic.
      return udg_addr_decode_leaf(uop_opt_target(t), coeff, out);
    }
    Term inner = 0; u32 c = 0;
    if (udg_match_imul_const(t, &inner, &c)) {
      // (coeff * c) needs to fit in u32; bail on overflow.
      u64 prod = (u64)coeff * (u64)c;
      if (prod > 0xFFFFFFFFu) return 0;
      return udg_addr_decode_leaf(inner, (u32)prod, out);
    }
  }
  i32 ival = 0;
  if (udg_match_iconst_signed(t, &ival)) {
    // CONST contributes coeff * ival to the absolute offset.
    out->offset += (i32)((i64)ival * (i64)coeff);
    return 1;
  }
  // Unrecognized leaf (IDIV/IMOD/IWHERE for conv2d-flat); bail.
  return 0;
}

static void udg_decode_addr_coeffs(Term addr, UdgAddrCoeffs *out) {
  out->n_axes = 0;
  out->offset = 0;
  out->ok     = 0;
  // Single entry point handles every shape (bare RANGE, bare CONST,
  // IMUL(RANGE, CONST), IADD chain, IMUL(IADD, CONST) -- the last is
  // the im2col-fingerprint case C7.1 added).
  if (udg_addr_decode_leaf(addr, 1, out)) out->ok = 1;
}

// Look up the coefficient for an axis_id in a decoded address.  Returns
// 0 if the axis isn't referenced.
static u32 udg_addr_lookup(UdgAddrCoeffs const *a, u32 axis_id) {
  for (u32 i = 0; i < a->n_axes; i++) {
    if (a->axis_ids[i] == axis_id) return a->coeffs[i];
  }
  return 0;
}

// Find the UOP_RANGE leaf in the kernel DAG with the requested axis_id
// and return its extent (statically resolved if a literal; max upper
// bound if a kvar).  0 if not found.
static u32 udg_axis_extent(Term root, u32 want_axis_id) {
  if (root == 0 || term_tag(root) != TAG_UOP) return 0;
  Term stack[256];
  u32 sp = 0;
  stack[sp++] = root;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      if (uop_range_axis_id(t) == want_axis_id) return uop_range_static_extent(t);
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      Term tgt = uop_opt_target(t);
      if (term_tag(tgt) == TAG_UOP && sp < 256) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 256; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return 0;
}

// Public entry: classify the kernel as a generalized tensor contraction
// (M-axes, N-axes, single K-axis) and recover the batched-GEMM dispatch
// shape.  See thvm.h for the contract.
//
// Strategy:
//   1. Peel optional UOP_OPT(_, TC) wrapper from STORE.value.
//   2. Verify STORE.value is REDUCE(MUL(INDEX_E(A, addr_a),
//                                       INDEX_E(B, addr_b))) with
//      A != B and kind == REDUCE_SUM.
//   3. Decode addr_a, addr_b, addr_out via udg_decode_addr_coeffs.
//   4. Reduce axis must reference exactly ONE axis (single K), and that
//      axis must appear in BOTH addr_a and addr_b but NOT in addr_out.
//   5. Partition: M_axes = (addr_b minus K, must equal addr_out minus N_axes).
//                 N_axes = (addr_a minus K, must equal addr_out minus M_axes).
//   6. Verify the N-axis composition in addr_a (= addr_out) is contiguous
//      with stride product N_total, matching K's stride in A == N_total.
//   7. Identify the "batch" M-axis as the M-axis with the LARGEST stride
//      in B, and verify its stride == K_extent * inner_M_total.
//      Remaining M-axes (inner_M) must be contiguous in B with strides
//      matching their composition (stride product == inner_M_total).
//   8. Verify the output's M-axis strides match (batch outer stride =
//      inner_M_total * N_total; inner M strides = N_total * inner stride).
int uop_dag_classify_contraction_shape(Term root,
                                       struct KernelEntry const *ke,
                                       UopDagContractionShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  // Peel optional UOP_OPT(_, TC) wrapper, structurally validate.
  Term buf_out  = heap_read(term_val(root) + 0);
  Term addr_out = heap_read(term_val(root) + 1);
  Term value    = heap_read(term_val(root) + 2);
  Term peeled   = udg_peel_tc_opt(value);
  Term reduce   = peeled;
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  u32 rkind = uop_reduce_kind(reduce);
  if (rkind != REDUCE_SUM) return 0;
  // Multi-K support: collect EVERY reduce axis_id.  Each must appear
  // in BOTH A and B address coefficients, never in the output, and
  // they must compose into one flat K dimension for cblas_sgemm.
  // Mirrors tinygrad's einsum: out[M,N] = sum_{k1, k2, ...} A[K,N] * B[M,K]
  // collapses to A[K_total, N] @ B[M, K_total].
  u32 n_K = uop_reduce_n_axes(reduce);
  if (n_K == 0 || n_K > UDG_CONTRACT_MAX_AXES) return 0;
  u32 red_axis_ids[UDG_CONTRACT_MAX_AXES];
  for (u32 i = 0; i < n_K; i++) red_axis_ids[i] = uop_reduce_axis(reduce, i);
  u32 red_axis_id = red_axis_ids[0];   // legacy single-K alias (still used for
                                       // single-K storage-stride checks below
                                       // when n_K == 1).
  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term a_idx = heap_read(term_val(mul) + 0);
  Term b_idx = heap_read(term_val(mul) + 1);
  if (term_tag(a_idx) != TAG_UOP || term_ext(a_idx) != UOP_INDEX_E) return 0;
  if (term_tag(b_idx) != TAG_UOP || term_ext(b_idx) != UOP_INDEX_E) return 0;

  Term buf_a = heap_read(term_val(a_idx) + 0);
  Term buf_b = heap_read(term_val(b_idx) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) return 0;
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) return 0;
  if (term_val(buf_a) == term_val(buf_b)) return 0;
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  u32 inst_a = uop_buffer_inst_get(buf_a);
  u32 inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) return 0;
  u32 a_input = inst_a - 1;
  u32 b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs) return 0;

  Term addr_a = heap_read(term_val(a_idx) + 1);
  Term addr_b = heap_read(term_val(b_idx) + 1);

  UdgAddrCoeffs ca = {0}, cb = {0}, co = {0};
  udg_decode_addr_coeffs(addr_a,  &ca);
  udg_decode_addr_coeffs(addr_b,  &cb);
  udg_decode_addr_coeffs(addr_out,&co);
  if (!ca.ok || !cb.ok || !co.ok) return 0;
  // Any non-zero address offsets => non-contiguous storage we don't
  // currently route.  Bail.
  if (ca.offset != 0 || cb.offset != 0 || co.offset != 0) return 0;

  // Per-K-axis check: every K axis must appear in BOTH A and B coeffs,
  // never in the output.  Collect per-axis extent + per-input coeff so
  // we can later compose them into one flat K dimension.
  u32 K_extents[UDG_CONTRACT_MAX_AXES];
  u32 K_a_coeffs[UDG_CONTRACT_MAX_AXES];
  u32 K_b_coeffs[UDG_CONTRACT_MAX_AXES];
  u64 K_total = 1;
  for (u32 i = 0; i < n_K; i++) {
    u32 aid = red_axis_ids[i];
    u32 ac  = udg_addr_lookup(&ca, aid);
    u32 bc  = udg_addr_lookup(&cb, aid);
    u32 oc  = udg_addr_lookup(&co, aid);
    if (ac == 0 || bc == 0 || oc != 0) return 0;
    u32 ext = udg_axis_extent(root, aid);
    if (ext == 0) return 0;
    K_extents [i] = ext;
    K_a_coeffs[i] = ac;
    K_b_coeffs[i] = bc;
    K_total *= ext;
  }
  if (K_total == 0 || K_total > 0x7fffffffULL) return 0;
  // Multi-K compose: A treats the K axes as a [K_0, K_1, ...] row-
  // major block (descending a_coeff, smallest = the single-K innermost
  // stride for A).  B likewise treats them as a row-major block
  // (descending b_coeff, smallest = single-K innermost for B).  When
  // both inputs agree on the K composition we can collapse to one
  // flat K for cblas_sgemm without strided dispatch (since the K
  // strides are dense within each input).
  u32 K_in_a_single = K_a_coeffs[0];
  u32 K_in_b_single = K_b_coeffs[0];
  if (n_K > 1) {
    // Sort indices by descending a_coeff so the row-major dense check
    // can verify each subsequent K-coeff = prev * inner extents.
    u32 ord_a[UDG_CONTRACT_MAX_AXES];
    for (u32 i = 0; i < n_K; i++) ord_a[i] = i;
    for (u32 i = 1; i < n_K; i++) {
      u32 tmp = ord_a[i]; i32 j = (i32)i - 1;
      while (j >= 0 && K_a_coeffs[ord_a[j]] < K_a_coeffs[tmp]) {
        ord_a[j + 1] = ord_a[j]; j--;
      }
      ord_a[j + 1] = tmp;
    }
    // Same sort for B.  For the simple GEMM dispatch we require BOTH
    // inputs sort K-axes in the SAME order (i.e. K axes are dense in
    // both A and B with the same nesting).  Different sorts would
    // require either a transpose or a strided-K dispatch (C7's
    // strided im2col path); bail for now.
    u32 ord_b[UDG_CONTRACT_MAX_AXES];
    for (u32 i = 0; i < n_K; i++) ord_b[i] = i;
    for (u32 i = 1; i < n_K; i++) {
      u32 tmp = ord_b[i]; i32 j = (i32)i - 1;
      while (j >= 0 && K_b_coeffs[ord_b[j]] < K_b_coeffs[tmp]) {
        ord_b[j + 1] = ord_b[j]; j--;
      }
      ord_b[j + 1] = tmp;
    }
    for (u32 i = 0; i < n_K; i++) if (ord_a[i] != ord_b[i]) return 0;
    // K_in_a_single (innermost K stride in A) corresponds to the LAST
    // entry in the sorted order; ditto for B.  These will be used as
    // K_in_a / K_in_b lookups below.
    K_in_a_single = K_a_coeffs[ord_a[n_K - 1]];
    K_in_b_single = K_b_coeffs[ord_b[n_K - 1]];
    // Verify the row-major composition: for descending order,
    // coeff[i] = coeff[i+1] * extent[i+1] (i.e. each outer step covers
    // the product of all inner K extents at its inner side).
    u64 acc_a = K_in_a_single;
    u64 acc_b = K_in_b_single;
    for (i32 i = (i32)n_K - 2; i >= 0; i--) {
      acc_a *= K_extents[ord_a[i + 1]];
      acc_b *= K_extents[ord_b[i + 1]];
      if (K_a_coeffs[ord_a[i]] != (u32)acc_a) return 0;
      if (K_b_coeffs[ord_b[i]] != (u32)acc_b) return 0;
    }
  }
  // Legacy aliases for single-K storage-stride checks below.  After
  // multi-K compose, K_in_a / K_in_b refer to the COMPOSITE K's
  // innermost (smallest) stride -- equivalent to a single-K of size
  // K_total at that stride.
  u32 K_in_a = K_in_a_single;
  u32 K_in_b = K_in_b_single;
  u32 K_extent = (u32)K_total;

  // Output axes: every output axis must appear in EITHER A or B (not
  // both) -- if in A only it's an N-axis; if in B only it's an M-axis.
  // Build M_axes (axis_id, extent, B_coeff, OUT_coeff) and N_axes
  // (axis_id, extent, A_coeff, OUT_coeff).
  typedef struct { u32 axis_id; u32 extent; u32 b_coeff; u32 out_coeff; } UdgMAxis;
  typedef struct { u32 axis_id; u32 extent; u32 a_coeff; u32 out_coeff; } UdgNAxis;
  UdgMAxis M_axes[UDG_CONTRACT_MAX_AXES];
  UdgNAxis N_axes[UDG_CONTRACT_MAX_AXES];
  u32 n_M = 0, n_N = 0;
  for (u32 i = 0; i < co.n_axes; i++) {
    u32 aid = co.axis_ids[i];
    u32 oc  = co.coeffs  [i];
    if (oc == 0) return 0;
    u32 a_c = udg_addr_lookup(&ca, aid);
    u32 b_c = udg_addr_lookup(&cb, aid);
    u32 ext = udg_axis_extent(root, aid);
    if (ext == 0) return 0;
    if (a_c != 0 && b_c == 0) {
      if (n_N >= UDG_CONTRACT_MAX_AXES) return 0;
      N_axes[n_N].axis_id   = aid;
      N_axes[n_N].extent    = ext;
      N_axes[n_N].a_coeff   = a_c;
      N_axes[n_N].out_coeff = oc;
      n_N++;
    } else if (b_c != 0 && a_c == 0) {
      if (n_M >= UDG_CONTRACT_MAX_AXES) return 0;
      M_axes[n_M].axis_id   = aid;
      M_axes[n_M].extent    = ext;
      M_axes[n_M].b_coeff   = b_c;
      M_axes[n_M].out_coeff = oc;
      n_M++;
    } else {
      // Either in both inputs (would-be K, but already excluded), or in
      // neither, or in output only -- not a clean contraction.
      return 0;
    }
  }
  if (n_M == 0 || n_N == 0) return 0;

  // Every axis referenced by A must be one of the K_axes or one of the
  // N_axes, and every axis referenced by B must be K or one of the
  // M_axes.  Guarantees we've found the FULL contraction shape.
  for (u32 i = 0; i < ca.n_axes; i++) {
    u32 aid = ca.axis_ids[i];
    int is_K = 0;
    for (u32 j = 0; j < n_K; j++) if (red_axis_ids[j] == aid) { is_K = 1; break; }
    if (is_K) continue;
    int found = 0;
    for (u32 j = 0; j < n_N; j++) if (N_axes[j].axis_id == aid) { found = 1; break; }
    if (!found) return 0;
  }
  for (u32 i = 0; i < cb.n_axes; i++) {
    u32 aid = cb.axis_ids[i];
    int is_K = 0;
    for (u32 j = 0; j < n_K; j++) if (red_axis_ids[j] == aid) { is_K = 1; break; }
    if (is_K) continue;
    int found = 0;
    for (u32 j = 0; j < n_M; j++) if (M_axes[j].axis_id == aid) { found = 1; break; }
    if (!found) return 0;
  }

  // Compute N_total = product of N_axes extents.
  u64 N_total = 1;
  for (u32 i = 0; i < n_N; i++) N_total *= N_axes[i].extent;
  if (N_total == 0 || N_total > 0x7fffffffULL) return 0;

  // A operand: must be a clean (K, N) row-major slice.
  //   Sort N_axes by descending a_coeff.  Expected layout: largest
  //   a_coeff first, each subsequent coeff = prev_coeff / prev_extent,
  //   smallest coeff == 1, and K_in_a == N_total.
  if ((u32)N_total != K_in_a) return 0;
  // Sort N_axes by descending a_coeff (insertion sort, n_N tiny).
  for (u32 i = 1; i < n_N; i++) {
    UdgNAxis tmp = N_axes[i];
    i32 j = (i32)i - 1;
    while (j >= 0 && N_axes[j].a_coeff < tmp.a_coeff) {
      N_axes[j + 1] = N_axes[j];
      j--;
    }
    N_axes[j + 1] = tmp;
  }
  // For sorted N_axes (descending stride), stride[n-1] == 1 and
  // stride[i] * extent[i] == stride[i-1], with stride[0] * extent[0]
  // == N_total.  Output address's coefficient for each N-axis must
  // match the A operand's coefficient (since N-stride is identical in
  // both A and output for a contiguous (M, N) output layout).
  {
    u64 acc = 1;
    for (i32 i = (i32)n_N - 1; i >= 0; i--) {
      if (N_axes[i].a_coeff != (u32)acc) return 0;
      // Output address must match A's N-coefficient.
      if (N_axes[i].out_coeff != (u32)acc) return 0;
      acc *= N_axes[i].extent;
    }
    if (acc != N_total) return 0;
  }

  // M operand (B): identify batch axis as the M-axis with the LARGEST
  // b_coeff.  Sort M_axes by descending b_coeff.
  for (u32 i = 1; i < n_M; i++) {
    UdgMAxis tmp = M_axes[i];
    i32 j = (i32)i - 1;
    while (j >= 0 && M_axes[j].b_coeff < tmp.b_coeff) {
      M_axes[j + 1] = M_axes[j];
      j--;
    }
    M_axes[j + 1] = tmp;
  }
  // For the simple batched-GEMM dispatch, decompose M as (batch =
  // M_axes[0], inner_M = M_axes[1..n_M-1]).  Inner M axes must be
  // contiguous in B with stride = product of extents to the right
  // ending at 1.  Batch axis must have b_coeff = K_extent * inner_M_total.
  // (When n_M == 1, batch == 1 and inner_M == M_axes[0].)
  u64 inner_M_total = 1;
  u32 batch         = 1;
  if (n_M == 1) {
    // Single M-axis: no batching needed (treat all of M as inner_M).
    inner_M_total = M_axes[0].extent;
    batch         = 1;
    // For the inner_M block to be contiguous in B, b_coeff must equal 1,
    // and K_in_b must equal inner_M_total (K stride == inner_M_total)
    // OR b_coeff == K_extent and K_in_b == 1 (would be transposed; not
    // currently routed -- bail).
    if (M_axes[0].b_coeff != 1) return 0;
    if (K_in_b != inner_M_total) return 0;
  } else {
    // batch = M_axes[0].extent, inner_M = remaining.
    batch = M_axes[0].extent;
    for (u32 i = 1; i < n_M; i++) inner_M_total *= M_axes[i].extent;
    // Inner M axes contig in B.
    u64 acc = 1;
    for (i32 i = (i32)n_M - 1; i >= 1; i--) {
      if (M_axes[i].b_coeff != (u32)acc) return 0;
      acc *= M_axes[i].extent;
    }
    if (acc != inner_M_total) return 0;
    // K's stride in B must equal inner_M_total (K immediately wraps
    // inner_M in B's memory).
    if (K_in_b != inner_M_total) return 0;
    // Batch axis's stride in B must equal K_extent * inner_M_total
    // (batch wraps the [K, inner_M] block).
    u64 expect_batch_stride_b = (u64)K_extent * inner_M_total;
    if (M_axes[0].b_coeff != (u32)expect_batch_stride_b) return 0;
  }
  if (inner_M_total == 0 || inner_M_total > 0x7fffffffULL) return 0;
  u64 M_total = (u64)batch * inner_M_total;
  if (M_total > 0x7fffffffULL) return 0;

  // Verify OUTPUT M-axis strides.  M_axes is sorted by descending
  // b_coeff which equals descending out_coeff (for a contiguous output
  // layout where M comes before N).  Output layout: [batch, inner_M,
  // N], strides [inner_M_total*N_total, N_total*(...), N_total].
  // For n_M == 1: out_coeff of M_axes[0] should equal N_total.
  // For n_M > 1: batch out_coeff = inner_M_total*N_total; subsequent
  // inner-M out_coeff = N_total * (prefix of inner-M extents to the
  // right).
  {
    u64 acc = N_total;
    for (i32 i = (i32)n_M - 1; i >= 0; i--) {
      if (M_axes[i].out_coeff != (u32)acc) return 0;
      acc *= M_axes[i].extent;
    }
  }

  // Verify per-buffer storage sizes.
  u64 a_needed = (u64)K_extent * N_total;
  u64 b_needed = (u64)batch * K_extent * inner_M_total;
  u64 c_needed = (u64)batch * inner_M_total * N_total;
  (void)c_needed;
  if (uop_buffer_ndim(buf_a) == 0 || uop_buffer_ndim(buf_b) == 0) return 0;
  // Storage-size cross-check on buffer dims:
  u64 a_buf_prod = 1, b_buf_prod = 1;
  for (u32 d = 0; d < uop_buffer_ndim(buf_a); d++)
    a_buf_prod *= uop_buffer_dim(buf_a, d);
  for (u32 d = 0; d < uop_buffer_ndim(buf_b); d++)
    b_buf_prod *= uop_buffer_dim(buf_b, d);
  if (a_buf_prod < a_needed) return 0;
  if (b_buf_prod < b_needed) return 0;

  // Uniform dtype check (matches uop_dag_classify_matmul_shape).
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32) return 0;       // only fp32 routed for now
  if (uop_buffer_dtype(buf_a) != dt) return 0;
  if (uop_buffer_dtype(buf_b) != dt) return 0;

  out->dtype          = dt;
  out->a_input        = a_input;
  out->b_input        = b_input;
  out->K              = K_extent;
  out->N              = (u32)N_total;
  out->inner_M        = (u32)inner_M_total;
  out->batch          = batch;
  out->ldA            = (u32)N_total;          // A's row stride in (K, N)
  out->ldB            = (u32)inner_M_total;    // B's row stride in (K, inner_M)
  out->ldC            = (u32)N_total;          // C's row stride in (inner_M, N)
  out->batch_stride_a = 0;                     // A has no batch axis
  out->batch_stride_b = (batch == 1) ? 0 : (u32)((u64)K_extent * inner_M_total);
  out->batch_stride_c = (batch == 1) ? 0 : (u32)(inner_M_total * N_total);
  out->flags          = 0;
  return 1;
}

// === TRUE batched gemm classifier (batch axis in A AND B AND output) =====
// A{batch,M,K} . B{batch,K,N} -> C{batch,M,N}: each batch an independent gemm.
// The bmm in TMultiHeadAttention's batched path is the canonical instance
// (batch = nHeads).  Operands are PERMUTED views, so ld/trans/batch-stride are
// recovered straight from the address coefficients -- NO canonical-layout
// validation (which uop_dag_classify_matmul_shape applies and which would
// reject a permuted operand).  Declines whenever anything is non-affine or
// non-unit-inner, leaving the kernel to the (correct) codegen path.
int uop_dag_classify_batched_gemm(Term root, struct KernelEntry const *ke,
                                  UopDagBatchedGemmShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  // Structural validation done INLINE -- udg_classify_matmul_store enforces
  // exactly 2 range axes per operand (via uop_classify_matmul), which rejects
  // the 3-axis {batch,M,K} batched operand.
  Term reduce = udg_peel_tc_opt(heap_read(term_val(root) + 2));
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  if (uop_reduce_kind(reduce) != REDUCE_SUM) return 0;
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis = uop_reduce_axis(reduce, 0);
  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term a_idx = heap_read(term_val(mul) + 0);
  Term b_idx = heap_read(term_val(mul) + 1);
  if (term_tag(a_idx) != TAG_UOP || term_ext(a_idx) != UOP_INDEX_E) return 0;
  if (term_tag(b_idx) != TAG_UOP || term_ext(b_idx) != UOP_INDEX_E) return 0;
  u32 k_extent = udg_axis_extent(root, red_axis);
  if (k_extent < 2) return 0;
  Term buf_a   = heap_read(term_val(a_idx) + 0);
  Term buf_b   = heap_read(term_val(b_idx) + 0);
  Term buf_out = heap_read(term_val(root) + 0);
  if (term_tag(buf_a)   != TAG_UOP || term_ext(buf_a)   != UOP_BUFFER) return 0;
  if (term_tag(buf_b)   != TAG_UOP || term_ext(buf_b)   != UOP_BUFFER) return 0;
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  if (term_val(buf_a) == term_val(buf_b)) return 0;
  u32 inst_a = uop_buffer_inst_get(buf_a), inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) return 0;
  u32 a_input = inst_a - 1, b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs || a_input == b_input) return 0;

  UdgAddrCoeffs ca = {0}, cb = {0}, co = {0};
  udg_decode_addr_coeffs(heap_read(term_val(a_idx) + 1), &ca);
  udg_decode_addr_coeffs(heap_read(term_val(b_idx) + 1), &cb);
  udg_decode_addr_coeffs(heap_read(term_val(root)  + 1), &co);
  if (!ca.ok || !cb.ok || !co.ok) return 0;
  if (ca.offset != 0 || cb.offset != 0 || co.offset != 0) return 0;
  // K in A and B, never in the output.
  if (udg_addr_lookup(&ca, red_axis) == 0) return 0;
  if (udg_addr_lookup(&cb, red_axis) == 0) return 0;
  if (udg_addr_lookup(&co, red_axis) != 0) return 0;
  // Output axes: in A only -> M; in B only -> N; in BOTH -> batch.  Require
  // exactly one of each (the simple single-batch gemm).
  u32 m_axis = 0, n_axis = 0, batch_axis = 0, n_M = 0, n_N = 0, n_batch = 0;
  for (u32 i = 0; i < co.n_axes; i++) {
    u32 aid = co.axis_ids[i];
    if (aid == red_axis) return 0;
    u32 ac = udg_addr_lookup(&ca, aid), bc = udg_addr_lookup(&cb, aid);
    if      (ac != 0 && bc == 0) { m_axis = aid; n_M++; }
    else if (bc != 0 && ac == 0) { n_axis = aid; n_N++; }
    else if (ac != 0 && bc != 0) { batch_axis = aid; n_batch++; }
    else return 0;
  }
  if (n_M != 1 || n_N != 1 || n_batch != 1) return 0;
  // Every A axis must be K/M/batch; every B axis K/N/batch (full shape).
  for (u32 i = 0; i < ca.n_axes; i++) {
    u32 aid = ca.axis_ids[i];
    if (aid != red_axis && aid != m_axis && aid != batch_axis) return 0;
  }
  for (u32 i = 0; i < cb.n_axes; i++) {
    u32 aid = cb.axis_ids[i];
    if (aid != red_axis && aid != n_axis && aid != batch_axis) return 0;
  }
  u32 a_K = udg_addr_lookup(&ca, red_axis), a_M = udg_addr_lookup(&ca, m_axis);
  u32 b_K = udg_addr_lookup(&cb, red_axis), b_N = udg_addr_lookup(&cb, n_axis);
  u32 c_M = udg_addr_lookup(&co, m_axis),   c_N = udg_addr_lookup(&co, n_axis);
  if (a_K == 0 || a_M == 0 || b_K == 0 || b_N == 0 || c_M == 0 || c_N == 0) return 0;
  // ld/trans from the unit-stride (contiguous inner) axis -- mirrors
  // uop_dag_classify_matmul_shape's convention WITHOUT its canonical check.
  u32 transA, ldA;
  if      (a_K == 1) { transA = 0; ldA = a_M; }
  else if (a_M == 1) { transA = 1; ldA = a_K; }
  else return 0;
  u32 transB, ldB;
  if      (b_N == 1) { transB = 0; ldB = b_K; }
  else if (b_K == 1) { transB = 1; ldB = b_N; }
  else return 0;
  if (c_N != 1) return 0;            // output must be row-contiguous {..,M,N}
  u32 ldC = c_M;
  u32 M = udg_axis_extent(root, m_axis);
  u32 N = udg_axis_extent(root, n_axis);
  u32 batch = udg_axis_extent(root, batch_axis);
  if (M < 1 || N < 1 || batch < 2) return 0;   // batch<2 -> blas_try_gemm's job
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32) return 0;
  if (uop_buffer_dtype(buf_a) != dt || uop_buffer_dtype(buf_b) != dt) return 0;

  out->dtype   = dt;
  out->a_input = a_input; out->b_input = b_input;
  out->M = M; out->N = N; out->K = k_extent; out->batch = batch;
  out->ldA = ldA; out->ldB = ldB; out->ldC = ldC;
  out->transA = transA; out->transB = transB;
  out->batch_stride_a = udg_addr_lookup(&ca, batch_axis);
  out->batch_stride_b = udg_addr_lookup(&cb, batch_axis);
  out->batch_stride_c = udg_addr_lookup(&co, batch_axis);
  // Per-batch element span (max addressed index + 1) for buffer bounds checks.
  out->a_span = (M - 1) * a_M + (k_extent - 1) * a_K + 1;
  out->b_span = (k_extent - 1) * b_K + (N - 1) * b_N + 1;
  out->c_span = (M - 1) * c_M + (N - 1) * c_N + 1;
  return 1;
}

// === C7.2: im2col conv-bwd weight-grad contraction classifier ===========
//
// Recognises the conv-backward weight-grad pattern
//
//   dW[Cout, Cin, KH, KW] = sum_{B, OH, OW}
//                            dY[B, Cout, OH, OW] * X[B, Cin, OH+KH, OW+KW]
//
// where C7.1's distributing decoder splits the IMUL(IADD(OH, KH), W) +
// IADD(OW, KW) arms so the X address coefficients are
//   {B: Cin*H*W, Cin: H*W, OH: W, KH: W, OW: 1, KW: 1}
// (OH/KH and OW/KW share strides -- the im2col fingerprint).  Output
// axes Cout and Cin are clean (B-only or A-only); KH, KW each share an
// A-stride with one of the K-axes (OH, OW), so they're reclassified as
// "patch-M" axes.  The dispatch then materialises a per-batch im2col
// patches buffer and calls cblas_sgemm once per B.
//
// Reference: tinygrad lowers conv via `_pool` (mixin/movement.py:569),
// which expresses the patches as a ShapeTracker view; their codegen
// either inlines it or materialises a view.  thvm doesn't yet have a
// matching _pool lowering, so this classifier-plus-dispatch combo is
// the equivalent shortcut: detect the same pattern in the lifted DAG
// and route directly to cblas_sgemm.
int uop_dag_classify_im2col_contraction(Term root,
                                        struct KernelEntry const *ke,
                                        UopDagIm2colShape *out) {
#define IM2COL_REJ(tag) return 0
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) IM2COL_REJ("n_inputs<2");
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) IM2COL_REJ("not-store");

  Term buf_out  = heap_read(term_val(root) + 0);
  Term addr_out = heap_read(term_val(root) + 1);
  Term value    = heap_read(term_val(root) + 2);
  Term peeled   = udg_peel_tc_opt(value);
  Term reduce   = peeled;
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) IM2COL_REJ("not-reduce");
  if (uop_reduce_kind(reduce) != REDUCE_SUM) IM2COL_REJ("not-sum");

  // Multi-K reduce required: exactly 3 axes for (B, OH, OW) -- otherwise
  // the im2col fingerprint can't form.  Single-K conv-bwd is handled by
  // the regular contraction classifier.
  u32 n_K = uop_reduce_n_axes(reduce);
  if (n_K < 2 || n_K > UDG_CONTRACT_MAX_AXES) IM2COL_REJ("n_K-bounds");
  u32 red_axis_ids[UDG_CONTRACT_MAX_AXES];
  for (u32 i = 0; i < n_K; i++) red_axis_ids[i] = uop_reduce_axis(reduce, i);

  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) IM2COL_REJ("not-mul");
  Term a_idx = heap_read(term_val(mul) + 0);
  Term b_idx = heap_read(term_val(mul) + 1);
  if (term_tag(a_idx) != TAG_UOP || term_ext(a_idx) != UOP_INDEX_E) IM2COL_REJ("a-not-idx");
  if (term_tag(b_idx) != TAG_UOP || term_ext(b_idx) != UOP_INDEX_E) IM2COL_REJ("b-not-idx");

  Term buf_a = heap_read(term_val(a_idx) + 0);
  Term buf_b = heap_read(term_val(b_idx) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) IM2COL_REJ("a-not-buf");
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) IM2COL_REJ("b-not-buf");
  if (term_val(buf_a) == term_val(buf_b)) IM2COL_REJ("a==b");
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) IM2COL_REJ("out-not-buf");
  u32 inst_a = uop_buffer_inst_get(buf_a);
  u32 inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) IM2COL_REJ("inst-0");
  u32 a_input = inst_a - 1;
  u32 b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs) IM2COL_REJ("input-oob");

  Term addr_a = heap_read(term_val(a_idx) + 1);
  Term addr_b = heap_read(term_val(b_idx) + 1);
  UdgAddrCoeffs ca = {0}, cb = {0}, co = {0};
  udg_decode_addr_coeffs(addr_a,  &ca);
  udg_decode_addr_coeffs(addr_b,  &cb);
  udg_decode_addr_coeffs(addr_out,&co);
  if (!ca.ok || !cb.ok || !co.ok) IM2COL_REJ("addr-decode");
  if (ca.offset != 0 || cb.offset != 0 || co.offset != 0) IM2COL_REJ("addr-offset");

  // For each K-axis (reduce), check whether its A-coefficient is shared
  // with some output axis (im2col patch fingerprint).  Per K-axis we
  // record (a-coeff, b-coeff, extent).  The im2col fingerprint requires
  // that EVERY non-batch K-axis pair with a patch-M (same A-coeff as an
  // output axis); the unpaired K-axis is the outer batch K (B).
  //
  // For the conv-wgrad case:
  //   K = (B, OH, OW)   patch-M = (KH paired with OH; KW paired with OW)
  // K-axis ordering (sorted by A-coeff descending) determines the
  // batch/row/col splits:
  //   K_outer = sorted_K[0] (B, no patch pair)
  //   K_row   = sorted_K[1] (OH, paired with KH)
  //   K_col   = sorted_K[2] (OW, paired with KW; innermost)
  typedef struct {
    u32 aid; u32 a_coeff; u32 b_coeff; u32 extent;
    u32 patch_aid; u32 patch_extent; u32 patch_out_coeff;
    int has_patch;
  } KEntry;
  KEntry K[UDG_CONTRACT_MAX_AXES];
  for (u32 i = 0; i < n_K; i++) {
    K[i].aid = red_axis_ids[i];
    K[i].a_coeff = udg_addr_lookup(&ca, K[i].aid);
    K[i].b_coeff = udg_addr_lookup(&cb, K[i].aid);
    K[i].extent  = udg_axis_extent(root, K[i].aid);
    if (K[i].a_coeff == 0 || K[i].b_coeff == 0 || K[i].extent == 0) IM2COL_REJ("K-coeff-0");
    // K-axis must not be a direct output axis.
    if (udg_addr_lookup(&co, K[i].aid) != 0) IM2COL_REJ("K-also-out");
    K[i].has_patch = 0;
  }

  // Walk output axes.  Each output axis is one of:
  //   - patch-M: a-coeff is non-zero AND matches some K-axis's a-coeff
  //     AND b-coeff is 0 (output axis must not appear in B for patch).
  //   - clean N (non-patch): a-coeff is non-zero, doesn't match any K
  //     a-coeff, b-coeff is 0.
  //   - clean M: b-coeff is non-zero, a-coeff is 0.
  typedef struct { u32 aid; u32 extent; u32 a_coeff; u32 out_coeff; } NAxisRec;
  typedef struct { u32 aid; u32 extent; u32 b_coeff; u32 out_coeff; } MAxisRec;
  NAxisRec N_axes[UDG_CONTRACT_MAX_AXES];
  MAxisRec M_axes[UDG_CONTRACT_MAX_AXES];
  u32 n_N = 0, n_M = 0, n_patch = 0;
  for (u32 i = 0; i < co.n_axes; i++) {
    u32 aid = co.axis_ids[i];
    u32 oc  = co.coeffs  [i];
    if (oc == 0) IM2COL_REJ("oc-0");
    u32 ac = udg_addr_lookup(&ca, aid);
    u32 bc = udg_addr_lookup(&cb, aid);
    u32 ext = udg_axis_extent(root, aid);
    if (ext == 0) IM2COL_REJ("oc-ext-0");
    if (ac != 0 && bc == 0) {
      // Either patch-M or clean N.  Check stride-share with any K-axis.
      int paired = 0;
      for (u32 j = 0; j < n_K; j++) {
        if (!K[j].has_patch && K[j].a_coeff == ac) {
          // Patch-M: bind to this K-axis.
          K[j].has_patch       = 1;
          K[j].patch_aid       = aid;
          K[j].patch_extent    = ext;
          K[j].patch_out_coeff = oc;
          n_patch++;
          paired = 1;
          break;
        }
      }
      if (!paired) {
        if (n_N >= UDG_CONTRACT_MAX_AXES) IM2COL_REJ("n_N-overflow");
        N_axes[n_N].aid       = aid;
        N_axes[n_N].extent    = ext;
        N_axes[n_N].a_coeff   = ac;
        N_axes[n_N].out_coeff = oc;
        n_N++;
      }
    } else if (bc != 0 && ac == 0) {
      if (n_M >= UDG_CONTRACT_MAX_AXES) IM2COL_REJ("n_M-overflow");
      M_axes[n_M].aid       = aid;
      M_axes[n_M].extent    = ext;
      M_axes[n_M].b_coeff   = bc;
      M_axes[n_M].out_coeff = oc;
      n_M++;
    } else {
      IM2COL_REJ("ac-bc-both-or-neither");  // not a clean im2col contraction
    }
  }
  // Need at least one patch-M and one clean M for the dispatch to be
  // useful.  (Pure clean-N case is the regular contraction.)
  if (n_patch == 0) IM2COL_REJ("n_patch=0");
  if (n_M != 1) IM2COL_REJ("n_M!=1");
  if (n_N > 1) IM2COL_REJ("n_N>1");
  if (n_patch > 2) IM2COL_REJ("n_patch>2");

  // Sort K by descending a_coeff so K[0] is the outermost (batch), K[n_K-1]
  // innermost.  Use insertion sort (n_K tiny, <=4).
  for (u32 i = 1; i < n_K; i++) {
    KEntry tmp = K[i];
    i32 j = (i32)i - 1;
    while (j >= 0 && K[j].a_coeff < tmp.a_coeff) {
      K[j+1] = K[j]; j--;
    }
    K[j+1] = tmp;
  }
  // Innermost K-axis (K_col) must have a-coeff == 1.
  if (K[n_K-1].a_coeff != 1) IM2COL_REJ("K_col.a!=1");
  // Outermost (K_outer) and middle (K_row) when n_K==3 must be unpaired
  // (K_outer) and paired (K_row).  In C7.2 scope: n_K must be 3 (B, OH, OW).
  if (n_K != 3) IM2COL_REJ("n_K!=3");
  if (K[0].has_patch != 0) IM2COL_REJ("K0-paired");
  if (K[1].has_patch != 1) IM2COL_REJ("K1-unpaired");
  if (K[2].has_patch != 1) IM2COL_REJ("K2-unpaired");

  // ----- BWD vs FWD discriminator + canonical extent mapping -----
  //
  // The two duals share the n_K=3, n_patch=2, n_M=1 structural shape with
  // OH<->KH and OW<->KW stride-shared (the im2col fingerprint).  They
  // differ in which member of each pair is the K-axis and which is the
  // patch (output) axis:
  //
  //   BWD: K = (B, OH, OW)            patches = (KH paired with OH,
  //                                              KW paired with OW)
  //        M (Cout) lives in dY: b_coeff = OH*OW.
  //        K_outer (B) is ABOVE M in dY: b_coeff = M.ext * OH * OW.
  //
  //   FWD: K = (Cin, KH, KW)          patches = (OH paired with KH,
  //                                              OW paired with KW)
  //        M (Cout) lives in W:  b_coeff = Cin*KH*KW.
  //        K_outer (Cin) is INSIDE M in W: b_coeff = KH*KW.
  //
  // Discriminator: M.b_coeff selects direction; from there OH/OW/KH/KW
  // map onto K[1..2] vs patches consistently across the rest of the
  // function.
  u32 K_inner = K[2].extent;             // sort-order K[2] = KW (FWD) / OW (BWD)
  u32 K_middle = K[1].extent;            // sort-order K[1] = KH (FWD) / OH (BWD)
  u32 K_outer_ext = K[0].extent;
  if (K[2].b_coeff != 1) IM2COL_REJ("K2.b!=1");
  if (K[1].b_coeff != K_inner) IM2COL_REJ("K1.b!=K2.ext");
  u32 m_bcoeff_bwd = (u32)((u64)K_inner * K_middle);
  u32 m_bcoeff_fwd = (u32)((u64)K_outer_ext * K_inner * K_middle);
  int forward_conv;
  if (M_axes[0].b_coeff == m_bcoeff_bwd) {
    forward_conv = 0;
    if (K[0].b_coeff !=
        (u32)((u64)M_axes[0].extent * K_inner * K_middle)) IM2COL_REJ("bwd:K0.b!=M*OH*OW");
  } else if (M_axes[0].b_coeff == m_bcoeff_fwd) {
    forward_conv = 1;
    if (K[0].b_coeff != (u32)((u64)K_inner * K_middle)) IM2COL_REJ("fwd:K0.b!=KH*KW");
    if (n_N != 1) IM2COL_REJ("fwd:n_N!=1");  // BWD allows 0 or 1
  } else {
    IM2COL_REJ("M.b-no-match");
  }

  // Canonical extent mapping: OH/OW are always output-pixel rows/cols,
  // KH/KW are always kernel-window rows/cols, regardless of direction.
  // In FWD K[1..2] hold KH/KW and patches hold OH/OW; in BWD it's flipped.
  u32 OH_ext, OW_ext, KH_ext, KW_ext;
  if (forward_conv) {
    OH_ext = K[1].patch_extent;  OW_ext = K[2].patch_extent;
    KH_ext = K_middle;           KW_ext = K_inner;
  } else {
    OH_ext = K_middle;           OW_ext = K_inner;
    KH_ext = K[1].patch_extent;  KW_ext = K[2].patch_extent;
  }

  // Outer batch role + Cin role.  In BWD the K_outer K-axis IS the batch
  // (B); in FWD the clean-N N-axis IS the batch and K_outer plays Cin.
  // Either way, X_Cin_stride = H*W and X_outer_stride = N_outer * H * W.
  u32 X_Cin_stride;       // X stride per Cin step (= H*W)
  u32 N_extent_used;      // Cin extent (1 if no explicit Cin axis)
  u32 X_outer_stride;     // X stride per outer-batch step (= Cin*H*W)
  u32 N_outer_extent;     // outer-batch loop bound (B in both forms)
  if (forward_conv) {
    X_Cin_stride   = K[0].a_coeff;       // K_outer = Cin axis
    N_extent_used  = K_outer_ext;
    X_outer_stride = N_axes[0].a_coeff;  // clean-N = B axis
    N_outer_extent = N_axes[0].extent;
  } else if (n_N == 1) {
    X_Cin_stride   = N_axes[0].a_coeff;  // clean-N = Cin axis
    N_extent_used  = N_axes[0].extent;
    X_outer_stride = K[0].a_coeff;       // K_outer = B axis
    N_outer_extent = K_outer_ext;
  } else {
    X_Cin_stride   = K[0].a_coeff;       // Cin=1 -> one H*W slab
    N_extent_used  = 1;
    X_outer_stride = K[0].a_coeff;
    N_outer_extent = K_outer_ext;
  }
  if (X_Cin_stride == 0 || X_outer_stride == 0) IM2COL_REJ("X-stride-0");

  // Verify X layout: width=W, height=H, X_Cin = H*W, X_outer = Cin*H*W.
  u32 X_W = K[1].a_coeff;          // K_middle a-stride = W
  if (X_W == 0) IM2COL_REJ("X_W=0");
  if (X_Cin_stride % X_W != 0) IM2COL_REJ("X_Cin%X_W");
  u32 X_H = X_Cin_stride / X_W;
  if (X_H == 0) IM2COL_REJ("X_H=0");
  if (X_outer_stride % X_Cin_stride != 0) IM2COL_REJ("X_outer%X_Cin");
  if (X_outer_stride / X_Cin_stride != N_extent_used) IM2COL_REJ("X_outer/X_Cin!=N");
  // Patch fits inside X: OH + KH <= X_H + 1, OW + KW <= X_W + 1.
  if (OH_ext + KH_ext > X_H + 1) IM2COL_REJ("OH+KH>X_H+1");
  if (OW_ext + KW_ext > X_W + 1) IM2COL_REJ("OW+KW>X_W+1");

  // Verify output strides.
  //   BWD: dW is (Cout, Cin, KH, KW) flat -- M.oc = N_total = Cin*KH*KW,
  //        Cin.oc = KH*KW, KH.oc = KW, KW.oc = 1.  (Cin axis is absent
  //        when n_N==0 and Cout.oc collapses to KH*KW.)
  //   FWD: out is (B, Cout, OH, OW) flat -- B.oc = Cout*OH*OW,
  //        Cout.oc = OH*OW, OH.oc = OW, OW.oc = 1.
  u64 N_total = (u64)N_extent_used * KH_ext * KW_ext;     // sgemm-N in BWD,
                                                          // sgemm-K in FWD
  u64 K_pixels = (u64)OH_ext * OW_ext;                    // sgemm-K in BWD,
                                                          // sgemm-N in FWD
  if (N_total == 0 || N_total > 0x7fffffffULL) IM2COL_REJ("N_total-bounds");
  if (forward_conv) {
    if (M_axes[0].out_coeff != (u32)K_pixels)                  IM2COL_REJ("fwd:M.oc");
    if (N_axes[0].out_coeff != (u32)((u64)M_axes[0].extent * K_pixels))
                                                               IM2COL_REJ("fwd:N.oc");
    if (K[1].patch_out_coeff != OW_ext)                        IM2COL_REJ("fwd:OH.oc");
    if (K[2].patch_out_coeff != 1)                             IM2COL_REJ("fwd:OW.oc");
  } else {
    if (M_axes[0].out_coeff != (u32)N_total)                   IM2COL_REJ("bwd:M.oc");
    if (n_N == 1 && N_axes[0].out_coeff != (u32)((u64)KH_ext * KW_ext))
                                                               IM2COL_REJ("bwd:N.oc");
    if (K[1].patch_out_coeff != KW_ext)                        IM2COL_REJ("bwd:KH.oc");
    if (K[2].patch_out_coeff != 1)                             IM2COL_REJ("bwd:KW.oc");
  }

  // Verify per-buffer storage sizes.  X is always (N_outer, Cin, H*W); the
  // direction-dependent operand is dY (BWD) vs W (FWD), and the output is
  // dW (BWD, no batch dim) vs out (FWD, has batch dim).
  u64 a_need = (u64)N_outer_extent * X_Cin_stride;
  u64 b_need = forward_conv ? (u64)M_axes[0].extent * N_total
                            : (u64)N_outer_extent * M_axes[0].extent * K_pixels;
  if (uop_buffer_ndim(buf_a) == 0 || uop_buffer_ndim(buf_b) == 0) IM2COL_REJ("buf-ndim-0");
  u64 a_buf_prod = 1, b_buf_prod = 1;
  for (u32 d = 0; d < uop_buffer_ndim(buf_a); d++) a_buf_prod *= uop_buffer_dim(buf_a, d);
  for (u32 d = 0; d < uop_buffer_ndim(buf_b); d++) b_buf_prod *= uop_buffer_dim(buf_b, d);
  if (a_buf_prod < a_need) IM2COL_REJ("a-buf-too-small");
  if (b_buf_prod < b_need) IM2COL_REJ("b-buf-too-small");

  // Uniform dtype check.
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32) IM2COL_REJ("dt!=fp32");
  if (uop_buffer_dtype(buf_a) != dt) IM2COL_REJ("a-dt");
  if (uop_buffer_dtype(buf_b) != dt) IM2COL_REJ("b-dt");

  // Populate the shape struct with semantic OH/OW/KH/KW extents (not
  // sort-order K[0..2].extent) so the dispatcher's gather + sgemm logic
  // is identical across BWD and FWD.  In BWD, OH/OW are K-axes and KH/KW
  // are patches; in FWD it's reversed -- but the gather and matmul still
  // see "OH = output-pixel rows" and "KH = kernel-window rows".
  out->dtype              = dt;
  out->a_input            = a_input;
  out->b_input            = b_input;
  out->M                  = M_axes[0].extent;
  out->N_patchless        = N_extent_used;
  out->KH_total           = (u32)((u64)KH_ext * KW_ext);
  out->N                  = (u32)N_total;
  out->K_row              = OH_ext;          // OH (output-pixel rows)
  out->K_col              = OW_ext;          // OW (output-pixel cols)
  out->K_outer            = K_outer_ext;
  out->X_W                = X_W;
  out->X_H                = X_H;
  out->X_Cin_stride       = X_Cin_stride;
  out->X_outer_stride     = X_outer_stride;
  out->Y_outer_stride     = forward_conv ? 0u : K[0].b_coeff;
  out->Y_M_stride         = M_axes[0].b_coeff;
  out->out_M_stride       = M_axes[0].out_coeff;
  out->patch_n            = 2;
  out->patch_extent[0]    = KH_ext;          // KH (kernel rows)
  out->patch_extent[1]    = KW_ext;          // KW (kernel cols)
  out->patch_out_coeff[0] = forward_conv ? OW_ext : KW_ext;  // OH.oc or KH.oc
  out->patch_out_coeff[1] = 1;
  out->patch_a_stride[0]  = X_W;             // KH stride in X (= image width)
  out->patch_a_stride[1]  = 1;               // KW stride in X
  out->forward_conv       = (u32)forward_conv;
  out->N_outer_extent     = N_outer_extent;
  out->N_outer_out_stride = forward_conv ? N_axes[0].out_coeff : 0u;
  return 1;
#undef IM2COL_REJ
}

// === Public wrappers around udg_decode_addr_coeffs / udg_addr_lookup
// (codegen/hand_opts.c needs them to implement tinygrad's stride heuristic).
// UDG_CONTRACT_MAX_AXES == UOP_DAG_ADDR_MAX_AXES; the structs are wire-
// compatible up to field order so we copy field-by-field.

void uop_dag_decode_addr_coeffs(Term addr, UopDagAddrCoeffsView *out) {
  if (out == NULL) return;
  UdgAddrCoeffs c;
  udg_decode_addr_coeffs(addr, &c);
  u32 n = c.n_axes;
  if (n > UOP_DAG_ADDR_MAX_AXES) n = UOP_DAG_ADDR_MAX_AXES;
  out->n_axes = n;
  out->offset = c.offset;
  out->ok     = c.ok;
  for (u32 i = 0; i < n; i++) {
    out->axis_ids[i] = c.axis_ids[i];
    out->coeffs  [i] = c.coeffs  [i];
  }
}

u32 uop_dag_addr_coeff_lookup(UopDagAddrCoeffsView const *a, u32 axis_id) {
  if (a == NULL) return 0;
  for (u32 i = 0; i < a->n_axes; i++) {
    if (a->axis_ids[i] == axis_id) return a->coeffs[i];
  }
  return 0;
}

u32 uop_dag_collect_index_e_addrs(Term root, Term *out_addrs, u32 cap) {
  if (root == 0 || term_tag(root) != TAG_UOP || out_addrs == NULL || cap == 0) {
    return 0;
  }
  Term stack[512];
  u32  sp = 0;
  stack[sp++] = root;
  u32 n = 0;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_INDEX_E) {
      Term addr = heap_read(term_val(t) + 1);
      int seen = 0;
      for (u32 i = 0; i < n; i++) if (out_addrs[i] == addr) { seen = 1; break; }
      if (!seen && n < cap) out_addrs[n++] = addr;
      // Still descend into INDEX_E's children (BUFFER + addr subtree)
      // in case nested INDEX_E live inside the addr (unlikely but cheap).
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID || op == UOP_RANGE) {
      continue;
    }
    if (op == UOP_OPT) {
      Term tgt = uop_opt_target(t);
      if (term_tag(tgt) == TAG_UOP && sp < 512) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 512; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return n;
}
