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
// the address expression. Caller decides what to do with it.
fn int uop_classify_matmul(Term root, u32 *out_k_extent) {
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
  // TC matmul classifier expects a single reduce axis (K) -- multi-axis
  // REDUCE bails (caller falls back to non-TC path).
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis = uop_reduce_axis(reduce, 0);

  // Walk the MUL's INDEX_E address expressions for a UOP_RANGE leaf
  // whose axis_id matches red_axis; its extent is K.  Mirrors
  // rmu_emit_matmul_tc's K-extent lookup so callers see the same
  // value the renderer will see.
  Term mul    = uop_reduce_src(reduce);
  Term addr_a = heap_read(term_val(heap_read(term_val(mul) + 0)) + 1);
  Term addr_b = heap_read(term_val(heap_read(term_val(mul) + 1)) + 1);

  // Conv2d single-input shape also matches REDUCE(MUL(INDEX_E(W),
  // INDEX_E(X))) but its X address references ranges {bi, ci, oh+kh,
  // ow+kw} -- four or more distinct UOP_RANGE leaves -- whereas
  // matmul addresses reference exactly two each (m*K+k for A,
  // k*N+n for B).  The simdgroup_matrix template assumes a clean
  // 2-range linear layout per operand; matching against conv would
  // produce wrong simdgroup_load reads.  Reject if either address
  // touches more than 2 distinct range axis_ids.
  u32 seen_a[8] = {0};
  u32 seen_b[8] = {0};
  u32 n_a = rec_tc_count_distinct_ranges(addr_a, seen_a, 8, 0, 0);
  u32 n_b = rec_tc_count_distinct_ranges(addr_b, seen_b, 8, 0, 0);
  if (n_a != 2 || n_b != 2) return 0;

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

// Wrap the STORE root's value with UOP_OPT(_, TC, 0) when the matmul
// shape matches. Returns the input root unchanged on any non-match.
fn Term uop_recognise_tc(Term root) {
  if (!uop_classify_matmul(root, NULL)) return root;

  // Rebuild: STORE(buf_out, addr_out, OPT(reduce, TC, 0)).
  u64 sloc = term_val(root);
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  Term reduce   = heap_read(sloc + 2);
  Term tc       = uop_opt(reduce, UOP_OPT_TC, 0);
  return uop_store(buf_out, addr_out, tc);
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
