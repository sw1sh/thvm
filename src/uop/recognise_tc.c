// uop/recognise_tc.c - classify a matmul STORE(out, addr, REDUCE(SUM, A*B)) and
// wrap its REDUCE in OPT(_, TC, 0), which render_uop's rmu_emit_matmul_tc emits
// as the simdgroup_matrix template.  K need not be a multiple of 8: on a tile
// mismatch the renderer falls back to the scalar accumulator.

// The matmul shape as a UPat; the classifiers walk it by hand instead, as an
// operand may be CAST(INDEX_E).  Braced macros, not named objects: a static
// initializer wants constant expressions (C11 6.7.9p4), as clang 14 enforces.
#define REC_TC_A_BUF  { UOP_BUFFER,  0,    0, 0,  NULL, NULL }
#define REC_TC_A_ADDR { 0,           0xFF, 0, 1,  NULL, NULL }
static UPat const rec_tc_a_kids[2] = { REC_TC_A_BUF, REC_TC_A_ADDR };
#define REC_TC_A      { UOP_INDEX_E, 2,    0, -1, rec_tc_a_kids, NULL }

#define REC_TC_B_BUF  { UOP_BUFFER,  0,    0, 2,  NULL, NULL }
#define REC_TC_B_ADDR { 0,           0xFF, 0, 3,  NULL, NULL }
static UPat const rec_tc_b_kids[2] = { REC_TC_B_BUF, REC_TC_B_ADDR };
#define REC_TC_B      { UOP_INDEX_E, 2,    0, -1, rec_tc_b_kids, NULL }

static UPat const rec_tc_mul_kids[2] = { REC_TC_A, REC_TC_B };
#define REC_TC_MUL    { UOP_MUL,     2,    0, -1, rec_tc_mul_kids, NULL }

// A REDUCE matches its body only; the kind and axis NUMs after it are payload.
static UPat const rec_tc_reduce_kids[1] = { REC_TC_MUL };
#define REC_TC_REDUCE { UOP_REDUCE,  1,    0, -1, rec_tc_reduce_kids, NULL }

static UPat const rec_tc_store_kids[3] = {
  { 0, 0xFF, 0, -1, NULL, NULL }, { 0, 0xFF, 0, -1, NULL, NULL }, REC_TC_REDUCE
};
static UPat const rec_tc_store = { UOP_STORE,   3,    0, -1, rec_tc_store_kids, NULL };

// The INDEX_E under one CAST/BITCAST (a bf16 widening or q8's int8 dequant),
// else `t`; rmu_emit_matmul_tc peels the same cast.
static Term rec_tc_peel_cast(Term t) {
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  if (op == UOP_CAST || op == UOP_BITCAST) {
    Term src = heap_read(term_val(t) + 0);
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_INDEX_E) return src;
  }
  return t;
}

// THVM_FUSE_MATMUL_INPUT: the first INDEX_E leaf of a fused A producer, or 0.
// Every leaf indexes at the matmul's (m, k), so any one gives A's layout.
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

// Add the distinct RANGE ids of index expression `t` to seen[n_seen..cap);
// returns the new count.
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

// 1 iff index expression `t` holds an IDIV or IMOD: a conv2d_flat lift packs
// (co, bi, oh, ow) into one range that way, a matmul's m*K+k never does.
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

// The extent of RANGE `want_axis_id` in index expression `t`, or 0.
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

