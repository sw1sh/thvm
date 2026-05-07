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

// Wrap the STORE root's value with UOP_OPT(_, TC, 0) when the matmul
// shape matches. Returns the input root unchanged on any non-match.
fn Term uop_recognise_tc(Term root) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return root;

  Term bindings[UPAT_NUM_BINDINGS] = {0};
  if (!upat_match(&rec_tc_store, root, bindings)) return root;

  // Distinctness gate: matmul requires two different source buffers.
  // Bindings 0/2 are the A_buf / B_buf BUFFER terms.
  Term buf_a = bindings[0];
  Term buf_b = bindings[2];
  if (buf_a == 0 || buf_b == 0 || buf_a == buf_b) return root;

  // SUM-only: render_uop's rmu_detect_matmul_tc rejects MAX so we
  // also gate here to avoid annotating a useless wrap.
  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  u64 rloc = term_val(reduce);
  u32 kind = (u32)term_val(heap_read(rloc + 1));
  if (kind != REDUCE_SUM) return root;

  // Rebuild: STORE(buf_out, addr_out, OPT(reduce, TC, 0)).
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  Term tc       = uop_opt(reduce, UOP_OPT_TC, 0);
  return uop_store(buf_out, addr_out, tc);
}
