// codegen/render_uop.c - UOp DAG renderer skeleton (Phase F0).
//
// Walks the UOp DAG rooted at a kernel-output store and emits
// pseudo-MSL.  Replaces the in-tree TileUop[] skeleton renderer
// (deleted in Phase G).  This is the seed Phase F's renderer rewrite
// proper grows into; today it covers a small set of UOp shapes:
//
//   UOP_BUFFER         -- kernel arg or local alloc
//   UOP_INDEX_E        -- buf[addr]
//   UOP_STORE          -- buf[addr] = value;
//   UOP_AFTER          -- threadgroup_barrier when cross-scope
//   UOP_RANGE          -- for-loop or thread-position bind
//   UOP_OPT            -- annotation on target
//   UOP_CONST/ICONST   -- literal value
//   UOP_IADD/IMUL/etc. -- symbolic int expressions
//
// Future extensions: UOP_REDUCE (init/accum/finalize), full UOP_*
// elementwise (S_LOAD / S_ADD / S_MUL / etc. now lifted to UOps),
// UOP_OPT pattern-match for GEMM / conv2d templates.
//
// No consumer wires this in yet -- it's a structural seam tested in
// isolation.  When the renderer rewrite proper lands, render_metal's
// CtKernelInfo path swaps to call into this walker.

static void rmu_emit_term(Term t, FILE *fp);

static const char *rmu_msl_type_name(u32 dtype) {
  switch (dtype) {
    case DT_FP32:  return "float";
    case DT_FP16:  return "half";
    case DT_INT32: return "int";
    case DT_INT64: return "long";
    case DT_UINT8: return "uchar";
    default:       return "?";
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
      fprintf(fp, "buf%llu[", (unsigned long long)term_val(buf));
      rmu_emit_term(addr, fp);
      fputs("]", fp);
      return;
    }
    case UOP_BUFFER:
      // A bare BUFFER reference -- the renderer normally goes
      // through INDEX_E, but emit the name for diagnostic dumps.
      fprintf(fp, "buf%llu", (unsigned long long)loc);
      return;
    case UOP_OPT: {
      // Annotation: render the target, ignore the directive in F0
      // (F1+ pattern-matches for specialised templates).
      rmu_emit_term(heap_read(loc + 0), fp);
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
// `opt_kinds[]` / `opt_factors[]` so emit_range_open can fire the
// matching pragma (UNROLL/UPCAST).  No-OPT ranges record kind=0 /
// factor=0 (UNROLL with factor=0 is a no-op).
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
                             opt_factors, n_out, 0, 0);
      rmu_collect_ranges_rec(heap_read(loc + 1), ranges, opt_kinds,
                             opt_factors, n_out, 0, 0);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
      rmu_collect_ranges_rec(heap_read(loc + 0), ranges, opt_kinds,
                             opt_factors, n_out, 0, 0);
      return;
    case UOP_IWHERE:
      rmu_collect_ranges_rec(heap_read(loc + 0), ranges, opt_kinds,
                             opt_factors, n_out, 0, 0);
      rmu_collect_ranges_rec(heap_read(loc + 1), ranges, opt_kinds,
                             opt_factors, n_out, 0, 0);
      rmu_collect_ranges_rec(heap_read(loc + 2), ranges, opt_kinds,
                             opt_factors, n_out, 0, 0);
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
                             opt_factors, n_out, 0, 0);
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
                         0, 0);
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

// REDUCE-shaped emission: STORE(buf, addr, REDUCE(src, kind, axis)).
// Hoists an accumulator outside the reduce-axis loop and references it
// in the store statement.  Returns 1 if the shape matched and was
// emitted; 0 if the caller should fall back to the generic path.
static int rmu_emit_store_reduce(Term store, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  u64 sloc = term_val(store);
  Term value = heap_read(sloc + 2);
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
    if (out_kinds[i] == UOP_OPT_UNROLL || out_kinds[i] == UOP_OPT_UPCAST) {
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
  // Emit accumulator decl using the reduce_axis as the unique id.
  char acc_name[32];
  snprintf(acc_name, sizeof(acc_name), "_acc%u", red_axis);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "float %s = ", acc_name);
  rmu_emit_reduce_init(red_kind, fp);
  fputs(";\n", fp);
  // Reduce-axis loop.
  Term red_range = ranges[reduce_idx];
  u32  red_kind_opt   = opt_kinds  [reduce_idx];
  u32  red_factor_opt = opt_factors[reduce_idx];
  if (red_kind_opt == UOP_OPT_UNROLL) {
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
  fprintf(fp, "buf%llu[", (unsigned long long)term_val(buf));
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
    if (opt_kinds[i] == UOP_OPT_UNROLL || opt_kinds[i] == UOP_OPT_UPCAST) {
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
  for (u32 i = 0; i < body_depth; i++) fputs("  ", fp);
  fprintf(fp, "buf%llu[", (unsigned long long)term_val(buf));
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
  fputs("#include <metal_stdlib>\n", fp);
  fputs("using namespace metal;\n\n", fp);
  fprintf(fp, "kernel void %s(\n", kernel_name);
  // Output goes to buffer(0); each input goes to buffer(1+i).
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "    device %s *buf%llu [[ buffer(0) ]]",
          rmu_msl_type_name(out_dtype),
          (unsigned long long)term_val(out_buf));
  for (u32 i = 0; i < n_inputs; i++) {
    u32 dt = uop_buffer_dtype(in_bufs[i]);
    fprintf(fp, ",\n    device const %s *buf%llu [[ buffer(%u) ]]",
            rmu_msl_type_name(dt),
            (unsigned long long)term_val(in_bufs[i]), i + 1);
  }
  fputs(",\n    uint tid [[ thread_position_in_grid ]]) {\n", fp);
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
