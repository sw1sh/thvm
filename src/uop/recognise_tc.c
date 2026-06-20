// uop/recognise_tc.c - pre-render pass that wraps the matmul shape
// with UOP_OPT(_, TC, 0).
//
// render_uop already pattern-matches OPT(REDUCE(MUL(INDEX_E, INDEX_E)),
// TC, 0) in rmu_detect_matmul_tc / rmu_emit_matmul_tc and emits the
// simdgroup_matrix MMA template. The piece missing for the F3 wedge
// (ideal-pipeline plan) is the producer side: someone has to install
// the OPT(_, TC, 0) annotation on the lifter output before the
// renderer sees it. That's this file.
//
// The recogniser is a single UPat: STORE(?out_buf, ?addr_out,
// REDUCE(MUL(INDEX_E(?A, ?addrA), INDEX_E(?B, ?addrB)))). When it
// matches AND A != B (matmul, not square-of-self), we rebuild the
// store with the OPT wrapper. Otherwise we return the input root
// unchanged.
//
// The recogniser does NOT need to verify that the K-extent is a
// multiple of 8 -- rmu_emit_matmul_tc handles tile-mismatch by
// emitting a "/* TC tile mismatch */" comment and falling back to
// the F1e accumulator path. So it's safe to install OPT(_, TC, 0)
// whenever the structural shape matches.

// Pattern leaves: bind A_buf at slot 0, A_addr at slot 1,
// B_buf at slot 2, B_addr at slot 3.
static UPat const rec_tc_a_buf  = { UOP_BUFFER,  0,    0, 0, NULL,           NULL };
static UPat const rec_tc_a_addr = { 0,           0xFF, 0, 1, NULL,           NULL };
static UPat const rec_tc_a_kids[2] = { rec_tc_a_buf, rec_tc_a_addr };
static UPat const rec_tc_a      = { UOP_INDEX_E, 2,    0, -1, rec_tc_a_kids, NULL };

static UPat const rec_tc_b_buf  = { UOP_BUFFER,  0,    0, 2, NULL,           NULL };
static UPat const rec_tc_b_addr = { 0,           0xFF, 0, 3, NULL,           NULL };
static UPat const rec_tc_b_kids[2] = { rec_tc_b_buf, rec_tc_b_addr };
static UPat const rec_tc_b      = { UOP_INDEX_E, 2,    0, -1, rec_tc_b_kids, NULL };

static UPat const rec_tc_mul_kids[2] = { rec_tc_a, rec_tc_b };
static UPat const rec_tc_mul    = { UOP_MUL,     2,    0, -1, rec_tc_mul_kids, NULL };

// REDUCE has uop_arity 1 -- src[0] is the body. UP1 trusts explicit
// nsrc, so nsrc=1 walks just the body slot. The kind/axis NUMs at
// loc+1, loc+2 are not Term children so we don't bind them here.
static UPat const rec_tc_reduce_kids[1] = { rec_tc_mul };
static UPat const rec_tc_reduce = { UOP_REDUCE,  1,    0, -1, rec_tc_reduce_kids, NULL };

// STORE heap layout: [buf, addr, value]. nsrc=3 walks all three.
// We don't bind buf / addr here -- the rebuilder reads them off the
// store's heap directly.
static UPat const rec_tc_store_buf  = { 0, 0xFF, 0, -1, NULL, NULL };
static UPat const rec_tc_store_addr = { 0, 0xFF, 0, -1, NULL, NULL };
static UPat const rec_tc_store_kids[3] = {
  rec_tc_store_buf, rec_tc_store_addr, rec_tc_reduce
};
static UPat const rec_tc_store = { UOP_STORE,   3,    0, -1, rec_tc_store_kids, NULL };

// Peel a single UOP_CAST/BITCAST wrapper to reach an underlying INDEX_E.
// A bf16-input gemm accumulating in f32 wraps each operand as
// CAST(INDEX_E(bf16_buf)); the matmul recognisers want the INDEX_E so the
// 3-range / 2-range structural classification can read the addresses.
static Term rec_tc_peel_cast(Term t) {
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  if (op == UOP_CAST || op == UOP_BITCAST) {
    Term src = heap_read(term_val(t) + 0);
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_INDEX_E) return src;
  }
  return t;
}

// THVM_FUSE_MATMUL_INPUT: when the A operand is a fused elementwise producer
// (not a bare INDEX_E -- see render_uop.c rmu_fused_producer_ok), recover a
// REPRESENTATIVE INDEX_E leaf so the 2-range / K-extent classification reads a
// real {m,k} address.  Every leaf in the producer indexes A's input buffers at
// the matmul's (m, k) iteration, so any one carries the canonical layout.
// Bounded recursion; returns 0 if no INDEX_E leaf.
static Term rec_tc_fused_first_index(Term t, int depth) {
  if (depth > 40 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_INDEX_E) return t;
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    Term r = rec_tc_fused_first_index(heap_read(term_val(t) + i), depth + 1);
    if (r != 0) return r;
  }
  return 0;
}

// Count distinct UOP_RANGE axis_ids reachable from `t` through the
// integer-binary / iwhere index expressions.  Used by the matmul
// classifier to reject shapes whose addresses reference more than 2
// ranges (e.g. conv2d's X address references {bi, ci, oh+kh, ow+kw}
// = 4+ ranges; matmul's m*K+k references exactly 2). Bounded depth
// so a misshapen DAG can't run away.
static u32 rec_tc_count_distinct_ranges(Term t, u32 *seen, u32 cap,
                                        u32 n_seen, int depth) {
  if (depth > 32) return n_seen;
  if (term_tag(t) != TAG_UOP) return n_seen;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 axis_id = (u32)term_val(heap_read(loc + 0));
    for (u32 i = 0; i < n_seen; i++) if (seen[i] == axis_id) return n_seen;
    if (n_seen < cap) seen[n_seen++] = axis_id;
    return n_seen;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
      n_seen = rec_tc_count_distinct_ranges(heap_read(loc + 0),
                                            seen, cap, n_seen, depth + 1);
      n_seen = rec_tc_count_distinct_ranges(heap_read(loc + 1),
                                            seen, cap, n_seen, depth + 1);
      return n_seen;
    case UOP_IWHERE:
      n_seen = rec_tc_count_distinct_ranges(heap_read(loc + 0),
                                            seen, cap, n_seen, depth + 1);
      n_seen = rec_tc_count_distinct_ranges(heap_read(loc + 1),
                                            seen, cap, n_seen, depth + 1);
      n_seen = rec_tc_count_distinct_ranges(heap_read(loc + 2),
                                            seen, cap, n_seen, depth + 1);
      return n_seen;
    default:
      return n_seen;
  }
}