// 1 iff `root` is a matmul STORE: a SUM REDUCE of a MUL over two distinct
// INDEX_E buffers.  *out_k_extent gets K (0 if unresolved), *out_unit_axis 1 or
// 2 when A's M or B's N collapsed to size 1, else 0; either may be NULL.
fn int uop_classify_matmul(Term root, u32 *out_k_extent, u32 *out_unit_axis) {
  if (out_k_extent != NULL) *out_k_extent = 0;
  if (out_unit_axis != NULL) *out_unit_axis = 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  // An OPT_TC value does not match, so the wrap is idempotent; callers that
  // re-classify a wrapped root peel it first (udg_peel_tc_opt).
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  u32 kind = uop_reduce_kind(reduce);
  if (kind != REDUCE_SUM) return 0;
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis = uop_reduce_axis(reduce, 0);

  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term idx_a = rec_tc_peel_cast(heap_read(term_val(mul) + 0));
  Term idx_b = rec_tc_peel_cast(heap_read(term_val(mul) + 1));
  // THVM_FUSE_MATMUL_INPUT: A may be a fused elementwise producer; B must stay
  // a plain buffer read.
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
  // x*x is no matmul; a fused A, an ALU tree, is exempt from that check.
  if (!a_fused && term_val(buf_a) == term_val(buf_b)) return 0;

  Term addr_a = heap_read(term_val(idx_a) + 1);
  Term addr_b = heap_read(term_val(idx_b) + 1);

  // Matmul addresses name 2 ranges each (m*K+k, k*N+n); a conv's X names 4 or
  // more, which the simdgroup loads would misread.  A size-1 M or N is CONST(0)
  // (ru_new_range, as tinygrad schedule/indexing.py new_range), leaving only K.
  u32 seen_a[8] = {0};
  u32 seen_b[8] = {0};
  u32 n_a = rec_tc_count_distinct_ranges(addr_a, seen_a, 8, 0, 0);
  u32 n_b = rec_tc_count_distinct_ranges(addr_b, seen_b, 8, 0, 0);
  u32 unit_axis = 0;
  if (n_a == 2 && n_b == 2) {
    unit_axis = 0;
  } else if (n_a == 1 && n_b == 2) {
    if (seen_a[0] != red_axis) return 0;
    unit_axis = 1;
  } else if (n_a == 2 && n_b == 1) {
    if (seen_b[0] != red_axis) return 0;
    unit_axis = 2;
  } else {
    return 0;
  }
  if (out_unit_axis != NULL) *out_unit_axis = unit_axis;

  // A conv2d_flat lift also names 2 ranges per address, through IDIV/IMOD.
  if (rec_tc_addr_has_divmod(addr_a, 0)) return 0;
  if (rec_tc_addr_has_divmod(addr_b, 0)) return 0;

  // K as rmu_emit_matmul_tc finds it: the extent of the reduce axis' RANGE.
  u32 ka = rec_tc_find_range_extent(addr_a, red_axis, 0);
  if (ka == 0) ka = rec_tc_find_range_extent(addr_b, red_axis, 0);
  if (out_k_extent != NULL) *out_k_extent = ka;
  return 1;
}

// THVM_FUSE_MATMUL_EPILOGUE: remove_bufferize (rangeify_unified.c) inlines a
// matmul REDUCE into its elementwise consumer; the helpers below wrap it there.

// Index arithmetic over RANGEs is epilogue-safe (rmu_epilogue_is_index_int_op):
// a broadcast gate's EXPAND shows up as an IWHERE/IMOD index, not as movement.
static int rec_tc_is_index_int_op(u32 op) {
  return op == UOP_IADD || op == UOP_ISUB || op == UOP_IMUL || op == UOP_IDIV
      || op == UOP_IMOD || op == UOP_ILT  || op == UOP_IAND || op == UOP_IOR
      || op == UOP_IXOR || op == UOP_ISHR || op == UOP_IWHERE;
}

// rmu_epilogue_walk with a bare REDUCE where the renderer wants an OPT_TC:
// (Term)1 when `t` is only elementwise, cast and index ops over leaves, 0 on
// any other op.  Counts the REDUCEs into *n_red, the last in *out_red.
static Term rec_tc_epilogue_walk(Term t, Term *out_red, int *n_red, int depth) {
  if (depth > 48) return (Term)1;            // deeper nodes go unchecked
  if (term_tag(t) != TAG_UOP) return (Term)1;
  u32 op = term_ext(t);
  if (op == UOP_REDUCE) { *out_red = t; (*n_red)++; return (Term)1; }
  if (op == UOP_CONST || op == UOP_INDEX_E || op == UOP_BUFFER
      || op == UOP_RANGE) return (Term)1;
  if (op == UOP_OPT) return 0;               // already wrapped: not a bare chain
  if (op == UOP_CAST || op == UOP_BITCAST
      || uop_is_unary_elementwise((u8)op)
      || uop_is_binary_elementwise((u8)op)
      || uop_is_ternary_elementwise((u8)op)
      || rec_tc_is_index_int_op(op)) {
    u8 ar = uop_arity((u8)op);
    for (u8 i = 0; i < ar; i++) {
      if (rec_tc_epilogue_walk(heap_read(term_val(t) + i), out_red, n_red,
                               depth + 1) == 0)
        return 0;
    }
    return (Term)1;
  }
  return 0;
}
// The one REDUCE under an epilogue chain, or 0.
static Term rec_tc_find_epilogue_reduce(Term value) {
  Term red = 0; int n = 0;
  if (rec_tc_epilogue_walk(value, &red, &n, 0) == 0) return 0;
  if (n != 1 || red == 0) return 0;
  return red;
}

