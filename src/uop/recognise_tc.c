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
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: {
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
  u64 rloc = term_val(reduce);
  u32 kind = (u32)term_val(heap_read(rloc + 1));
  if (kind != REDUCE_SUM) return 0;
  u32 red_axis = (u32)term_val(heap_read(rloc + 2));

  // Walk the MUL's INDEX_E address expressions for a UOP_RANGE leaf
  // whose axis_id matches red_axis; its extent is K.  Mirrors
  // rmu_emit_matmul_tc's K-extent lookup so callers see the same
  // value the renderer will see.
  Term mul    = heap_read(rloc + 0);
  Term addr_a = heap_read(term_val(heap_read(term_val(mul) + 0)) + 1);
  Term addr_b = heap_read(term_val(heap_read(term_val(mul) + 1)) + 1);
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