// Walk `t` looking for UOP_IDIV / UOP_IMOD nodes.  These are the
// structural marker that distinguishes the kernel_lift conv2d_flat
// shape from a clean matmul: conv compresses (co, bi, oh, ow) into
// a single r_out via IDIV/IMOD decomposition, so its W and X
// addresses contain those nodes.  A clean matmul `m*K+k` / `k*N+n`
// never produces them.  Used by both the matmul classifier (to
// reject conv shapes) and the conv classifier (in recognise_conv.c)
// to distinguish.  Bounded depth so a misshapen DAG can't run away.
static int rec_tc_addr_has_divmod(Term t, int depth) {
  if (depth > 32) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_IDIV || op == UOP_IMOD) return 1;
  u64 loc = term_val(t);
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR: case UOP_ISHR:
      if (rec_tc_addr_has_divmod(heap_read(loc + 0), depth + 1)) return 1;
      return rec_tc_addr_has_divmod(heap_read(loc + 1), depth + 1);
    case UOP_IWHERE:
      if (rec_tc_addr_has_divmod(heap_read(loc + 0), depth + 1)) return 1;
      if (rec_tc_addr_has_divmod(heap_read(loc + 1), depth + 1)) return 1;
      return rec_tc_addr_has_divmod(heap_read(loc + 2), depth + 1);
    default:
      return 0;
  }
}

// Walk `t` looking for a UOP_RANGE leaf whose axis_id matches the
// requested value; returns its extent if found, 0 otherwise. Local
// duplicate of render_uop's rmu_collect_ranges -- rmu_collect_ranges
// is static-in-render_uop.c so we can't call it from here in the
// unity build (recognise_tc.c is compiled before the codegen layer).
static u32 rec_tc_find_range_extent(Term t, u32 want_axis_id, int depth) {
  if (depth > 32) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 axis_id = (u32)term_val(heap_read(loc + 0));
    if (axis_id != want_axis_id) return 0;
    return (u32)term_val(heap_read(loc + 2));
  }
  // Recurse through known-arity-Term opcodes used in INDEX expressions.
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR: {
      u32 e = rec_tc_find_range_extent(heap_read(loc + 0), want_axis_id, depth + 1);
      if (e != 0) return e;
      return rec_tc_find_range_extent(heap_read(loc + 1), want_axis_id, depth + 1);
    }
    case UOP_IWHERE: {
      u32 e = rec_tc_find_range_extent(heap_read(loc + 0), want_axis_id, depth + 1);
      if (e != 0) return e;
      e = rec_tc_find_range_extent(heap_read(loc + 1), want_axis_id, depth + 1);
      if (e != 0) return e;
      return rec_tc_find_range_extent(heap_read(loc + 2), want_axis_id, depth + 1);
    }
    default:
      return 0;
  }
}

// Detection helper: returns 1 if `root` is a matmul-shape STORE
// (REDUCE-of-MUL on two distinct INDEX_E buffers, SUM kind). When 1,
// `*out_k_extent` carries the reduce-axis extent if statically known
// (looked up from the UOP_RANGE leaf with axis_id == REDUCE.axis);
// 0 means the reduce axis didn't resolve to a clean RANGE leaf in
// the address expression.  `*out_unit_axis` reports a collapsed
// leading-/trailing-unit GEMM axis: 0 = both operands carry 2 ranges
// (dense M,N>1 matmul), 1 = A's M-axis collapsed (M==1), 2 = B's
// N-axis collapsed (N==1).  Pass NULL when the caller doesn't care.
// Caller decides what to do with it.
fn int uop_classify_matmul(Term root, u32 *out_k_extent, u32 *out_unit_axis) {
  if (out_k_extent != NULL) *out_k_extent = 0;
  if (out_unit_axis != NULL) *out_unit_axis = 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  // Structural walk (mirrors the batched recogniser): STORE -> REDUCE ->
  // MUL -> [INDEX_E | CAST(INDEX_E)] x2.  An operand may carry a CAST over
  // its INDEX_E -- a bf16->f32 widening into the f32 accumulator, OR the
  // q8 weight-only-quantized path's int8->bf16 dequant cast
  // (TMatMul[x, Transpose[Cast[w_int8, bf16]]]).  rec_tc_peel_cast strips
  // that wrapper so the 2-range / divmod structural classification reads
  // the underlying INDEX_E addresses; rmu_emit_matmul_tc peels the same
  // cast at render time (and reads char -> bfloat for int8 B).  A bare
  // INDEX_E (the f32 / bf16 matmul) peels to itself -> byte-identical.
  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  // Stay idempotent: an already OPT(_, TC, _)-wrapped value is NOT a bare
  // matmul shape (it was already recognised), so do not peel/re-wrap it --
  // callers that re-classify a wrapped root (the dispatch-shape derivation)
  // peel the OPT themselves (udg_peel_tc_opt) before calling in.
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  u32 kind = uop_reduce_kind(reduce);
  if (kind != REDUCE_SUM) return 0;
  // TC matmul classifier expects a single reduce axis (K) -- multi-axis
  // REDUCE bails (caller falls back to non-TC path).
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis = uop_reduce_axis(reduce, 0);

  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term idx_a = rec_tc_peel_cast(heap_read(term_val(mul) + 0));
  Term idx_b = rec_tc_peel_cast(heap_read(term_val(mul) + 1));
  // THVM_FUSE_MATMUL_INPUT: A may be a fused elementwise producer (not a bare
  // INDEX_E).  Recover a representative INDEX_E leaf for the A-side layout
  // classification; B must still be a clean buffer read (FLUX's weight).
  int a_fused = 0;
  if (!(term_tag(idx_a) == TAG_UOP && term_ext(idx_a) == UOP_INDEX_E)
      && ru_fuse_matmul_input_on()) {
    Term rep = rec_tc_fused_first_index(idx_a, 0);
    if (rep != 0) { idx_a = rep; a_fused = 1; }
  }
  if (term_tag(idx_a) != TAG_UOP || term_ext(idx_a) != UOP_INDEX_E) return 0;
  if (term_tag(idx_b) != TAG_UOP || term_ext(idx_b) != UOP_INDEX_E) return 0;
  Term buf_a = heap_read(term_val(idx_a) + 0);
  Term buf_b = heap_read(term_val(idx_b) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) return 0;
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) return 0;
  // Fused A is structurally distinct (an ALU tree, not B's buffer), so the
  // square-of-self guard only applies to the bare-INDEX_E A case.
  if (!a_fused && term_val(buf_a) == term_val(buf_b)) return 0;

  // Walk the MUL's INDEX_E address expressions for a UOP_RANGE leaf
  // whose axis_id matches red_axis; its extent is K.  Mirrors
  // rmu_emit_matmul_tc's K-extent lookup so callers see the same
  // value the renderer will see.
  Term addr_a = heap_read(term_val(idx_a) + 1);
  Term addr_b = heap_read(term_val(idx_b) + 1);

  // Conv2d single-input shape also matches REDUCE(MUL(INDEX_E(W),
  // INDEX_E(X))) but its X address references ranges {bi, ci, oh+kh,
  // ow+kw} -- four or more distinct UOP_RANGE leaves -- whereas
  // matmul addresses reference exactly two each (m*K+k for A,
  // k*N+n for B).  The simdgroup_matrix template assumes a clean
  // 2-range linear layout per operand; matching against conv would
  // produce wrong simdgroup_load reads.  Reject if either address
  // touches more than 2 distinct range axis_ids.
  //
  // The collapsed-unit-axis matmul (M==1: A={1,K}@B={K,N}={1,N}, the
  // GPT-2 decode LM-head) is the exception: ru_new_range collapses the
  // size-1 leading M-axis to CONST(0) (rangeify_unified.c:54, mirroring
  // tinygrad/schedule/indexing.py:53), so A's address loses its m-arm
  // and references only the reduce axis K (n_a == 1).  This is still a
  // genuine GEMM with M==1 -- accept (1,2) and (2,1) provided the
  // 1-range operand references PRECISELY the reduce axis (its other
  // axis is the collapsed unit dim).  *out_unit_axis reports the side
  // (1 = A's M collapsed, 2 = B's N collapsed, 0 = full 2x2) so the TC
  // wrapper can stay strict (the simdgroup template needs both 2-range
  // operands).
  u32 seen_a[8] = {0};
  u32 seen_b[8] = {0};
  u32 n_a = rec_tc_count_distinct_ranges(addr_a, seen_a, 8, 0, 0);
  u32 n_b = rec_tc_count_distinct_ranges(addr_b, seen_b, 8, 0, 0);
  u32 unit_axis = 0;
  if (n_a == 2 && n_b == 2) {
    unit_axis = 0;
  } else if (n_a == 1 && n_b == 2) {
    if (seen_a[0] != red_axis) return 0;     // A's lone range must be K
    unit_axis = 1;                           // A's M-axis collapsed (M==1)
  } else if (n_a == 2 && n_b == 1) {
    if (seen_b[0] != red_axis) return 0;     // B's lone range must be K
    unit_axis = 2;                           // B's N-axis collapsed (N==1)
  } else {
    return 0;
  }
  if (out_unit_axis != NULL) *out_unit_axis = unit_axis;

  // F4: the kernel_lift conv2d_flat shape compresses (co, bi, oh, ow)
  // into a single r_out via IDIV/IMOD, so its W and X addresses also
  // reference exactly 2 distinct ranges (r_out, r_q) and would slip
  // past the n_a==2/n_b==2 gate above.  Distinguish by checking for
  // IDIV/IMOD presence in either address tree -- a clean matmul
  // `m*K+k` / `k*N+n` never produces those.  uop_recognise_conv
  // installs the CONV opt wrapper for this shape; here we just
  // reject so the TC wrapper doesn't also fire.
  if (rec_tc_addr_has_divmod(addr_a, 0)) return 0;
  if (rec_tc_addr_has_divmod(addr_b, 0)) return 0;

  u32 ka = rec_tc_find_range_extent(addr_a, red_axis, 0);
  if (ka == 0) ka = rec_tc_find_range_extent(addr_b, red_axis, 0);
  if (out_k_extent != NULL) *out_k_extent = ka;
  return 1;
}

