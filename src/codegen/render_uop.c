// codegen/render_uop.c - UOp DAG renderer.
//
// Walks the UOp DAG rooted at a kernel-output store and emits MSL.
// Coverage:
//
//   UOP_BUFFER         -- kernel arg or local alloc
//   UOP_INDEX_E        -- buf[addr]
//   UOP_STORE          -- buf[addr] = value;
//   UOP_AFTER          -- threadgroup_barrier when cross-scope
//   UOP_RANGE          -- for-loop or thread-position bind
//   UOP_OPT            -- annotation on target (UNROLL/UPCAST/TC/...)
//   UOP_CONST/ICONST   -- literal value
//   UOP_IADD/IMUL/etc. -- symbolic int expressions
//   UOP_ADD/MUL/NEG/...  -- float elementwise + transcendentals
//   UOP_REDUCE         -- hoisted accumulator (init/accum/finalize)
//   UOP_CAST/BITCAST   -- type conversion
//   UOP_IWHERE         -- ternary
//
// Pattern-matches for specialised templates:
//   OPT(REDUCE(MUL(LOAD,LOAD), SUM, k), TC, _) with K%8==0
//     -> 8x8 simdgroup_matrix<float, 8, 8> template.
//   OPT(_, UNROLL/UPCAST, factor) -> #pragma unroll(N).
//   OPT(_, LOCAL, _) -> bind to thread_position_in_threadgroup.

static void rmu_emit_term(Term t, FILE *fp);

// Buffer name map: cg_render_uop_kernel populates this with the
// kernel's output buffer (Term -> "out") and inputs (Term -> "in0",
// "in1", ...).  Inner functions emit buffer references via
// rmu_buf_name(t) instead of `buf<heap_loc>`.  Static global is fine
// because the renderer isn't re-entrant in practice; cleared at start
// of each render to defend against stale state.
#define RMU_BUF_MAX 32
static struct { Term term; char name[16]; } RMU_BUF_NAMES[RMU_BUF_MAX];
static u32 RMU_BUF_NAMES_N;

static void rmu_buf_names_reset(void) {
  RMU_BUF_NAMES_N = 0;
}

static void rmu_buf_names_set(Term t, const char *name) {
  if (RMU_BUF_NAMES_N >= RMU_BUF_MAX) return;
  RMU_BUF_NAMES[RMU_BUF_NAMES_N].term = t;
  snprintf(RMU_BUF_NAMES[RMU_BUF_NAMES_N].name,
           sizeof(RMU_BUF_NAMES[0].name), "%s", name);
  RMU_BUF_NAMES_N++;
}

// Returns the symbolic name for buffer `t`, or a `buf<loc>` fallback
// when no map entry exists (synthetic kernels in unit tests, or
// future paths).
static const char *rmu_buf_name(Term t) {
  for (u32 i = 0; i < RMU_BUF_NAMES_N; i++) {
    if (RMU_BUF_NAMES[i].term == t) return RMU_BUF_NAMES[i].name;
  }
  static char fallback[24];
  snprintf(fallback, sizeof(fallback), "buf%llu",
           (unsigned long long)term_val(t));
  return fallback;
}

static const char *rmu_msl_type_name(u32 dtype) {
  switch (dtype) {
    case DT_FP32:  return "float";
    case DT_FP16:  return "half";
    case DT_INT32: return "int";
    case DT_INT64: return "long";
    case DT_UINT8: return "uchar";
    default:       return "float";  // safe fallback for the renderer
  }
}

static const char *rmu_int_op_name(u32 op) {
  switch (op) {
    case UOP_IADD: return "+";
    case UOP_ISUB: return "-";
    case UOP_IMUL: return "*";
    case UOP_IDIV: return "/";
    case UOP_IMOD: return "%";
    case UOP_ILT:  return "<";
    case UOP_IAND: return "&";
    default:       return "?";
  }
}

