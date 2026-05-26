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
  u32 kind = uop_reduce_kind(reduce);
  if (kind != REDUCE_SUM) return 0;
  // Conv recogniser expects a single reduce axis (K = Cin*kH*kW flat).
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis = uop_reduce_axis(reduce, 0);

  Term mul = uop_reduce_src(reduce);
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

// Direct multi-axis conv classifier: detect the conv2d shape that
// the multi-axis-REDUCE port (commit 598055ee) emits when shapes
// stay un-flattened.  Vs the flat form above, the reduce has multiple
// axes (Cin, kH, kW separately) and BOTH address trees are linear
// `axis*stride + ...` chains with NO IDIV/IMOD.  The decisive
// distinguishing signature vs a pure multi-axis matmul:
//
//   STORE(out, addr_out,
//     REDUCE_SUM([a4_cin, a5_kh, a6_kw],
//       MUL(INDEX_E(W, a1*W_S + a4*W_S + a5*W_S + a6),
//           INDEX_E(X, a0*X_BS + a4*X_CIN + (a2+a5)*X_ROW + (a3+a6)))))
//
// W's address references only reduce axes + ONE output axis (cOut/a1).
// X's address references reduce axes + multiple output spatial axes
// AND at least one reduce axis appears INSIDE an IADD whose other
// operand is an output axis (the `a2+a5` / `a3+a6` kH/kW offset
// pattern) -- the "reduce axis ALSO offsets an output spatial axis"
// pattern that a pure matmul never has.
//
// Returns 1 + the product of all reduce-axis extents in *out_kred.
// On a non-match returns 0 (and 0 in *out_kred).
//
// Used by hand_opt_is_conv_kernel to flag kid=3-shaped direct conv
// kernels for LOCAL/UPCAST tiling on CUDA (where the metal-only gate
// previously left them un-tiled, ~70x off compute peak).

// Collect every UOP_RANGE axis_id reachable from `t` via the integer
// expression ops.  Bounded depth so a misshapen DAG can't run away.
// Different from rec_tc_count_distinct_ranges in that it also reports
// the parent op kind for each leaf via the `parent_iadd_with_other`
// out-param -- specifically, returns 1 if any of the wanted reduce
// axes appears inside an IADD whose OTHER operand resolves to a
// RANGE leaf of a non-reduce (output) axis.  That's the X-side
// "kw_v + ow" / "kh_v + oh" conv-input signature.
static int rec_conv_axis_added_to_output(Term t, u32 const *red_axes,
                                         u32 n_red, int depth) {
  if (depth > 32) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_IADD) {
    Term a = heap_read(loc + 0);
    Term b = heap_read(loc + 1);
    // Check whether (a is a wanted reduce axis RANGE leaf AND b
    // ultimately references a non-reduce RANGE), or symmetrically b/a.
    for (int swap = 0; swap < 2; swap++) {
      Term lhs = swap == 0 ? a : b;
      Term rhs = swap == 0 ? b : a;
      if (term_tag(lhs) != TAG_UOP || term_ext(lhs) != UOP_RANGE) continue;
      u32 lhs_id = (u32)term_val(heap_read(term_val(lhs) + 0));
      int is_red = 0;
      for (u32 i = 0; i < n_red; i++) if (red_axes[i] == lhs_id) { is_red = 1; break; }
      if (!is_red) continue;
      // Now find a non-reduce RANGE somewhere in rhs.
      u32 seen[8] = {0};
      u32 n_seen = rec_tc_count_distinct_ranges(rhs, seen, 8, 0, 0);
      for (u32 i = 0; i < n_seen; i++) {
        int is_red_i = 0;
        for (u32 j = 0; j < n_red; j++) if (red_axes[j] == seen[i]) { is_red_i = 1; break; }
        if (!is_red_i) return 1;
      }
    }
  }
  // Recurse through the usual integer ops.
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
      if (rec_conv_axis_added_to_output(heap_read(loc + 0), red_axes, n_red, depth + 1)) return 1;
      return rec_conv_axis_added_to_output(heap_read(loc + 1), red_axes, n_red, depth + 1);
    case UOP_IWHERE:
      if (rec_conv_axis_added_to_output(heap_read(loc + 0), red_axes, n_red, depth + 1)) return 1;
      if (rec_conv_axis_added_to_output(heap_read(loc + 1), red_axes, n_red, depth + 1)) return 1;
      return rec_conv_axis_added_to_output(heap_read(loc + 2), red_axes, n_red, depth + 1);
    default:
      return 0;
  }
}