// THVM_FUSE_MATMUL_EPILOGUE: the scheduler (rangeify_unified.c remove_bufferize)
// un-realizes a single-consumer matmul reduce so its REDUCE inlines into the
// consuming elementwise chain -- the store value is then ELEMENTWISE(...REDUCE
// ...), not a bare REDUCE.  uop_recognise_tc / uop_recognise_tc_parallel only
// classify a bare-REDUCE store value, so without help they would never install
// the OPT_TC marker the codegen epilogue (render_uop.c rmu_detect_matmul_tc)
// walks for.  These helpers find that nested REDUCE and wrap it in OPT_TC in
// place, leaving the surrounding elementwise chain intact.

// Walk a pure elementwise/cast store value for EXACTLY ONE UOP_REDUCE leaf (the
// inlined matmul).  Mirrors render_uop.c rmu_epilogue_walk: any movement /
// second reduce / unknown op makes it non-epilogue, so we reject and the bare
// path leaves the root unchanged.  Returns the single REDUCE term, or 0.
static Term rec_tc_epilogue_walk(Term t, Term *out_red, int *n_red, int depth) {
  if (depth > 48) return (Term)1;            // sentinel: still OK, keep walking
  if (term_tag(t) != TAG_UOP) return (Term)1;
  u32 op = term_ext(t);
  if (op == UOP_REDUCE) { *out_red = t; (*n_red)++; return (Term)1; }
  if (op == UOP_CONST || op == UOP_INDEX_E || op == UOP_BUFFER
      || op == UOP_RANGE) return (Term)1;
  if (op == UOP_OPT) return 0;               // already wrapped: not a bare chain
  if (op == UOP_CAST || op == UOP_BITCAST
      || uop_is_unary_elementwise((u8)op)
      || uop_is_binary_elementwise((u8)op)
      || uop_is_ternary_elementwise((u8)op)) {
    u8 ar = uop_arity((u8)op);
    for (u8 i = 0; i < ar; i++) {
      if (rec_tc_epilogue_walk(heap_read(term_val(t) + i), out_red, n_red,
                               depth + 1) == 0)
        return 0;
    }
    return (Term)1;
  }
  return 0;                                  // movement/unknown: not epilogue-safe
}
static Term rec_tc_find_epilogue_reduce(Term value) {
  Term red = 0; int n = 0;
  if (rec_tc_epilogue_walk(value, &red, &n, 0) == 0) return 0;
  if (n != 1 || red == 0) return 0;
  return red;
}

// Single-pass memoized substitution: replace the target term with `repl`
// throughout the value tree.  Each pre-existing term is rebuilt at most once;
// `repl` is returned as-is (never recursed into) so the wrapped OPT(REDUCE,...)
// can't re-trigger the walk.  Mirror of apply_opt_dag_substitute, scoped to one
// target term.  Bounded depth so a misshapen DAG can't run away.
#define REC_TC_SUB_MEMO_CAP 1024
typedef struct {
  Term target, repl;
  Term key[REC_TC_SUB_MEMO_CAP], val[REC_TC_SUB_MEMO_CAP];
  u32 n;
} RecTcSubState;
static Term rec_tc_sub(RecTcSubState *st, Term t, int depth) {
  if (depth > 64) return t;
  Term r = term_resolve(t);
  if (r == st->target) return st->repl;
  if (term_tag(r) != TAG_UOP) return r;
  for (u32 i = 0; i < st->n; i++) if (st->key[i] == r) return st->val[i];
  u8 op = term_ext(r);
  if (op == UOP_KERNEL || op == UOP_BUFFER) return r;
  u8 ar = uop_arity(op);
  Term srcs[MAX_UOP_SRC] = {0};
  int changed = 0;
  u64 loc = term_val(r);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_c = heap_read(loc + i);
    Term new_c = rec_tc_sub(st, old_c, depth + 1);
    srcs[i] = new_c;
    if (new_c != old_c) changed = 1;
  }
  Term out = changed ? uop_graph_rebuild_with_srcs(r, srcs) : r;
  if (st->n < REC_TC_SUB_MEMO_CAP) { st->key[st->n] = r; st->val[st->n] = out; st->n++; }
  return out;
}

// Build a synthetic STORE(buf_out, addr_out, reduce) so the bare-REDUCE matmul
// classifiers (uop_classify_matmul / uop_matmul_mn_axes) -- which expect the
// REDUCE at the store's slot+2 -- can run against an epilogue-fused store whose
// real slot+2 is the elementwise chain.  The reduce inherits the consumer's
// output (M, N) ranges, so the synthetic store carries the genuine output
// address.  Returns 0 if `value` is not an epilogue chain over one REDUCE.
static Term rec_tc_synth_matmul_store(Term root, Term *out_reduce) {
  if (out_reduce != NULL) *out_reduce = 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  u64 sloc = term_val(root);
  Term value = heap_read(sloc + 2);
  Term red = rec_tc_find_epilogue_reduce(value);
  if (red == 0) return 0;
  if (out_reduce != NULL) *out_reduce = red;
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  return uop_store(buf_out, addr_out, red);
}