// Emit a symbolic int expression (UOP_RANGE / UOP_I* / UOP_IWHERE /
// UOP_INVALID / UOP_CONST).  Recursive; parenthesises binary ops to
// keep precedence unambiguous.
static void rmu_emit_term(Term t, FILE *fp) {
  if (term_tag(t) != TAG_UOP) {
    if (term_tag(t) == TAG_NUM) {
      fprintf(fp, "%u", (u32)term_val(t));
      return;
    }
    fputs("/*?*/", fp);
    return;
  }
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_CONST: {
      u32 dtype = term_ext(heap_read(loc));
      u32 bits  = (u32)term_val(heap_read(loc));
      if (dtype == DT_FP32) {
        union { u32 b; float f; } pun = { .b = bits };
        fprintf(fp, "%ff", pun.f);
      } else {
        fprintf(fp, "%d", (int)bits);
      }
      return;
    }
    case UOP_RANGE: {
      u32 axis_id = term_val(heap_read(loc + 0));
      fprintf(fp, "a%u", axis_id);
      return;
    }
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD: case UOP_ILT:
    case UOP_IAND: {
      Term a = heap_read(loc + 0);
      Term b = heap_read(loc + 1);
      fputs("(", fp);
      rmu_emit_term(a, fp);
      fprintf(fp, " %s ", rmu_int_op_name(op));
      rmu_emit_term(b, fp);
      fputs(")", fp);
      return;
    }
    // Float elementwise binary ops.  Same parenthesised shape as
    // the int binaries; renderer emits MSL infix operators.
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ: {
      const char *infix = (op == UOP_ADD)   ? "+"
                        : (op == UOP_MUL)   ? "*"
                        : (op == UOP_CMPLT) ? "<"
                        :                     "==";
      Term a = heap_read(loc + 0);
      Term b = heap_read(loc + 1);
      fputs("(", fp);
      rmu_emit_term(a, fp);
      fprintf(fp, " %s ", infix);
      rmu_emit_term(b, fp);
      fputs(")", fp);
      return;
    }
    // Float elementwise unary ops.  RECIP -> 1.0f/x, EXP2 / LOG2 /
    // SQRT use MSL builtins.  NEG via prefix `-`.
    case UOP_NEG: {
      fputs("(-", fp);
      rmu_emit_term(heap_read(loc + 0), fp);
      fputs(")", fp);
      return;
    }
    case UOP_RECIP: {
      fputs("(1.0f/", fp);
      rmu_emit_term(heap_read(loc + 0), fp);
      fputs(")", fp);
      return;
    }
    case UOP_EXP2: case UOP_LOG2: case UOP_SQRT: {
      const char *fn_name = (op == UOP_EXP2) ? "exp2"
                          : (op == UOP_LOG2) ? "log2"
                          :                    "sqrt";
      fprintf(fp, "%s(", fn_name);
      rmu_emit_term(heap_read(loc + 0), fp);
      fputs(")", fp);
      return;
    }
    case UOP_CAST: case UOP_BITCAST: {
      // heap = [src, NUM(dst_dtype)].
      Term src      = heap_read(loc + 0);
      u32  dst_dt   = term_val(heap_read(loc + 1));
      const char *fn_name = (op == UOP_CAST) ? "" : "as_type";
      if (op == UOP_BITCAST) {
        fprintf(fp, "as_type<%s>(", rmu_msl_type_name(dst_dt));
        rmu_emit_term(src, fp);
        fputs(")", fp);
      } else {
        fprintf(fp, "((%s)", rmu_msl_type_name(dst_dt));
        rmu_emit_term(src, fp);
        fputs(")", fp);
      }
      (void)fn_name;
      return;
    }
    case UOP_IWHERE: {
      Term cond = heap_read(loc + 0);
      Term tv   = heap_read(loc + 1);
      Term ev   = heap_read(loc + 2);
      fputs("(", fp);
      rmu_emit_term(cond, fp);
      fputs(" ? ", fp);
      rmu_emit_term(tv, fp);
      fputs(" : ", fp);
      rmu_emit_term(ev, fp);
      fputs(")", fp);
      return;
    }
    case UOP_INVALID:
      fputs("/*INVALID*/0", fp);
      return;
    case UOP_INDEX_E: {
      Term buf  = heap_read(loc + 0);
      Term addr = heap_read(loc + 1);
      // Buffer is rendered by name; for now we use the heap loc
      // as the identifier.  F0+ wires names through a BUFFER->id
      // table so kernel args / local allocs resolve to `inN` /
      // `_alloc<id>` consistently.
      fprintf(fp, "%s[", rmu_buf_name(buf));
      rmu_emit_term(addr, fp);
      fputs("]", fp);
      return;
    }
    case UOP_BUFFER:
      fputs(rmu_buf_name(t), fp);
      return;
    case UOP_OPT: {
      // Annotation: render the target, ignore the directive in F0
      // (F1+ pattern-matches for specialised templates).
      rmu_emit_term(heap_read(loc + 0), fp);
      return;
    }
    case UOP_REDUCE: {
      // When REDUCE appears in an expression context (not directly as
      // STORE.value), the caller has hoisted an accumulator outside.
      // We emit the placeholder name `_acc<axis>`; the caller emits
      // the init / reduce-axis loop / combine code before the
      // expression and just substitutes here.
      u32 axis = term_val(heap_read(loc + 2));
      fprintf(fp, "_acc%u", axis);
      return;
    }
    default:
      fprintf(fp, "/*uop%u*/", op);
      return;
  }
}