// Replace `target` by `repl` under `t`, rebuilding each Term at most once.
// `repl` is not walked, so a wrapped OPT(target) cannot match again.
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

// The bare-REDUCE STORE(buf, addr, reduce) of an epilogue store, whose REDUCE
// shares the output's M and N ranges; 0 without one epilogue REDUCE.
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

// `root` with its nested `reduce` wrapped in OPT(_, TC, 0), the chain kept.
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

// THVM_FUSE_MATMUL_EPILOGUE: 1 iff `root` is an epilogue chain over a 2x2
// matmul, with its K (and M/N axes if asked), so hand_opts applies KOP_TC
// before an elementwise UPCAST could split M/N into 3-range operands.
fn int uop_classify_epilogue_matmul(Term root, u32 *out_K,
                                    u32 *out_m_axis, u32 *out_m_extent,
                                    u32 *out_n_axis, u32 *out_n_extent) {
  if (out_K        != NULL) *out_K        = 0;
  if (out_m_axis   != NULL) *out_m_axis   = 0xFFFFFFFFu;
  if (out_n_axis   != NULL) *out_n_axis   = 0xFFFFFFFFu;
  if (out_m_extent != NULL) *out_m_extent = 0;
  if (out_n_extent != NULL) *out_n_extent = 0;
  if (!ru_fuse_epilogue_on()) return 0;
  Term reduce = 0;
  Term synth = rec_tc_synth_matmul_store(root, &reduce);
  if (synth == 0) return 0;
  u32 em_axis, em_ext, en_axis, en_ext, ek_ext = 0, unit_axis = 0;
  if (!uop_classify_matmul(synth, &ek_ext, &unit_axis) || unit_axis != 0) return 0;
  if (!uop_matmul_mn_axes(synth, &em_axis, &em_ext, &en_axis, &en_ext)) return 0;
  if (ek_ext == 0) return 0;
  if (out_K        != NULL) *out_K        = ek_ext;
  if (out_m_axis   != NULL) *out_m_axis   = em_axis;
  if (out_m_extent != NULL) *out_m_extent = em_ext;
  if (out_n_axis   != NULL) *out_n_axis   = en_axis;
  if (out_n_extent != NULL) *out_n_extent = en_ext;
  return 1;
}

// `root` with its matmul REDUCE wrapped in OPT(_, TC, 0), else `root` itself.
fn Term uop_recognise_tc(Term root) {
  if (ru_fuse_epilogue_on()) {
    Term reduce = 0;
    Term synth = rec_tc_synth_matmul_store(root, &reduce);
    if (synth != 0) {
      u32 unit_axis = 0;
      if (uop_classify_matmul(synth, NULL, &unit_axis) && unit_axis == 0)
        return rec_tc_wrap_epilogue_reduce(root, reduce);
      return root;
    }
  }
  // The simdgroup template needs both operands 2-range: a size-1 M or N stays
  // on the generic accumulator or the BLAS GEMV path.
  u32 unit_axis = 0;
  if (!uop_classify_matmul(root, NULL, &unit_axis) || unit_axis != 0) return root;

  u64 sloc = term_val(root);
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  Term reduce   = heap_read(sloc + 2);
  Term tc       = uop_opt(reduce, UOP_OPT_TC, 0);
  return uop_store(buf_out, addr_out, tc);
}