// Rebuild `root`'s store with the nested `reduce` replaced by OPT(reduce,TC,0),
// keeping the surrounding elementwise epilogue chain intact.
static Term rec_tc_wrap_epilogue_reduce(Term root, Term reduce) {
  u64 sloc = term_val(root);
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  Term value    = heap_read(sloc + 2);
  Term tc       = uop_opt(reduce, UOP_OPT_TC, 0);
  RecTcSubState st = { reduce, tc, {0}, {0}, 0 };
  Term new_value = rec_tc_sub(&st, value, 0);
  if (new_value == value) return root;
  return uop_store(buf_out, addr_out, new_value);
}

// Wrap the STORE root's value with UOP_OPT(_, TC, 0) when the matmul
// shape matches. Returns the input root unchanged on any non-match.
fn Term uop_recognise_tc(Term root) {
  // THVM_FUSE_MATMUL_EPILOGUE: the store value may be an elementwise chain over
  // a single matmul REDUCE (epilogue fusion).  Classify the synthetic
  // bare-REDUCE store; on a match, wrap the nested REDUCE in OPT_TC in place.
  if (ru_fuse_epilogue_on()) {
    Term reduce = 0;
    Term synth = rec_tc_synth_matmul_store(root, &reduce);
    if (synth != 0) {
      u32 unit_axis = 0;
      if (uop_classify_matmul(synth, NULL, &unit_axis) && unit_axis == 0)
        return rec_tc_wrap_epilogue_reduce(root, reduce);
      return root;       // epilogue chain but not a clean 2x2 matmul: leave bare
    }
  }
  // The simdgroup_matrix template needs BOTH operands to carry a clean
  // 2-range {m,k}/{k,n} layout; a collapsed-unit GEMM axis (M==1 or
  // N==1, unit_axis != 0) loses one operand's outer arm, so skip the TC
  // wrapper there and let the generic accumulator / BLAS GEMV-shaped
  // dispatch handle it.
  u32 unit_axis = 0;
  if (!uop_classify_matmul(root, NULL, &unit_axis) || unit_axis != 0) return root;

  // Rebuild: STORE(buf_out, addr_out, OPT(reduce, TC, 0)).
  u64 sloc = term_val(root);
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  Term reduce   = heap_read(sloc + 2);
  Term tc       = uop_opt(reduce, UOP_OPT_TC, 0);
  return uop_store(buf_out, addr_out, tc);
}

// Extract the M-axis (the non-reduce range in operand A's address) and
// the N-axis (the non-reduce range in operand B's address) of a
// matmul-shaped STORE, along with their extents.  Mirrors the axis
// discovery rmu_emit_matmul_tc does at render time so the producer side
// (uop_recognise_tc_parallel) can re-stamp those exact two RANGE leaves
// KAX_GLOBAL.  Returns 1 and fills *out_m_axis / *out_m_extent /
// *out_n_axis / *out_n_extent on a matmul match; 0 otherwise.
//
// "A's address" is MUL.src[0].addr (slot 1 of the INDEX_E), "B's" is
// MUL.src[1].addr.  The reduce axis (K) is the one both addresses share;
// each address also touches exactly one other range -- M for A, N for B.
fn int uop_matmul_mn_axes(Term root, u32 *out_m_axis, u32 *out_m_extent,
                          u32 *out_n_axis, u32 *out_n_extent) {
  if (out_m_axis   != NULL) *out_m_axis   = 0xFFFFFFFFu;
  if (out_n_axis   != NULL) *out_n_axis   = 0xFFFFFFFFu;
  if (out_m_extent != NULL) *out_m_extent = 0;
  if (out_n_extent != NULL) *out_n_extent = 0;
  if (!uop_classify_matmul(root, NULL, NULL)) return 0;

  u64 sloc = term_val(root);
  Term reduce  = heap_read(sloc + 2);
  u32 red_axis = uop_reduce_axis(reduce, 0);
  Term mul     = uop_reduce_src(reduce);
  // Peel a CAST(INDEX_E) operand (bf16->f32 widen / int8->bf16 dequant)
  // to read its underlying address -- mirrors uop_classify_matmul.
  Term idx_a   = rec_tc_peel_cast(heap_read(term_val(mul) + 0));
  Term idx_b   = rec_tc_peel_cast(heap_read(term_val(mul) + 1));
  // THVM_FUSE_MATMUL_INPUT: recover a representative INDEX_E from a fused A
  // producer (same {m,k} layout as a plain A operand) so the M-axis discovery
  // reads a real address.
  if (!(term_tag(idx_a) == TAG_UOP && term_ext(idx_a) == UOP_INDEX_E)
      && ru_fuse_matmul_input_on()) {
    Term rep = rec_tc_fused_first_index(idx_a, 0);
    if (rep != 0) idx_a = rep;
  }
  Term addr_a  = heap_read(term_val(idx_a) + 1);
  Term addr_b  = heap_read(term_val(idx_b) + 1);

  // A's two ranges are {M, K}; B's are {K, N}.  Pull the non-reduce one
  // from each.  uop_classify_matmul already guaranteed each address
  // touches exactly 2 distinct ranges.
  u32 seen_a[8] = {0};
  u32 seen_b[8] = {0};
  u32 n_a = rec_tc_count_distinct_ranges(addr_a, seen_a, 8, 0, 0);
  u32 n_b = rec_tc_count_distinct_ranges(addr_b, seen_b, 8, 0, 0);
  if (n_a != 2 || n_b != 2) return 0;

  u32 m_axis = (seen_a[0] == red_axis) ? seen_a[1] : seen_a[0];
  u32 n_axis = (seen_b[0] == red_axis) ? seen_b[1] : seen_b[0];
  if (m_axis == red_axis || n_axis == red_axis) return 0;
  if (m_axis == n_axis) return 0;

  u32 m_ext = rec_tc_find_range_extent(addr_a, m_axis, 0);
  u32 n_ext = rec_tc_find_range_extent(addr_b, n_axis, 0);
  if (m_ext == 0 || n_ext == 0) return 0;

  if (out_m_axis   != NULL) *out_m_axis   = m_axis;
  if (out_n_axis   != NULL) *out_n_axis   = n_axis;
  if (out_m_extent != NULL) *out_m_extent = m_ext;
  if (out_n_extent != NULL) *out_n_extent = n_ext;
  return 1;
}