// Walk a term tree collecting unique UOP_RANGE leaves in encounter
// order, plus optional UOP_OPT annotations attached to each range.
// Used by rmu_emit_store to wrap the store body in for-loops over
// every range that appears in the addr / value expressions.
//
// Up to MAX_DIM ranges per kernel; duplicates skipped (same axis_id
// only emits one for-loop).  When a range is encountered via a
// wrapping OPT(range, kind, factor) we record (kind, factor) into
// `opt_kinds[]` / `opt_factors[]`.  RMU_NO_OPT marks "no OPT wrap"
// distinctly from "OPT(_, UNROLL, _)" since UOP_OPT_UNROLL == 0
// would otherwise collide with the zero-init default.
#define RMU_NO_OPT 0xFFu
static void rmu_collect_ranges_rec(Term t, Term *ranges,
                                   u32 *opt_kinds, u32 *opt_factors,
                                   u32 *n_out,
                                   u32 inherit_kind, u32 inherit_factor) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= MAX_DIM) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 axis_id = term_val(heap_read(loc + 0));
    for (u32 i = 0; i < *n_out; i++) {
      u32 existing = term_val(heap_read(term_val(ranges[i]) + 0));
      if (existing == axis_id) return;
    }
    ranges     [*n_out] = t;
    opt_kinds  [*n_out] = inherit_kind;
    opt_factors[*n_out] = inherit_factor;
    (*n_out)++;
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_collect_ranges_rec(heap_read(loc + 0), ranges, opt_kinds,
                             opt_factors, n_out, RMU_NO_OPT, 0);
      rmu_collect_ranges_rec(heap_read(loc + 1), ranges, opt_kinds,
                             opt_factors, n_out, RMU_NO_OPT, 0);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
      rmu_collect_ranges_rec(heap_read(loc + 0), ranges, opt_kinds,
                             opt_factors, n_out, RMU_NO_OPT, 0);
      return;
    case UOP_IWHERE:
      rmu_collect_ranges_rec(heap_read(loc + 0), ranges, opt_kinds,
                             opt_factors, n_out, RMU_NO_OPT, 0);
      rmu_collect_ranges_rec(heap_read(loc + 1), ranges, opt_kinds,
                             opt_factors, n_out, RMU_NO_OPT, 0);
      rmu_collect_ranges_rec(heap_read(loc + 2), ranges, opt_kinds,
                             opt_factors, n_out, RMU_NO_OPT, 0);
      return;
    case UOP_OPT: {
      // OPT(target, kind, factor): inherit annotation into the
      // recursed target's collection.  Stacked OPTs accumulate the
      // outermost kind (last-wins for now).
      u32 kind   = term_val(heap_read(loc + 1));
      u32 factor = term_val(heap_read(loc + 2));
      rmu_collect_ranges_rec(heap_read(loc + 0), ranges, opt_kinds,
                             opt_factors, n_out, kind, factor);
      return;
    }
    case UOP_CAST: case UOP_BITCAST:
      rmu_collect_ranges_rec(heap_read(loc + 0), ranges, opt_kinds,
                             opt_factors, n_out, RMU_NO_OPT, 0);
      return;
    default:
      return;
  }
}

// Walk a term tree collecting UOP_REDUCE nodes for hoisting.  Each
// REDUCE produces a separate accumulator.  Up to MAX_DIM reduces.
static void rmu_collect_reduces(Term t, Term *reduces, u32 *n_out) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= MAX_DIM) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) {
    // Dedup by REDUCE term identity.
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) return;
    }
    reduces[*n_out] = t;
    (*n_out)++;
    // Don't recurse into the body -- the outer accumulator handles it.
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
      rmu_collect_reduces(heap_read(loc + 1), reduces, n_out);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
      return;
    case UOP_IWHERE:
      rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
      rmu_collect_reduces(heap_read(loc + 1), reduces, n_out);
      rmu_collect_reduces(heap_read(loc + 2), reduces, n_out);
      return;
    default:
      return;
  }
}

static void rmu_collect_ranges(Term t, Term *ranges, u32 *n_out) {
  u32 dummy_kinds[MAX_DIM]   = {0};
  u32 dummy_factors[MAX_DIM] = {0};
  rmu_collect_ranges_rec(t, ranges, dummy_kinds, dummy_factors, n_out,
                         0, 0);
}

static void rmu_collect_ranges_with_opts(Term t, Term *ranges,
                                         u32 *opt_kinds,
                                         u32 *opt_factors,
                                         u32 *n_out) {
  rmu_collect_ranges_rec(t, ranges, opt_kinds, opt_factors, n_out,
                         RMU_NO_OPT, 0);
}

// Returns 1 if `kind`/`axis_type` indicates the axis binds directly to
// a thread/group position (no for-loop emitted).  These paths emit a
// `uint aN = tt;` or `uint aN = tg;` declaration instead.
static int rmu_axis_is_threadbound(u32 opt_kind, u32 axis_type) {
  return (opt_kind == UOP_OPT_LOCAL)
      || axis_type == 4  /* legacy KAX_LOCAL  */
      || axis_type == 5  /* legacy KAX_GLOBAL */;
}

// Emit a for-loop opener for a UOP_RANGE leaf at the given depth.
// `opt_kind` / `opt_factor` are the OPT annotation (0 if none).  Three
// patterns:
//   LOCAL  -> `uint aN = tt; /* local */`   (thread-position bind)
//   GLOBAL -> `uint aN = tg; /* global */`  (group-position bind)
//   else   -> `for (uint aN = 0; aN < ext; aN++)` with optional /*reduce*/
//             marker when axis_type == REDUCE.
static void rmu_emit_range_open(Term r, FILE *fp, u32 depth,
                                u32 opt_kind) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return;
  u64 loc = term_val(r);
  u32 axis_id   = term_val(heap_read(loc + 0));
  u32 axis_type = term_val(heap_read(loc + 1));
  u32 extent    = term_val(heap_read(loc + 2));
  for (u32 i = 0; i < depth; i++) fputs("  ", fp);
  // LOCAL via OPT annotation OR via axis_type == 4 (legacy KAX_LOCAL).
  if (opt_kind == UOP_OPT_LOCAL || axis_type == 4) {
    fprintf(fp, "uint a%u = tt; /* local ext=%u */\n", axis_id, extent);
    return;
  }
  if (axis_type == 5 /* legacy KAX_GLOBAL */) {
    fprintf(fp, "uint a%u = tg; /* global ext=%u */\n", axis_id, extent);
    return;
  }
  if (axis_type == 1 /*REDUCE*/) {
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u++) /*reduce*/ {\n",
            axis_id, axis_id, extent, axis_id);
  } else {
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u++) {\n",
            axis_id, axis_id, extent, axis_id);
  }
}

