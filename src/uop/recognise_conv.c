// uop/recognise_conv.c - pre-render pass that wraps the conv2d_flat
// shape with UOP_OPT(_, CONV, 0).
//
// The conv2d_flat UOp DAG that triggers this recogniser has the shape:
//
//   STORE(C, r_out, REDUCE(MUL(INDEX_E(W, wi), X_VAL), SUM, q_axis))
//
// where r_out = UOP_RANGE(0, LOOP, c_out*patches),
//       q_axis is the reduce-axis id of the second UOP_RANGE,
//       wi    = w_offset + (r_out / patches) * w_stride0 + r_q * w_stride1
//       X_VAL = INDEX_E(X, xi)                  (single-input conv)
//             | UOP_IWHERE chain over xi loads  (multi-input im2col)
//       xi    = composite of bi, ci, oh+kh_v, ow+kw_v sums where the
//               bi/oh/ow factors come from r_out via IDIV/IMOD and
//               ci/kh_v/kw_v come from r_q via IDIV/IMOD.
//
// Distinguishing this from the matmul shape (REDUCE(MUL(INDEX_E,
// INDEX_E), SUM, k) where both addresses are clean linear `m*K+k` /
// `k*N+n` products) hinges on the presence of UOP_IDIV / UOP_IMOD in
// either INDEX_E address tree, OR a UOP_IWHERE second MUL operand
// (multi-input case). A clean matmul never produces IDIV/IMOD or
// IWHERE in its index expressions.
//
// Once the recogniser fires, render_uop's rmu_emit_conv template
// emits the conv2d-shaped output loop + reduce accumulator without
// the matmul-tile structure that the simdgroup_matrix path assumes.
// (The current rmu_emit_store_reduce already correctness-handles this
// shape; CONV template is a perf optimisation.)

// Pattern leaves: only bind the W (LHS) buffer + address. The RHS of
// MUL is left unconstrained because it's either INDEX_E (single-input
// conv) or UOP_IWHERE (multi-input conv). The classifier validates the
// RHS shape in code rather than via UPat.
static UPat const rec_conv_w_buf  = { UOP_BUFFER, 0,    0, 0, NULL,           NULL };
static UPat const rec_conv_w_addr = { 0,          0xFF, 0, 1, NULL,           NULL };
static UPat const rec_conv_w_kids[2] = { rec_conv_w_buf, rec_conv_w_addr };
static UPat const rec_conv_w      = { UOP_INDEX_E, 2,   0, -1, rec_conv_w_kids, NULL };

static UPat const rec_conv_rhs    = { 0, 0xFF, 0, -1, NULL, NULL };
static UPat const rec_conv_mul_kids[2] = { rec_conv_w, rec_conv_rhs };
static UPat const rec_conv_mul    = { UOP_MUL, 2, 0, -1, rec_conv_mul_kids, NULL };

static UPat const rec_conv_red_kids[1] = { rec_conv_mul };
static UPat const rec_conv_reduce = { UOP_REDUCE, 1, 0, -1, rec_conv_red_kids, NULL };

static UPat const rec_conv_store_buf  = { 0, 0xFF, 0, -1, NULL, NULL };
static UPat const rec_conv_store_addr = { 0, 0xFF, 0, -1, NULL, NULL };
static UPat const rec_conv_store_kids[3] = {
  rec_conv_store_buf, rec_conv_store_addr, rec_conv_reduce
};
static UPat const rec_conv_store = { UOP_STORE, 3, 0, -1, rec_conv_store_kids, NULL };

// Walk an IWHERE chain (or a single INDEX_E) collecting every nested
// INDEX_E address tree and OR-reducing rec_tc_addr_has_divmod over
// them. Multi-input conv produces:
//   IWHERE(cond, INDEX_E(X0, xi0), IWHERE(cond, INDEX_E(X1, xi1), ...))
// so we need to peek into each branch's INDEX_E to discover the conv-
// shaped address. Single-input conv just hands us INDEX_E directly.
// rec_tc_addr_has_divmod is defined in recognise_tc.c and re-used
// here for a single source of truth on the conv address signature.
static int rec_conv_rhs_has_conv_address(Term rhs, int depth) {
  if (depth > 32) return 0;
  if (term_tag(rhs) != TAG_UOP) return 0;
  u32 op = term_ext(rhs);
  if (op == UOP_INDEX_E) {
    Term addr = heap_read(term_val(rhs) + 1);
    return rec_tc_addr_has_divmod(addr, 0);
  }
  if (op == UOP_IWHERE) {
    u64 loc = term_val(rhs);
    // Skip the cond (loc+0); recurse into the two branches.
    if (rec_conv_rhs_has_conv_address(heap_read(loc + 1), depth + 1)) return 1;
    return rec_conv_rhs_has_conv_address(heap_read(loc + 2), depth + 1);
  }
  return 0;
}

// Detection helper: returns 1 if `root` is a conv2d-shape STORE
// (REDUCE-of-MUL with W*X structure where at least one side's address
// contains IDIV/IMOD, signalling decomposed r_out/r_q axes). When 1,
// `*out_kred` carries the reduce-axis extent if statically known
// (zero otherwise -- the caller decides whether to bail).
fn int uop_classify_conv2d(Term root, u32 *out_kred) {
  if (out_kred != NULL) *out_kred = 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  Term bindings[UPAT_NUM_BINDINGS] = {0};
  if (!upat_match(&rec_conv_store, root, bindings)) return 0;

  // Reject same-buffer matmul-of-self (W*W); conv has W != X.
  Term w_buf  = bindings[0];
  Term w_addr = bindings[1];
  if (w_buf == 0) return 0;

  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  u64 rloc = term_val(reduce);
  u32 kind = (u32)term_val(heap_read(rloc + 1));
  if (kind != REDUCE_SUM) return 0;
  u32 red_axis = (u32)term_val(heap_read(rloc + 2));

  Term mul = heap_read(rloc + 0);
  Term rhs = heap_read(term_val(mul) + 1);
  if (term_tag(rhs) != TAG_UOP) return 0;

  // The decisive signature: at least one of the W address or the X
  // (RHS) address chain contains UOP_IDIV / UOP_IMOD. A pure matmul
  // never does -- its addresses are clean linear `k*N+n` / `m*K+k`
  // products.
  int w_has    = rec_tc_addr_has_divmod(w_addr, 0);
  int rhs_has  = rec_conv_rhs_has_conv_address(rhs, 0);
  if (!w_has && !rhs_has) return 0;

  // Find KRED extent by scanning W's address for the RANGE leaf with
  // axis_id == red_axis (lifter's r_q has axis_id = red_axis).
  u32 kred = rec_tc_find_range_extent(w_addr, red_axis, 0);
  if (out_kred != NULL) *out_kred = kred;
  return 1;
}

// Wrap the STORE root's value with UOP_OPT(_, CONV, 0) when the conv2d
// shape matches. Returns the input root unchanged on any non-match.
fn Term uop_recognise_conv(Term root) {
  if (!uop_classify_conv2d(root, NULL)) return root;

  // Rebuild: STORE(buf_out, addr_out, OPT(reduce, CONV, 0)).
  u64 sloc = term_val(root);
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  Term reduce   = heap_read(sloc + 2);
  Term conv     = uop_opt(reduce, UOP_OPT_CONV, 0);
  return uop_store(buf_out, addr_out, conv);
}