// Producer-side parallel-TC wrap.  When the matmul shape qualifies for
// the simdgroup_matrix template (K, M, N all multiples of 8), wrap the
// REDUCE with OPT(_, TC, 0) AND re-stamp the M and N output RANGE leaves
// KAX_GLOBAL.  This makes render_uop's rmu_emit_matmul_tc take the
// `parallel_tc` branch -- one simdgroup per 8x8 output tile, no
// `if(sgi==0u && tg==0u)` serial guard -- and makes
// cg_tile_metal_dispatch_shape launch product(extent/8) threadgroups.
//
// Without the GLOBAL stamp the renderer falls back to the guarded body
// that runs the WHOLE matmul on a single 32-thread simdgroup (~14x off
// peak on M3).  Each threadgroup owns a UNIQUE 8x8 output tile so there
// is no multi-simdgroup write race -- the guard the legacy path adds is
// pure waste here, not a correctness requirement.
//
// === split-M/N TC collapse (gated on THVM_FUSE_MATMUL_EPILOGUE) ===========
// A FUSED matmul (epilogue / PCONTIG) whose output rides an elementwise chain
// with a broadcast (FLUX's gated residual `x + gate*matmul`, gate {dim} over
// {S,dim}) can have its M (or N) output axis SPLIT across several contiguous
// LOOP range axes by the consumer's reshape -- so the operand address touches
// >2 distinct ranges and uop_classify_matmul declines (the matmul falls to the
// generic accumulator instead of the simdgroup TILED path).  When the split
// axes are contiguous (stride(outer) == extent(inner)*stride(inner)) and all
// KAX_LOOP, collapse them into ONE flat range: the outer split axes become
// CONST(0) and the innermost a fresh RANGE of the product extent.  Since
// flat*stride_inner == sum_i axis_i*stride_i exactly (contiguity), every
// address is byte-preserved and the matmul recognises as a clean 2-range GEMM
// on the TILED tensor-core path.  Declines (returns the store unchanged) on any
// non-contiguous / non-LOOP split, so a mis-shaped fusion falls back to the
// (correct) generic accumulator rather than miscompiling.
static int rec_tc_const_i64(Term t, i64 *out) {
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term num = heap_read(term_val(t));
  if (term_tag(num) != TAG_NUM) return 0;
  *out = (i64)(i32)(u32)term_val(num);
  return 1;
}
// Coefficient (stride) of RANGE(axis_id) in a linear index tree.  *ok=0 when
// the axis appears under a non-linear op (IDIV/IMOD/...) -> caller declines.
static i64 rec_tc_addr_coeff(Term t, u32 axis_id, int *ok, int depth) {
  if (depth > 40) { *ok = 0; return 0; }
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE)
    return ((u32)term_val(heap_read(loc + 0)) == axis_id) ? 1 : 0;
  if (op == UOP_IADD || op == UOP_ISUB) {
    i64 l = rec_tc_addr_coeff(heap_read(loc + 0), axis_id, ok, depth + 1);
    i64 r = rec_tc_addr_coeff(heap_read(loc + 1), axis_id, ok, depth + 1);
    return (op == UOP_ISUB) ? (l - r) : (l + r);
  }
  if (op == UOP_IMUL) {
    Term a = heap_read(loc + 0), b = heap_read(loc + 1);
    int oka = 1, okb = 1;
    i64 ca = rec_tc_addr_coeff(a, axis_id, &oka, depth + 1);
    i64 cb = rec_tc_addr_coeff(b, axis_id, &okb, depth + 1);
    if (!oka || !okb) { *ok = 0; return 0; }
    i64 k;
    if (ca != 0 && cb == 0 && rec_tc_const_i64(b, &k)) return ca * k;
    if (cb != 0 && ca == 0 && rec_tc_const_i64(a, &k)) return cb * k;
    if (ca == 0 && cb == 0) return 0;       // axis absent from this product
    *ok = 0; return 0;                       // axis * non-const -> non-linear
  }
  // Unsupported op: bail iff our axis appears beneath it.
  if (rec_tc_find_range_extent(t, axis_id, 0) != 0) *ok = 0;
  return 0;
}
static Term rec_tc_find_range_node(Term t, u32 axis_id, int depth) {
  if (depth > 40) return 0;
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE)
    return ((u32)term_val(heap_read(loc + 0)) == axis_id) ? t : 0;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term r = rec_tc_find_range_node(heap_read(loc + i), axis_id, depth + 1);
    if (r != 0) return r;
  }
  return 0;
}
// Verify the split axes `ax[0..n)` are a contiguous nest in `addr` (each
// present axis's stride == product of inner extents * inner stride), reusing
// the order/extents already computed for the operand.  Absent axes (coeff 0)
// are fine (the output may not index every operand axis).  Returns 0 to decline.
static int rec_tc_split_contiguous(Term addr, u32 *ax_ord, u32 *ex_ord, u32 n) {
  i64 prev = 0; int have_prev = 0;
  for (u32 i = 0; i < n; i++) {           // ax_ord/ex_ord sorted outer->inner
    int ok = 1;
    i64 c = rec_tc_addr_coeff(addr, ax_ord[i], &ok, 0);
    if (!ok) return 0;
    if (c == 0) { have_prev = 0; continue; }   // axis absent here
    // contiguous nest: stride(outer) == stride(inner) * extent(inner)
    if (have_prev && prev != c * (i64)ex_ord[i]) return 0;
    prev = c; have_prev = 1;
  }
  return 1;
}
static Term rec_tc_collapse_split(Term root) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return root;
  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  if (!(term_tag(reduce) == TAG_UOP && term_ext(reduce) == UOP_REDUCE))
    reduce = rec_tc_find_epilogue_reduce(heap_read(sloc + 2));
  if (reduce == 0 || term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE)
    return root;
  if (uop_reduce_kind(reduce) != REDUCE_SUM) return root;
  if (uop_reduce_n_axes(reduce) != 1) return root;
  u32 red_axis = uop_reduce_axis(reduce, 0);
  Term mul = uop_reduce_src(reduce);
  if (!(term_tag(mul) == TAG_UOP && term_ext(mul) == UOP_MUL)) return root;
  Term idx_a = rec_tc_peel_cast(heap_read(term_val(mul) + 0));
  Term idx_b = rec_tc_peel_cast(heap_read(term_val(mul) + 1));
  if (!(term_tag(idx_a) == TAG_UOP && term_ext(idx_a) == UOP_INDEX_E)) return root;
  if (!(term_tag(idx_b) == TAG_UOP && term_ext(idx_b) == UOP_INDEX_E)) return root;
  Term addr_a = heap_read(term_val(idx_a) + 1);
  Term addr_b = heap_read(term_val(idx_b) + 1);
  Term addr_out = heap_read(sloc + 1);
  // Precompute each operand's range set: a non-K axis shared by BOTH operands
  // is a BATCH axis (it indexes A, B and the output) -- excluded, because it is
  // contiguous-with-M by construction and collapsing it would merge the batch
  // into M/N and miscompile a batched matmul.  Only an axis UNIQUE to one
  // operand (M for A, N for B) is a genuine split.
  u32 sa[8], sb[8];
  u32 na = rec_tc_count_distinct_ranges(addr_a, sa, 8, 0, 0);
  u32 nb = rec_tc_count_distinct_ranges(addr_b, sb, 8, 0, 0);
  Term result = root;
  Term opers[2]; opers[0] = addr_a; opers[1] = addr_b;
  u32 *oth[2];   oth[0]  = sb; oth[1]  = sa;
  u32  oth_n[2]; oth_n[0] = nb; oth_n[1] = na;
  for (int side = 0; side < 2; side++) {
    Term addr = opers[side];
    u32 seen[8];
    u32 n = rec_tc_count_distinct_ranges(addr, seen, 8, 0, 0);
    u32 nk[8], n_nk = 0;
    for (u32 i = 0; i < n; i++) {
      if (seen[i] == red_axis) continue;
      int shared = 0;
      for (u32 j = 0; j < oth_n[side]; j++) if (oth[side][j] == seen[i]) { shared = 1; break; }
      if (shared) continue;                  // batch axis: never collapse
      if (n_nk < 8) nk[n_nk++] = seen[i];
    }
    if (n_nk <= 1) continue;                 // already a clean (or M==1) axis
    i64 cf[8]; u32 ex[8]; int decline = 0;
    for (u32 i = 0; i < n_nk; i++) {
      int ok = 1;
      cf[i] = rec_tc_addr_coeff(addr, nk[i], &ok, 0);
      if (!ok || cf[i] <= 0) return root;    // non-linear: decline outright
      ex[i] = rec_tc_find_range_extent(addr, nk[i], 0);
      Term rn = rec_tc_find_range_node(addr, nk[i], 0);
      if (ex[i] == 0 || rn == 0) return root;
      u32 kax = (u32)term_val(heap_read(term_val(rn) + 1));
      if (kax != KAX_LOOP) { decline = 1; break; }  // already-tiled: leave alone
    }
    if (decline) continue;
    u32 ord[8]; for (u32 i = 0; i < n_nk; i++) ord[i] = i;
    for (u32 i = 0; i < n_nk; i++)
      for (u32 j = i + 1; j < n_nk; j++)
        if (cf[ord[j]] > cf[ord[i]]) { u32 t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    u32 ax_ord[8], ex_ord[8];
    for (u32 i = 0; i < n_nk; i++) { ax_ord[i] = nk[ord[i]]; ex_ord[i] = ex[ord[i]]; }
    if (!rec_tc_split_contiguous(addr, ax_ord, ex_ord, n_nk)) return root;
    if (!rec_tc_split_contiguous(addr_out, ax_ord, ex_ord, n_nk)) return root;
    u32 flat = 1; for (u32 i = 0; i < n_nk; i++) flat *= ex_ord[i];
    u32 inner = ax_ord[n_nk - 1];            // smallest stride survives
    for (u32 i = 0; i < n_nk; i++) {
      Term rn = rec_tc_find_range_node(result, ax_ord[i], 0);
      if (rn == 0) continue;
      Term repl = (ax_ord[i] == inner) ? uop_range(inner, KAX_LOOP, flat)
                                       : uop_const(DT_INT32, 0);
      RecTcSubState st = { rn, repl, {0}, {0}, 0 };
      result = rec_tc_sub(&st, result, 0);
    }
  }
  return result;
}