// Recognise the canonical matmul shape:
//   STORE(C, addr_C, OPT(REDUCE(MUL(INDEX_E(A,_), INDEX_E(B,_)),
//                              SUM, k_axis), TC, _))
// The OPT wrapper marks the reduction as a tensor-core target.  The
// detection is structural; if the shape matches, returns 1 and fills
// `*out_red_value` with the inner REDUCE term so the caller can fall
// back to F1e's accumulator path while wrapping with TC markers.
// (F2b: emit a real simdgroup_matrix template instead of falling
// back.)
static int rmu_detect_matmul_tc(Term store, Term *out_red_value) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  Term value = heap_read(term_val(store) + 2);
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_OPT) return 0;
  if (uop_opt_kind(value) != UOP_OPT_TC) return 0;
  Term inner = uop_opt_target(value);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_REDUCE) return 0;
  u64 rloc = term_val(inner);
  u32 kind = term_val(heap_read(rloc + 1));
  if (kind != REDUCE_SUM) return 0;
  Term mul = heap_read(rloc + 0);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term lhs = heap_read(term_val(mul) + 0);
  Term rhs = heap_read(term_val(mul) + 1);
  // LHS / RHS must be INDEX_E reads (or wrapped in identity / load).
  if (term_tag(lhs) != TAG_UOP || term_ext(lhs) != UOP_INDEX_E) return 0;
  if (term_tag(rhs) != TAG_UOP || term_ext(rhs) != UOP_INDEX_E) return 0;
  if (out_red_value != NULL) *out_red_value = inner;
  return 1;
}

// Filter the collected ranges, splitting them into output ranges
// (axis_id != reduce_axis) and the reduce range (axis_id == reduce_axis,
// at most one).  Used by the REDUCE-as-store-value shape so the
// renderer can emit output loops outside, accumulator+inner loop
// inside.  Returns the index of the reduce range in the input array,
// or n_ranges if not found.
static u32 rmu_split_reduce(Term *ranges, u32 *opt_kinds,
                            u32 *opt_factors, u32 n_ranges,
                            u32 reduce_axis,
                            Term *out_ranges, u32 *out_kinds,
                            u32 *out_factors, u32 *n_out) {
  u32 reduce_idx = n_ranges;
  *n_out = 0;
  for (u32 i = 0; i < n_ranges; i++) {
    u32 axis_id = term_val(heap_read(term_val(ranges[i]) + 0));
    if (axis_id == reduce_axis) {
      reduce_idx = i;
      continue;
    }
    out_ranges  [*n_out] = ranges[i];
    out_kinds   [*n_out] = opt_kinds[i];
    out_factors [*n_out] = opt_factors[i];
    (*n_out)++;
  }
  return reduce_idx;
}

// Emit `<acc> = <combine(kind, acc, src)>;` per the REDUCE kind.
static void rmu_emit_reduce_combine(const char *acc_name, u32 kind,
                                    Term src, FILE *fp) {
  if (kind == REDUCE_MAX) {
    fprintf(fp, "%s = fmax(%s, ", acc_name, acc_name);
    rmu_emit_term(src, fp);
    fputs(");\n", fp);
  } else {
    // SUM (default).
    fprintf(fp, "%s = %s + ", acc_name, acc_name);
    rmu_emit_term(src, fp);
    fputs(";\n", fp);
  }
}

// Emit init expression for a REDUCE kind: 0.0f for SUM, -INFINITY for MAX.
static void rmu_emit_reduce_init(u32 kind, FILE *fp) {
  if (kind == REDUCE_MAX) fputs("-INFINITY", fp);
  else                    fputs("0.0f", fp);
}