fn int uop_classify_conv2d_direct(Term root, u32 *out_kred) {
  if (out_kred != NULL) *out_kred = 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  // We can't reuse rec_conv_store UPat -- it's reduce-with-MUL-with-INDEX_E
  // structure which matches here too, but the rest of the predicates
  // (IDIV/IMOD presence) differ.  Walk explicitly.
  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  if (uop_reduce_kind(reduce) != REDUCE_SUM) return 0;
  u32 n_red = uop_reduce_n_axes(reduce);
  if (n_red < 2) return 0;                       // multi-axis is the marker
  if (n_red > 8) return 0;                       // sanity / array cap
  u32 red_axes[8];
  u64 kred_prod = 1;
  for (u32 i = 0; i < n_red; i++) red_axes[i] = uop_reduce_axis(reduce, i);

  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term lhs = heap_read(term_val(mul) + 0);
  Term rhs = heap_read(term_val(mul) + 1);
  if (term_tag(lhs) != TAG_UOP || term_ext(lhs) != UOP_INDEX_E) return 0;
  if (term_tag(rhs) != TAG_UOP || term_ext(rhs) != UOP_INDEX_E) return 0;
  Term w_buf  = heap_read(term_val(lhs) + 0);
  Term w_addr = heap_read(term_val(lhs) + 1);
  Term x_buf  = heap_read(term_val(rhs) + 0);
  Term x_addr = heap_read(term_val(rhs) + 1);
  if (w_buf == 0 || x_buf == 0 || w_buf == x_buf) return 0;

  // Both addresses must be linear -- no IDIV/IMOD.  The flat-form
  // classifier above requires IDIV/IMOD; the direct form forbids it
  // (its reduce axes are separate RANGEs, no decomposition needed).
  if (rec_tc_addr_has_divmod(w_addr, 0)) return 0;
  if (rec_tc_addr_has_divmod(x_addr, 0)) return 0;

  // The decisive direct-conv signature: at least one reduce axis
  // appears inside an IADD whose other operand resolves to a RANGE
  // leaf of a non-reduce (output) axis.  That's `(a2 + a5_kh)` /
  // `(a3 + a6_kw)` -- the kernel offset pattern conv input reads
  // produce, that a clean multi-axis matmul never does.  Check X
  // first (most distinctive); fall through to W only if X had no
  // mix (unusual but possible for a transposed conv layout).
  int x_mix = rec_conv_axis_added_to_output(x_addr, red_axes, n_red, 0);
  int w_mix = x_mix ? 0 : rec_conv_axis_added_to_output(w_addr, red_axes, n_red, 0);
  if (!x_mix && !w_mix) return 0;

  // Sum kred across reduce-axis extents found via either address.
  // Walk every reduce axis; at least one address contains the RANGE.
  for (u32 i = 0; i < n_red; i++) {
    u32 e = rec_tc_find_range_extent(w_addr, red_axes[i], 0);
    if (e == 0) e = rec_tc_find_range_extent(x_addr, red_axes[i], 0);
    if (e == 0) return 0;                        // missing range -- bail
    kred_prod *= (u64)e;
    if (kred_prod > 0xFFFFFFFFull) return 0;
  }
  if (out_kred != NULL) *out_kred = (u32)kred_prod;
  return 1;
}

// Combined predicate: flat-form OR direct-multi-axis form.  Used by
// callers (hand_opt_is_conv_kernel) that just need "is this a conv?".
fn int uop_classify_conv2d_any(Term root, u32 *out_kred) {
  if (uop_classify_conv2d(root, out_kred)) return 1;
  return uop_classify_conv2d_direct(root, out_kred);
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