// Falls back to the plain (guarded) uop_recognise_tc wrap when M or N is
// not a multiple of 8 (the simdgroup template can't tile a ragged
// fragment, so it bails to the scalar accumulator -- which needs the
// output_numel grid, not the tile grid).  Returns the input root
// unchanged on any non-matmul.
fn Term uop_recognise_tc_parallel(Term root) {
  // THVM_FUSE_MATMUL_EPILOGUE: an epilogue/PCONTIG-fused matmul can have its
  // M/N output axis split by the consumer's broadcast reshape; collapse the
  // contiguous split so the clean 2-range classifier keeps it on the TILED
  // tensor-core path instead of the generic accumulator.  No-op (returns root)
  // without a split, so the default path stays byte-identical.
  if (ru_fuse_epilogue_on()) root = rec_tc_collapse_split(root);
  // THVM_FUSE_MATMUL_EPILOGUE: an epilogue-fused store has the matmul REDUCE
  // nested inside an elementwise chain, so uop_matmul_mn_axes (which reads the
  // bare-REDUCE store slot+2) won't classify it.  Classify the synthetic
  // bare-REDUCE store for the M/N/K axes, wrap the nested reduce via
  // uop_recognise_tc, then stamp M/N GLOBAL on the (chain-preserving) result.
  if (ru_fuse_epilogue_on()) {
    Term reduce = 0;
    Term synth = rec_tc_synth_matmul_store(root, &reduce);
    if (synth != 0) {
      u32 em_axis, em_ext, en_axis, en_ext, ek_ext = 0;
      if (!uop_matmul_mn_axes(synth, &em_axis, &em_ext, &en_axis, &en_ext))
        return uop_recognise_tc(root);       // not a clean 2x2 matmul: leave bare
      uop_classify_matmul(synth, &ek_ext, NULL);
      if ((em_ext % 8u) != 0 || (en_ext % 8u) != 0
          || ek_ext == 0 || (ek_ext % 8u) != 0)
        return uop_recognise_tc(root);        // ragged: scalar epilogue path
      Term tc = uop_recognise_tc(root);
      if (tc == root) return root;            // shape didn't actually wrap
      tc = uop_dag_apply_global(tc, em_axis);
      tc = uop_dag_apply_global(tc, en_axis);
      return tc;
    }
  }
  u32 m_axis, m_extent, n_axis, n_extent;
  if (!uop_matmul_mn_axes(root, &m_axis, &m_extent, &n_axis, &n_extent)) {
    // Not a matmul, or M/N axes didn't resolve -- still install the
    // plain TC marker (no GLOBAL) so non-conforming matmuls render
    // through the guarded simdgroup/scalar path.
    return uop_recognise_tc(root);
  }
  if ((m_extent % 8u) != 0 || (n_extent % 8u) != 0) {
    // Ragged M/N: the simdgroup template bails to the scalar
    // accumulator, whose grid is output_numel.  Keep axes LOOP.
    return uop_recognise_tc(root);
  }

  // K is also gated %8 inside the renderer; check it here so we only
  // stamp GLOBAL when the parallel tile path will actually fire.
  u32 k_extent = 0;
  uop_classify_matmul(root, &k_extent, NULL);
  if (k_extent == 0 || (k_extent % 8u) != 0) {
    return uop_recognise_tc(root);
  }

  Term tc = uop_recognise_tc(root);
  if (tc == root) return root;  // shape didn't actually wrap
  tc = uop_dag_apply_global(tc, m_axis);
  tc = uop_dag_apply_global(tc, n_axis);
  return tc;
}