// Specialised simdgroup_matrix MSL template for the matmul pattern.
// Called when rmu_detect_matmul_tc fires.  Emits an 8x8
// simdgroup_matrix-tiled K-loop: load A and B subblocks, multiply-
// accumulate into C, store final C.  Falls back to the generic
// accumulator path when the address shapes don't yield clean
// ptr+offset (e.g. non-contiguous strides).
static int rmu_emit_matmul_tc(Term store, Term tc_red, FILE *fp,
                              u32 depth) {
  // Extract inner pieces validated by rmu_detect_matmul_tc.
  u64 sloc = term_val(store);
  Term addr_c = heap_read(sloc + 1);
  Term buf_c  = heap_read(sloc + 0);
  u64 rloc      = term_val(tc_red);
  u32 red_axis  = term_val(heap_read(rloc + 2));
  Term mul      = heap_read(rloc + 0);
  Term lhs      = heap_read(term_val(mul) + 0);
  Term rhs      = heap_read(term_val(mul) + 1);
  Term buf_a    = heap_read(term_val(lhs) + 0);
  Term addr_a   = heap_read(term_val(lhs) + 1);
  Term buf_b    = heap_read(term_val(rhs) + 0);
  Term addr_b   = heap_read(term_val(rhs) + 1);

  // Find the K-axis extent by scanning addr_a and addr_b for the
  // RANGE leaf with axis_id == red_axis.
  Term ranges[MAX_DIM];
  u32  n_r = 0;
  rmu_collect_ranges(addr_a, ranges, &n_r);
  rmu_collect_ranges(addr_b, ranges, &n_r);
  u32 k_extent = 0;
  for (u32 i = 0; i < n_r; i++) {
    if (term_val(heap_read(term_val(ranges[i]) + 0)) == red_axis) {
      k_extent = term_val(heap_read(term_val(ranges[i]) + 2));
      break;
    }
  }
  if (k_extent == 0 || (k_extent % 8) != 0) {
    // Tile size mismatch -- fall back to the generic accumulator path.
    return 0;
  }

  // Emit output-range loops.  Reuse the F1e structure but simplified
  // (no UNROLL pragmas yet for TC; F2c can layer those).
  Term out_ranges[MAX_DIM];
  u32  out_kinds[MAX_DIM]   = {0};
  u32  out_factors[MAX_DIM] = {0};
  u32  opt_kinds[MAX_DIM]   = {0};
  u32  opt_factors[MAX_DIM] = {0};
  Term combined[MAX_DIM];
  u32  n_combined = 0;
  rmu_collect_ranges_with_opts(addr_c, combined, opt_kinds, opt_factors, &n_combined);
  // Don't include red_src ranges in outer loops -- the K range is
  // consumed by the simdgroup loop.
  u32 n_out = 0;
  for (u32 i = 0; i < n_combined; i++) {
    u32 axis_id = term_val(heap_read(term_val(combined[i]) + 0));
    if (axis_id == red_axis) continue;
    out_ranges [n_out] = combined  [i];
    out_kinds  [n_out] = opt_kinds  [i];
    out_factors[n_out] = opt_factors[i];
    n_out++;
  }

  u32 body_depth = depth;
  int needs_close[MAX_DIM] = {0};
  for (u32 i = 0; i < n_out; i++) {
    Term r = out_ranges[i];
    u32 axis_type = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 1)) : 0;
    int threadbound = rmu_axis_is_threadbound(out_kinds[i], axis_type);
    rmu_emit_range_open(r, fp, body_depth, out_kinds[i]);
    if (!threadbound) { needs_close[i] = 1; body_depth++; }
  }

  // Emit the simdgroup template body.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("/* TC simdgroup_matrix matmul */\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("simdgroup_matrix<float, 8, 8> _a_mat;\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("simdgroup_matrix<float, 8, 8> _b_mat;\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("simdgroup_matrix<float, 8, 8> _c_mat = simdgroup_matrix<float, 8, 8>(0);\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
          red_axis, red_axis, k_extent, red_axis);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_load(_a_mat, &%s[", rmu_buf_name(buf_a));
  rmu_emit_term(addr_a, fp);
  fputs("]);\n", fp);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_load(_b_mat, &%s[", rmu_buf_name(buf_b));
  rmu_emit_term(addr_b, fp);
  fputs("]);\n", fp);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fputs("simdgroup_multiply_accumulate(_c_mat, _a_mat, _b_mat, _c_mat);\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("}\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_store(_c_mat, &%s[", rmu_buf_name(buf_c));
  rmu_emit_term(addr_c, fp);
  fputs("]);\n", fp);

  // Close output loops.
  for (i32 i = (i32)n_out - 1; i >= 0; i--) {
    if (!needs_close[i]) continue;
    body_depth--;
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  (void)out_factors;
  return 1;
}

// GROUP_REDUCE-shaped emission: parallel cooperative reduce using a
// threadgroup-shared accumulator + barrier + per-thread serial walk
// + final single-thread combine.  Fires when the reduce range was
// stamped with UOP_OPT_GROUP_REDUCE by the lifter (Phase 4 follow-on).
//
// Shape:
//   threadgroup float _accN[L];
//   _accN[tt] = init;
//   threadgroup_barrier(...);
//   for (uint k = tt; k < red_extent; k += L) _accN[tt] = combine(_accN[tt], body(k));
//   threadgroup_barrier(...);
//   if (tt == 0) {
//     float total = init;
//     for (uint i = 0; i < L; i++) total = combine(total, _accN[i]);
//     out[addr] = total;
//   }
//
// Caller passes the open output-axis loop nest in `n_out` /
// `needs_close[]` so this function can close them in the same order
// as the scalar path.
static int rmu_emit_group_reduce(Term buf, Term addr,
                                 Term red_range, Term red_src,
                                 u32 red_kind, u32 red_axis,
                                 u32 group_extent,
                                 FILE *fp, u32 body_depth,
                                 u32 n_out, int const *needs_close) {
  if (group_extent == 0) return 0;
  if (term_tag(red_range) != TAG_UOP || term_ext(red_range) != UOP_RANGE) {
    return 0;
  }
  u32 red_extent = term_val(heap_read(term_val(red_range) + 2));
  char acc_name[32];
  snprintf(acc_name, sizeof(acc_name), "_acc%u", red_axis);
  // Shared-mem accumulator declaration.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "threadgroup float %s[%u];\n", acc_name, group_extent);
  // Per-thread init.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s[tt] = ", acc_name);
  rmu_emit_reduce_init(red_kind, fp);
  fputs(";\n", fp);
  // Pre-loop barrier so every thread sees a clean slot.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("threadgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
  // Per-thread strided walk over the reduce extent.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "for (uint a%u = tt; a%u < %u; a%u += %u) {\n",
          red_axis, red_axis, red_extent, red_axis, group_extent);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "%s[tt] = ", acc_name);
  // Use the existing combine helper but supply the shared-slot lhs.
  // rmu_emit_reduce_combine writes "_accN = _accN OP rhs;" -- we want
  // "_accN[tt] = _accN[tt] OP rhs;" so render the rhs alone here.
  fprintf(fp, "%s[tt]", acc_name);
  if (red_kind == REDUCE_SUM)      fputs(" + ", fp);
  else if (red_kind == REDUCE_MAX) fputs(" > ", fp);   // placeholder
  else                              fputs(" + ", fp);
  rmu_emit_term(red_src, fp);
  if (red_kind == REDUCE_MAX) {
    // max via ternary so the line stays a single statement.
    fprintf(fp, " ? %s[tt] : ", acc_name);
    rmu_emit_term(red_src, fp);
  }
  fputs(";\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("}\n", fp);
  // Post-loop barrier.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("threadgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
  // Final combine + store on a single thread.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("if (tt == 0) {\n", fp);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fputs("float _total = ", fp);
  rmu_emit_reduce_init(red_kind, fp);
  fputs(";\n", fp);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "for (uint _i = 0; _i < %u; _i++) {\n", group_extent);
  for (u32 d = 0; d < body_depth + 2; d++) fputs("  ", fp);
  if (red_kind == REDUCE_SUM) {
    fprintf(fp, "_total = _total + %s[_i];\n", acc_name);
  } else {
    fprintf(fp, "_total = (_total > %s[_i]) ? _total : %s[_i];\n",
            acc_name, acc_name);
  }
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fputs("}\n", fp);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "%s[", rmu_buf_name(buf));
  rmu_emit_term(addr, fp);
  fputs("] = _total;\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("}\n", fp);
  // Close any open output-axis loops opened by the caller.
  for (i32 i = (i32)n_out - 1; i >= 0; i--) {
    if (!needs_close[i]) continue;
    body_depth--;
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  return 1;
}

// REDUCE-shaped emission: STORE(buf, addr, REDUCE(src, kind, axis)).
// Hoists an accumulator outside the reduce-axis loop and references it
// in the store statement.  Returns 1 if the shape matched and was
// emitted; 0 if the caller should fall back to the generic path.
//
// When the value is wrapped in OPT(REDUCE(...), TC, _) AND the shape
// matches the matmul pattern, dispatches to the F2b simdgroup_matrix
// template; falls back to the generic accumulator path when tile sizes
// don't fit (e.g. K extent not divisible by 8).
static int rmu_emit_store_reduce(Term store, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  u64 sloc = term_val(store);
  Term value = heap_read(sloc + 2);
  // F2b: dispatch to simdgroup_matrix template when matmul-shaped AND
  // tile dims fit; falls back to F1e accumulator otherwise.
  Term tc_red = 0;
  if (rmu_detect_matmul_tc(store, &tc_red)) {
    if (rmu_emit_matmul_tc(store, tc_red, fp, depth)) return 1;
    // Fall through to accumulator path.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("/* TC tile mismatch: falling back to scalar accumulator */\n", fp);
    value = tc_red;
  }
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  Term buf  = heap_read(sloc + 0);
  Term addr = heap_read(sloc + 1);
  u64 rloc      = term_val(value);
  Term red_src  = heap_read(rloc + 0);
  u32  red_kind = term_val(heap_read(rloc + 1));
  u32  red_axis = term_val(heap_read(rloc + 2));

  // Collect ranges from addr + red_src; split into output vs reduce.
  Term ranges[MAX_DIM];
  u32  opt_kinds[MAX_DIM]   = {0};
  u32  opt_factors[MAX_DIM] = {0};
  u32  n_ranges = 0;
  rmu_collect_ranges_with_opts(addr,    ranges, opt_kinds, opt_factors, &n_ranges);
  rmu_collect_ranges_with_opts(red_src, ranges, opt_kinds, opt_factors, &n_ranges);

  Term out_ranges[MAX_DIM];
  u32  out_kinds[MAX_DIM]   = {0};
  u32  out_factors[MAX_DIM] = {0};
  u32  n_out = 0;
  u32 reduce_idx = rmu_split_reduce(ranges, opt_kinds, opt_factors,
                                    n_ranges, red_axis,
                                    out_ranges, out_kinds, out_factors, &n_out);
  if (reduce_idx == n_ranges) {
    // No reduce range in the body -- nothing to accumulate over.
    return 0;
  }

  // Emit output ranges (outer loops).
  u32 body_depth = depth;
  int needs_close[MAX_DIM] = {0};
  for (u32 i = 0; i < n_out; i++) {
    Term r = out_ranges[i];
    u32 axis_type = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 1)) : 0;
    int threadbound = rmu_axis_is_threadbound(out_kinds[i], axis_type);
    if (out_kinds[i] != RMU_NO_OPT
        && (out_kinds[i] == UOP_OPT_UNROLL
            || out_kinds[i] == UOP_OPT_UPCAST)
        && !threadbound) {
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      if (out_factors[i] > 0) {
        fprintf(fp, "#pragma unroll(%u)\n", out_factors[i]);
      } else {
        fputs("#pragma unroll\n", fp);
      }
    }
    rmu_emit_range_open(r, fp, body_depth, out_kinds[i]);
    if (!threadbound) {
      needs_close[i] = 1;
      body_depth++;
    }
  }
  // GROUP_REDUCE detection has to fire BEFORE we declare the scalar
  // accumulator -- otherwise the rendered MSL would have both decls
  // and the threadgroup-shared `_acc[L]` collides with the prior
  // `float _acc;`.
  Term red_range = ranges[reduce_idx];
  u32  red_kind_opt   = opt_kinds  [reduce_idx];
  u32  red_factor_opt = opt_factors[reduce_idx];
  if (red_kind_opt != RMU_NO_OPT && red_kind_opt == UOP_OPT_GROUP_REDUCE) {
    return rmu_emit_group_reduce(buf, addr, red_range, red_src,
                                 red_kind, red_axis, red_factor_opt,
                                 fp, body_depth, n_out, needs_close);
  }
  // Emit accumulator decl using the reduce_axis as the unique id.
  char acc_name[32];
  snprintf(acc_name, sizeof(acc_name), "_acc%u", red_axis);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "float %s = ", acc_name);
  rmu_emit_reduce_init(red_kind, fp);
  fputs(";\n", fp);
  // Reduce-axis loop.
  if (red_kind_opt != RMU_NO_OPT && red_kind_opt == UOP_OPT_UNROLL) {
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    if (red_factor_opt > 0) fprintf(fp, "#pragma unroll(%u)\n", red_factor_opt);
    else                    fputs("#pragma unroll\n", fp);
  }
  rmu_emit_range_open(red_range, fp, body_depth, red_kind_opt);
  // Combine inside the reduce loop.
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  rmu_emit_reduce_combine(acc_name, red_kind, red_src, fp);
  // Close reduce loop.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("}\n", fp);
  // Final store.
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "%s[", rmu_buf_name(buf));
  rmu_emit_term(addr, fp);
  fprintf(fp, "] = %s;\n", acc_name);
  // Close output loops.
  for (i32 i = (i32)n_out - 1; i >= 0; i--) {
    if (!needs_close[i]) continue;
    body_depth--;
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
  return 1;
}