// The M axis (A's non-reduce range) and N axis (B's) of a 2x2 matmul STORE with
// their extents, found as rmu_emit_matmul_tc finds them; 1 on a match.
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
  Term idx_a   = rec_tc_peel_cast(heap_read(term_val(mul) + 0));
  Term idx_b   = rec_tc_peel_cast(heap_read(term_val(mul) + 1));
  if (!(term_tag(idx_a) == TAG_UOP && term_ext(idx_a) == UOP_INDEX_E)
      && ru_fuse_matmul_input_on()) {
    Term rep = rec_tc_fused_first_index(idx_a, 0);
    if (rep != 0) idx_a = rep;
  }
  Term addr_a  = heap_read(term_val(idx_a) + 1);
  Term addr_b  = heap_read(term_val(idx_b) + 1);

  // A's ranges are {M, K}, B's {K, N}; a size-1 M or N leaves just K.
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

// THVM_FUSE_MATMUL_EPILOGUE: an epilogue broadcast (FLUX's x + gate*matmul)
// can split M or N over several LOOP ranges.  A contiguous split folds into
// one flat range, which keeps every address; any other shape is left as is.
static int rec_tc_const_i64(Term t, i64 *out) {
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term num = heap_read(term_val(t));
  if (term_tag(num) != TAG_NUM) return 0;
  *out = (i64)(i32)(u32)term_val(num);
  return 1;
}
// The stride of RANGE `axis_id` in linear index `t`; *ok = 0 when the axis sits
// under a non-linear op.
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
    if (ca == 0 && cb == 0) return 0;
    *ok = 0; return 0;
  }
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
// 1 iff the split axes ax_ord[0..n), outer to inner, nest contiguously in
// `addr`: stride(outer) == stride(inner) * extent(inner).  An axis `addr` lacks
// (the output need not index every operand axis) restarts the chain.
static int rec_tc_split_contiguous(Term addr, u32 *ax_ord, u32 *ex_ord, u32 n) {
  i64 prev = 0; int have_prev = 0;
  for (u32 i = 0; i < n; i++) {
    int ok = 1;
    i64 c = rec_tc_addr_coeff(addr, ax_ord[i], &ok, 0);
    if (!ok) return 0;
    if (c == 0) { have_prev = 0; continue; }
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
  // A non-K axis of both operands is a batch axis, which folding into M or N
  // would miscompile; only axes unique to one operand split M or N.
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
      if (shared) continue;
      if (n_nk < 8) nk[n_nk++] = seen[i];
    }
    if (n_nk <= 1) continue;
    i64 cf[8]; u32 ex[8]; int decline = 0;
    for (u32 i = 0; i < n_nk; i++) {
      int ok = 1;
      cf[i] = rec_tc_addr_coeff(addr, nk[i], &ok, 0);
      if (!ok || cf[i] <= 0) return root;
      ex[i] = rec_tc_find_range_extent(addr, nk[i], 0);
      Term rn = rec_tc_find_range_node(addr, nk[i], 0);
      if (ex[i] == 0 || rn == 0) return root;
      u32 kax = (u32)term_val(heap_read(term_val(rn) + 1));
      if (kax != KAX_LOOP) { decline = 1; break; }
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

// uop_recognise_tc, plus M and N stamped KAX_GLOBAL when K, M and N are
// multiples of 8: rmu_emit_matmul_tc then gives each 8x8 output tile its own
// simdgroup (no write race) and cg_tile_metal_dispatch_shape launches the grid.
fn Term uop_recognise_tc_parallel(Term root) {
  if (ru_fuse_epilogue_on()) root = rec_tc_collapse_split(root);
  if (ru_fuse_epilogue_on()) {
    Term reduce = 0;
    Term synth = rec_tc_synth_matmul_store(root, &reduce);
    if (synth != 0) {
      u32 em_axis, em_ext, en_axis, en_ext, ek_ext = 0;
      if (!uop_matmul_mn_axes(synth, &em_axis, &em_ext, &en_axis, &en_ext))
        return uop_recognise_tc(root);
      uop_classify_matmul(synth, &ek_ext, NULL);
      if ((em_ext % 8u) != 0 || (en_ext % 8u) != 0
          || ek_ext == 0 || (ek_ext % 8u) != 0)
        return uop_recognise_tc(root);
      Term tc = uop_recognise_tc(root);
      if (tc == root) return root;
      tc = uop_dag_apply_global(tc, em_axis);
      tc = uop_dag_apply_global(tc, en_axis);
      return tc;
    }
  }
  u32 m_axis, m_extent, n_axis, n_extent;
  if (!uop_matmul_mn_axes(root, &m_axis, &m_extent, &n_axis, &n_extent)) {
    return uop_recognise_tc(root);
  }
  if ((m_extent % 8u) != 0 || (n_extent % 8u) != 0) {
    // Ragged M/N fall back to the scalar accumulator, whose grid is
    // output_numel: keep them LOOP.
    return uop_recognise_tc(root);
  }

  // The tile path also needs K % 8 == 0 (rmu_emit_matmul_tc checks it too).
  u32 k_extent = 0;
  uop_classify_matmul(root, &k_extent, NULL);
  if (k_extent == 0 || (k_extent % 8u) != 0) {
    return uop_recognise_tc(root);
  }

  Term tc = uop_recognise_tc(root);
  if (tc == root) return root;
  tc = uop_dag_apply_global(tc, m_axis);
  tc = uop_dag_apply_global(tc, n_axis);
  return tc;
}

// Batched matmul (THeadAttention's mhaBmm): A names {batch, M, K}, B names
// {batch, K, N}, the output {batch, M, N}.  Operands may be permuted views, so
// an axis is told by which addresses hold it.  1 and the axes on a match.
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

  u64 sloc = term_val(root);
  Term reduce = heap_read(sloc + 2);
  // Peel an OPT_TC, as udg_peel_tc_opt does: the dispatch-shape path
  // re-classifies a root hand_opts already wrapped.
  while (term_tag(reduce) == TAG_UOP && term_ext(reduce) == UOP_OPT
         && uop_opt_kind(reduce) == UOP_OPT_TC) {
    reduce = uop_opt_target(reduce);
  }
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  if (uop_reduce_kind(reduce) != REDUCE_SUM) return 0;
  if (uop_reduce_n_axes(reduce) != 1) return 0;
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

  u32 seen_a[8] = {0}, seen_b[8] = {0}, seen_o[8] = {0};
  u32 n_a = rec_tc_count_distinct_ranges(addr_a, seen_a, 8, 0, 0);
  u32 n_b = rec_tc_count_distinct_ranges(addr_b, seen_b, 8, 0, 0);
  u32 n_o = rec_tc_count_distinct_ranges(addr_o, seen_o, 8, 0, 0);
  if (n_a != 3 || n_b != 3 || n_o != 3) return 0;

  u32 m_axis = 0xFFFFFFFFu, n_axis = 0xFFFFFFFFu, batch_axis = 0xFFFFFFFFu;
  for (u32 i = 0; i < n_o; i++) {
    u32 aid = seen_o[i];
    if (aid == red_axis) return 0;
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

// uop_recognise_tc_parallel for the batched matmul: OPT_TC and batch, M, N
// GLOBAL when M, N, K are multiples of 8; `root` unchanged otherwise.
fn Term uop_recognise_batched_tc_parallel(Term root) {
  u32 batch_axis, batch_extent, m_axis, m_extent, n_axis, n_extent, k_extent;
  if (!uop_classify_batched_matmul(root, &batch_axis, &batch_extent,
                                   &m_axis, &m_extent, &n_axis, &n_extent,
                                   &k_extent)) {
    return root;
  }
  if ((m_extent % 8u) != 0 || (n_extent % 8u) != 0
      || (k_extent % 8u) != 0) {
    return root;
  }
  // hand_opts may have wrapped it already; a second OPT layer would break
  // rmu_detect_matmul_tc's OPT(REDUCE) match.
  u64 sloc = term_val(root);
  Term value = heap_read(sloc + 2);
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
      && uop_opt_kind(value) == UOP_OPT_TC) {
    return root;
  }
  Term buf_out  = heap_read(sloc + 0);
  Term addr_out = heap_read(sloc + 1);
  Term tc       = uop_opt(value, UOP_OPT_TC, 0);
  Term out      = uop_store(buf_out, addr_out, tc);
  out = uop_dag_apply_global(out, batch_axis);
  out = uop_dag_apply_global(out, m_axis);
  out = uop_dag_apply_global(out, n_axis);
  return out;
}