// === TRUE batched-matmul recogniser (batch axis in A, B AND output) ======
//
// The 2-D matmul recogniser above rejects the batched gemm: each operand
// touches THREE distinct ranges ({batch,M,K} for A, {batch,K,N} for B), so
// the n_a==2/n_b==2 gate fails.  This classifier accepts that shape.  The
// canonical instance is THeadAttention's mhaBmm -- a single SUM reduce over
// K, with the batch (head) axis shared by both operands and the output.
// Operands may be PERMUTED views (the attention heads are {2,1,3}/{1,3,2}
// transposes), so M/N/K/batch are identified purely by which addresses each
// range appears in -- never by a canonical contiguous-layout assumption.
//
// Returns 1 + fills the four axis ids/extents on a match; 0 otherwise.  K is
// the (single) reduce axis; batch is the non-reduce range shared by A and B;
// M is the non-reduce range unique to A; N the one unique to B.  IDIV/IMOD
// in either address (a conv/view-decomposed lift) rejects.
fn int uop_classify_batched_matmul(Term root,
                                   u32 *out_batch_axis, u32 *out_batch_extent,
                                   u32 *out_m_axis, u32 *out_m_extent,
                                   u32 *out_n_axis, u32 *out_n_extent,
                                   u32 *out_k_extent) {
  if (out_batch_axis  != NULL) *out_batch_axis  = 0xFFFFFFFFu;
  if (out_m_axis      != NULL) *out_m_axis      = 0xFFFFFFFFu;
  if (out_n_axis      != NULL) *out_n_axis      = 0xFFFFFFFFu;
  if (out_batch_extent!= NULL) *out_batch_extent= 0;
  if (out_m_extent    != NULL) *out_m_extent    = 0;
  if (out_n_extent    != NULL) *out_n_extent    = 0;
  if (out_k_extent    != NULL) *out_k_extent    = 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  // Structural walk (not upat): the batched-gemm operands may be CAST(INDEX_E)
  // (a bf16->f32 widening into the f32 accumulator), which the bare-INDEX_E
  // UPat would reject.  Read STORE->REDUCE->MUL->[INDEX_E|CAST(INDEX_E)] x2.
  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  // Peel a leading OPT(_, TC, _) wrapper: hand_opts (and the producer-side
  // recogniser) wrap the REDUCE with the TC marker, but the dispatch-shape
  // path re-classifies the already-wrapped root.  Mirror udg_peel_tc_opt.
  while (term_tag(reduce) == TAG_UOP && term_ext(reduce) == UOP_OPT
         && uop_opt_kind(reduce) == UOP_OPT_TC) {
    reduce = uop_opt_target(reduce);
  }
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  if (uop_reduce_kind(reduce) != REDUCE_SUM) return 0;
  if (uop_reduce_n_axes(reduce) != 1) return 0;     // single K reduce
  u32 red_axis = uop_reduce_axis(reduce, 0);

  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term idx_a = rec_tc_peel_cast(heap_read(term_val(mul) + 0));
  Term idx_b = rec_tc_peel_cast(heap_read(term_val(mul) + 1));
  if (term_tag(idx_a) != TAG_UOP || term_ext(idx_a) != UOP_INDEX_E) return 0;
  if (term_tag(idx_b) != TAG_UOP || term_ext(idx_b) != UOP_INDEX_E) return 0;
  Term buf_a   = heap_read(term_val(idx_a) + 0);
  Term buf_b   = heap_read(term_val(idx_b) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) return 0;
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) return 0;
  if (term_val(buf_a) == term_val(buf_b)) return 0;

  Term addr_a  = heap_read(term_val(idx_a) + 1);
  Term addr_b  = heap_read(term_val(idx_b) + 1);
  Term addr_o  = heap_read(sloc + 1);
  if (rec_tc_addr_has_divmod(addr_a, 0)) return 0;
  if (rec_tc_addr_has_divmod(addr_b, 0)) return 0;
  if (rec_tc_addr_has_divmod(addr_o, 0)) return 0;

  // A's ranges are {batch, M, K}; B's are {batch, K, N}; output's
  // {batch, M, N}.  Each operand touches exactly 3 distinct ranges.
  u32 seen_a[8] = {0}, seen_b[8] = {0}, seen_o[8] = {0};
  u32 n_a = rec_tc_count_distinct_ranges(addr_a, seen_a, 8, 0, 0);
  u32 n_b = rec_tc_count_distinct_ranges(addr_b, seen_b, 8, 0, 0);
  u32 n_o = rec_tc_count_distinct_ranges(addr_o, seen_o, 8, 0, 0);
  if (n_a != 3 || n_b != 3 || n_o != 3) return 0;

  // Classify each output range by membership: in A only -> M, in B only
  // -> N, in BOTH -> batch.  K must be absent from the output.
  u32 m_axis = 0xFFFFFFFFu, n_axis = 0xFFFFFFFFu, batch_axis = 0xFFFFFFFFu;
  for (u32 i = 0; i < n_o; i++) {
    u32 aid = seen_o[i];
    if (aid == red_axis) return 0;        // K never appears in the output
    int in_a = 0, in_b = 0;
    for (u32 j = 0; j < n_a; j++) if (seen_a[j] == aid) in_a = 1;
    for (u32 j = 0; j < n_b; j++) if (seen_b[j] == aid) in_b = 1;
    if      (in_a && !in_b) { if (m_axis != 0xFFFFFFFFu) return 0; m_axis = aid; }
    else if (in_b && !in_a) { if (n_axis != 0xFFFFFFFFu) return 0; n_axis = aid; }
    else if (in_a &&  in_b) { if (batch_axis != 0xFFFFFFFFu) return 0; batch_axis = aid; }
    else return 0;
  }
  if (m_axis == 0xFFFFFFFFu || n_axis == 0xFFFFFFFFu
      || batch_axis == 0xFFFFFFFFu) return 0;
  // A's three ranges must be exactly {batch, M, K}; B's {batch, N, K}.
  for (u32 j = 0; j < n_a; j++) {
    u32 aid = seen_a[j];
    if (aid != batch_axis && aid != m_axis && aid != red_axis) return 0;
  }
  for (u32 j = 0; j < n_b; j++) {
    u32 aid = seen_b[j];
    if (aid != batch_axis && aid != n_axis && aid != red_axis) return 0;
  }

  u32 k_ext = rec_tc_find_range_extent(addr_a, red_axis, 0);
  if (k_ext == 0) k_ext = rec_tc_find_range_extent(addr_b, red_axis, 0);
  u32 m_ext = rec_tc_find_range_extent(addr_a, m_axis, 0);
  u32 n_ext = rec_tc_find_range_extent(addr_b, n_axis, 0);
  u32 b_ext = rec_tc_find_range_extent(addr_o, batch_axis, 0);
  if (k_ext == 0 || m_ext == 0 || n_ext == 0 || b_ext < 2) return 0;

  if (out_batch_axis   != NULL) *out_batch_axis   = batch_axis;
  if (out_batch_extent != NULL) *out_batch_extent = b_ext;
  if (out_m_axis       != NULL) *out_m_axis       = m_axis;
  if (out_m_extent     != NULL) *out_m_extent     = m_ext;
  if (out_n_axis       != NULL) *out_n_axis       = n_axis;
  if (out_n_extent     != NULL) *out_n_extent     = n_ext;
  if (out_k_extent     != NULL) *out_k_extent     = k_ext;
  return 1;
}