// Emit a single UOP_STORE statement, wrapping with for-loops over
// every UOP_RANGE that appears in the addr/value tree.  When a range
// was wrapped in UOP_OPT(_, UNROLL, factor), emit `#pragma unroll(N)`
// above the for-loop.  UPCAST/LOCAL/etc. handling lands in F1d+.
static void rmu_emit_store(Term store, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return;
  // Try the REDUCE-shape specialisation first.  Falls through to the
  // generic path when value isn't a UOP_REDUCE or shape doesn't match.
  if (rmu_emit_store_reduce(store, fp, depth)) return;
  u64 loc = term_val(store);
  Term buf   = heap_read(loc + 0);
  Term addr  = heap_read(loc + 1);
  Term value = heap_read(loc + 2);

  Term ranges[MAX_DIM];
  u32  opt_kinds[MAX_DIM]   = {0};
  u32  opt_factors[MAX_DIM] = {0};
  u32  n_ranges = 0;
  rmu_collect_ranges_with_opts(addr,  ranges, opt_kinds, opt_factors, &n_ranges);
  rmu_collect_ranges_with_opts(value, ranges, opt_kinds, opt_factors, &n_ranges);

  // Track the body indent and which ranges produced an open brace
  // (thread-bound axes don't, so we mustn't emit a matching close).
  u32 body_depth = depth;
  int needs_close[MAX_DIM] = {0};
  for (u32 i = 0; i < n_ranges; i++) {
    Term r = ranges[i];
    u32 axis_type = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 1)) : 0;
    int threadbound = rmu_axis_is_threadbound(opt_kinds[i], axis_type);
    if (opt_kinds[i] != RMU_NO_OPT
        && (opt_kinds[i] == UOP_OPT_UNROLL
            || opt_kinds[i] == UOP_OPT_UPCAST)
        && !threadbound) {
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      if (opt_factors[i] > 0) {
        fprintf(fp, "#pragma unroll(%u)\n", opt_factors[i]);
      } else {
        fputs("#pragma unroll\n", fp);
      }
    }
    rmu_emit_range_open(r, fp, body_depth, opt_kinds[i]);
    if (!threadbound) {
      needs_close[i] = 1;
      body_depth++;
    }
  }
  // Hoist any UOP_REDUCE nested inside the value expression: emit
  // accumulator init + reduce-axis loop + combine BEFORE the store
  // statement.  The term emitter substitutes _acc<axis> in the
  // expression itself.  Walks the value tree (not addr) since
  // reductions only ever appear in value position.
  Term reduces[MAX_DIM];
  u32  n_reduces = 0;
  rmu_collect_reduces(value, reduces, &n_reduces);
  for (u32 i = 0; i < n_reduces; i++) {
    Term red = reduces[i];
    u64 rloc = term_val(red);
    u32 r_kind = term_val(heap_read(rloc + 1));
    u32 r_axis = term_val(heap_read(rloc + 2));
    Term r_src = heap_read(rloc + 0);
    char acc_name[32];
    snprintf(acc_name, sizeof(acc_name), "_acc%u", r_axis);
    // Find the reduce-axis range leaf in the body and emit a loop.
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fprintf(fp, "float %s = ", acc_name);
    rmu_emit_reduce_init(r_kind, fp);
    fputs(";\n", fp);
    // Reduce loop opener: find the matching RANGE and emit its for.
    Term reduce_ranges[MAX_DIM];
    u32  reduce_kinds[MAX_DIM] = {0};
    u32  reduce_factors[MAX_DIM] = {0};
    u32  n_red_ranges = 0;
    rmu_collect_ranges_with_opts(r_src, reduce_ranges, reduce_kinds,
                                 reduce_factors, &n_red_ranges);
    Term reduce_range_term = 0;
    for (u32 j = 0; j < n_red_ranges; j++) {
      u32 axis = term_val(heap_read(term_val(reduce_ranges[j]) + 0));
      if (axis == r_axis) { reduce_range_term = reduce_ranges[j]; break; }
    }
    if (reduce_range_term != 0) {
      rmu_emit_range_open(reduce_range_term, fp, body_depth, 0);
      for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
      rmu_emit_reduce_combine(acc_name, r_kind, r_src, fp);
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
  }
  for (u32 i = 0; i < body_depth; i++) fputs("  ", fp);
  fprintf(fp, "%s[", rmu_buf_name(buf));
  rmu_emit_term(addr, fp);
  fputs("] = ", fp);
  rmu_emit_term(value, fp);
  fputs(";\n", fp);
  // Close braces (innermost first), only for ranges that opened one.
  for (i32 i = (i32)n_ranges - 1; i >= 0; i--) {
    if (!needs_close[i]) continue;
    body_depth--;
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
}

// Walk an AFTER chain bottom-up, emitting each store followed by a
// barrier when the next happens-after pair crosses a scope boundary.
// In F0 we just emit the leading store; the chain semantics get fully
// wired when the renderer flips its primary path.
static void rmu_emit_after(Term after, FILE *fp, u32 depth) {
  if (term_tag(after) != TAG_UOP || term_ext(after) != UOP_AFTER) return;
  u64 loc = term_val(after);
  Term node       = heap_read(loc + 0);
  Term after_node = heap_read(loc + 1);
  // Emit the prior side-effect first, then the barrier, then `node`.
  if (term_ext(after_node) == UOP_STORE) rmu_emit_store(after_node, fp, depth);
  if (term_ext(after_node) == UOP_AFTER) rmu_emit_after(after_node, fp, depth);
  // Cross-scope check: emit barrier when storing into LOCAL and the
  // next store reads from / writes to a different scope.
  Term prev_buf = (term_ext(after_node) == UOP_STORE)
                  ? heap_read(term_val(after_node) + 0) : 0;
  Term curr_buf = (term_ext(node) == UOP_STORE)
                  ? heap_read(term_val(node) + 0) : 0;
  u32 prev_scope = uop_buffer_scope(prev_buf);
  u32 curr_scope = uop_buffer_scope(curr_buf);
  if (prev_scope == UOP_SCOPE_LOCAL || curr_scope == UOP_SCOPE_LOCAL) {
    for (u32 i = 0; i < depth; i++) fputs("  ", fp);
    fputs("threadgroup_barrier(mem_flags::mem_threadgroup);\n", fp);
  }
  if (term_ext(node) == UOP_STORE) rmu_emit_store(node, fp, depth);
  if (term_ext(node) == UOP_AFTER) rmu_emit_after(node, fp, depth);
}

// Render a kernel rooted at `root`.  The root is typically a
// UOP_STORE (single-store kernel) or UOP_AFTER chain (multi-store
// kernel).  `kernel_name` and a list of input buffers + the output
// buffer drive the kernel signature.
fn void cg_render_uop_kernel(Term root, const char *kernel_name,
                             Term out_buf, Term const *in_bufs,
                             u32 n_inputs, FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  // Populate buffer-name map: out_buf -> "out", in_bufs[i] -> "inN".
  rmu_buf_names_reset();
  rmu_buf_names_set(out_buf, "out");
  for (u32 i = 0; i < n_inputs; i++) {
    char name[16];
    snprintf(name, sizeof(name), "in%u", i);
    rmu_buf_names_set(in_bufs[i], name);
  }
  fputs("#include <metal_stdlib>\n", fp);
  fputs("using namespace metal;\n\n", fp);
  fprintf(fp, "kernel void %s(\n", kernel_name);
  // Output goes to buffer(0); each input goes to buffer(1+i).
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "    device %s *out [[ buffer(0) ]]",
          rmu_msl_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    u32 dt = uop_buffer_dtype(in_bufs[i]);
    fprintf(fp, ",\n    device const %s *in%u [[ buffer(%u) ]]",
            rmu_msl_type_name(dt), i, i + 1);
  }
  fputs(",\n    uint tid [[ thread_position_in_grid ]],\n", fp);
  fputs("    uint tg [[ threadgroup_position_in_grid ]],\n", fp);
  fputs("    uint tt [[ thread_position_in_threadgroup ]]) {\n", fp);
  // Body.  In F0 we just dispatch on the root op and emit the
  // contained store (or AFTER chain).  Range-loop wrapping happens
  // when the root is wrapped in a RANGE chain (future work).
  if (root != 0 && term_tag(root) == TAG_UOP) {
    u32 op = term_ext(root);
    if (op == UOP_STORE)      rmu_emit_store(root, fp, 1);
    else if (op == UOP_AFTER) rmu_emit_after(root, fp, 1);
    else {
      fputs("  /* unsupported root op */\n", fp);
    }
  } else {
    fputs("  /* empty kernel */\n", fp);
  }
  fputs("}\n", fp);
}