// Producer-side parallel-TC wrap for the TRUE batched matmul.  When M,N,K
// are all multiples of 8, wraps the REDUCE with OPT(_, TC, 0) and re-stamps
// the batch, M and N output RANGE leaves KAX_GLOBAL.  render_uop's
// rmu_emit_matmul_tc then takes the batched parallel branch (one simdgroup
// per (batch, 8x8-tile)) and cg_tile_metal_dispatch_shape launches
// batch * (M/8) * (N/8) threadgroups.  Returns the input root unchanged on
// any non-batched-matmul / ragged shape (the scalar reduce stays correct).
fn Term uop_recognise_batched_tc_parallel(Term root) {
  u32 batch_axis, batch_extent, m_axis, m_extent, n_axis, n_extent, k_extent;
  if (!uop_classify_batched_matmul(root, &batch_axis, &batch_extent,
                                   &m_axis, &m_extent, &n_axis, &n_extent,
                                   &k_extent)) {
    return root;
  }
  if ((m_extent % 8u) != 0 || (n_extent % 8u) != 0
      || (k_extent % 8u) != 0) {
    return root;   // ragged: the scalar reduce path stays.
  }
  // If hand_opts already wrapped this REDUCE in OPT(_, TC, _) and stamped the
  // batch/M/N axes GLOBAL, do NOT re-wrap (a second OPT layer would break
  // rmu_detect_matmul_tc's OPT->REDUCE check).  This producer is the backup
  // for synthetic KernelEntries that skip hand_opts (e.g. the py bridge).
  u64 sloc = term_val(root);
  Term value = heap_read(sloc + 2);
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
      && uop_opt_kind(value) == UOP_OPT_TC) {
    return root;
  }
  // Wrap the REDUCE with the TC marker, then stamp batch/M/N GLOBAL.
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  Term tc       = uop_opt(value, UOP_OPT_TC, 0);
  Term out      = uop_store(buf_out, addr_out, tc);
  out = uop_dag_apply_global(out, batch_axis);
  out = uop_dag_apply_global(out, m_axis);
  out = uop_dag_apply_global(out, n_axis);
  return out;
}

// Structural classifier for the DOT shape.
// STORE(C, addr_out, REDUCE_SUM(MUL(INDEX_E(A, addr_a), INDEX_E(B, addr_b))))
// where A and B are distinct rank-1 buffers and the addresses reference
// the same single UOP_RANGE leaf (the K reduce axis).  Returns 1 + the
// reduce-axis extent on match; 0 otherwise.
//
// Distinguished from matmul by the range count: matmul addresses each
// touch exactly 2 distinct ranges (m,k for A; k,n for B); DOT addresses
// each touch exactly 1.  Distinguished from GEMV (which has W's address
// touching 2 ranges) by both addresses having only 1.
fn int uop_classify_dot(Term root, u32 *out_k_extent) {
  if (out_k_extent != NULL) *out_k_extent = 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  Term bindings[UPAT_NUM_BINDINGS] = {0};
  if (!upat_match(&rec_tc_store, root, bindings)) return 0;

  Term buf_a = bindings[0];
  Term buf_b = bindings[2];
  if (buf_a == 0 || buf_b == 0 || buf_a == buf_b) return 0;

  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  u32 kind = uop_reduce_kind(reduce);
  if (kind != REDUCE_SUM) return 0;
  // Dot recogniser expects single reduce axis.
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis = uop_reduce_axis(reduce, 0);

  Term mul    = uop_reduce_src(reduce);
  Term addr_a = heap_read(term_val(heap_read(term_val(mul) + 0)) + 1);
  Term addr_b = heap_read(term_val(heap_read(term_val(mul) + 1)) + 1);

  u32 seen_a[8] = {0};
  u32 seen_b[8] = {0};
  u32 n_a = rec_tc_count_distinct_ranges(addr_a, seen_a, 8, 0, 0);
  u32 n_b = rec_tc_count_distinct_ranges(addr_b, seen_b, 8, 0, 0);
  if (n_a != 1 || n_b != 1) return 0;
  // Both must reference the SAME range (the reduce axis).  Cross-check
  // by confirming both seen lists match red_axis.
  if (seen_a[0] != red_axis || seen_b[0] != red_axis) return 0;

  // Reject IDIV/IMOD anywhere -- they signal a non-dot lift shape.
  if (rec_tc_addr_has_divmod(addr_a, 0)) return 0;
  if (rec_tc_addr_has_divmod(addr_b, 0)) return 0;

  u32 ka = rec_tc_find_range_extent(addr_a, red_axis, 0);
  if (ka == 0) ka = rec_tc_find_range_extent(addr_b, red_axis, 0);
  if (out_k_extent != NULL) *out_k_extent = ka;
  return 1;
}

// Structural classifier for the GEMV shape.
// STORE(C, addr_out, REDUCE_SUM(MUL(INDEX_E(W, addr_w), INDEX_E(x, addr_x))))
// where W's address touches exactly 2 distinct ranges (m,k) and x's
// address touches exactly 1 distinct range (k -- the reduce axis,
// because m is broadcast-zero).  Returns 1 + the reduce-axis extent
// on match; *out_w_first set to 1 if the matrix-shaped operand is the
// MUL.src[0] side, 0 if it's the MUL.src[1] side.
fn int uop_classify_gemv(Term root, u32 *out_k_extent, int *out_w_first) {
  if (out_k_extent != NULL) *out_k_extent = 0;
  if (out_w_first  != NULL) *out_w_first  = 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  Term bindings[UPAT_NUM_BINDINGS] = {0};
  if (!upat_match(&rec_tc_store, root, bindings)) return 0;

  Term buf_a = bindings[0];
  Term buf_b = bindings[2];
  if (buf_a == 0 || buf_b == 0 || buf_a == buf_b) return 0;

  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  u32 kind = uop_reduce_kind(reduce);
  if (kind != REDUCE_SUM) return 0;
  // GEMV recogniser expects single reduce axis.
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis = uop_reduce_axis(reduce, 0);

  Term mul    = uop_reduce_src(reduce);
  Term addr_a = heap_read(term_val(heap_read(term_val(mul) + 0)) + 1);
  Term addr_b = heap_read(term_val(heap_read(term_val(mul) + 1)) + 1);

  u32 seen_a[8] = {0};
  u32 seen_b[8] = {0};
  u32 n_a = rec_tc_count_distinct_ranges(addr_a, seen_a, 8, 0, 0);
  u32 n_b = rec_tc_count_distinct_ranges(addr_b, seen_b, 8, 0, 0);
  // GEMV pattern: one address has 2 ranges (the matrix), the other 1
  // (the broadcast vector).
  int w_first;
  if (n_a == 2 && n_b == 1) {
    w_first = 1;
  } else if (n_a == 1 && n_b == 2) {
    w_first = 0;
  } else {
    return 0;
  }

  // Reject IDIV/IMOD: matmul/dot/gemv addresses are clean linear
  // combinations of ranges; conv2d-flat lifts use IDIV/IMOD.
  if (rec_tc_addr_has_divmod(addr_a, 0)) return 0;
  if (rec_tc_addr_has_divmod(addr_b, 0)) return 0;

  // Vector address must reference precisely the reduce axis (no other
  // axis -- confirms it's broadcast-on-M).
  u32 const *vec_seen = w_first ? seen_b : seen_a;
  if (vec_seen[0] != red_axis) return 0;

  // Matrix address must include the reduce axis as one of its 2 ranges.
  u32 const *mat_seen = w_first ? seen_a : seen_b;
  if (mat_seen[0] != red_axis && mat_seen[1] != red_axis) return 0;

  Term addr_mat = w_first ? addr_a : addr_b;
  u32 ka = rec_tc_find_range_extent(addr_mat, red_axis, 0);
  if (out_k_extent != NULL) *out_k_extent = ka;
  if (out_w_first  != NULL) *out_w_first  = w_first;
  return 1;
}
