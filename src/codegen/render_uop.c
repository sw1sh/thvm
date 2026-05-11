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

// Renderer target. 0 = MSL (default), 1 = C99 for CPU JIT (F6).
// When C99, axis-type LOCAL/GLOBAL is rendered as a regular for-loop
// (no thread-position bind), and the prologue/epilogue switch to a
// plain function signature without Metal kernel attributes.
static int RMU_TARGET_C = 0;

// Reduce-axis loop unroll threshold.  When a scalar accumulator's
// reduce axis has extent <= this and the target is MSL (not C99), the
// renderer emits `#pragma unroll(<extent>)` immediately above the
// `for`-loop so the MSL compiler can straight-line the K MAD ops --
// matmul's K=25 contraction, conv's K=9/27, etc.  Larger reduces stay
// on the rolled loop to keep the generated body size sane.  (Formerly
// RMU_CONV_UNROLL_MAX -- the conv2d-flat template uses the same gate.)
#define RMU_REDUCE_UNROLL_MAX 64u

// Buffer name resolution.
//
// Phase C slice 3 (structural slot indexing): production callers
// resolve buffer names through the UOP_BUFFER `instance` field
// (kernel_lift.c sets instance=0 on the output and instance=slot+1 on
// the i-th input).  rmu_buf_name(t) decodes instance directly:
//
//   instance == 0  -> "out" (resolved via the legacy map below)
//   instance >= 1  -> "in<instance-1>"
//
// This drops the renderer's prior dependency on Term-identity matches
// against an `in_bufs[]` array passed in from the caller for input
// naming; lift result and renderer agree on input naming via stable
// structural indices.  Slot 3 -> "in2" regardless of what Term the
// lifter happened to hash-cons for that slot in this session.
//
// The legacy Term-identity map (populated by rmu_buf_names_set) is
// retained for two reasons:
//   1. Synthetic test kernels (tests/test_render_uop.c) build
//      UOP_BUFFER leaves via uop_buffer(...) which leaves instance==0
//      on every leaf; the test entry point cg_render_uop_kernel(...)
//      registers each one explicitly so the structural fallback still
//      lands on "out" / "inN".
//   2. The output buffer's instance is 0 for both lifted and
//      synthetic kernels; the map disambiguates by Term identity (one
//      output per kernel makes this unambiguous in practice).
//
// Static globals are fine; the renderer isn't re-entrant in practice
// and the map is cleared at the start of each render.
#define RMU_BUF_MAX 32
static struct { Term term; char name[16]; } RMU_BUF_NAMES[RMU_BUF_MAX];
static u32 RMU_BUF_NAMES_N;

static void rmu_buf_names_reset(void) {
  RMU_BUF_NAMES_N = 0;
}

// Register a Term -> name mapping in the legacy fallback map.  Used
// by the cg_render_uop_kernel(out_buf, in_bufs[]) entry points to
// keep test-built kernels (instance==0 everywhere) renderable, and to
// register the output's "out" name (whose lifter-assigned instance is
// 0 and so doesn't carry a structural slot).
static void rmu_buf_names_set(Term t, const char *name) {
  if (RMU_BUF_NAMES_N >= RMU_BUF_MAX) return;
  RMU_BUF_NAMES[RMU_BUF_NAMES_N].term = t;
  snprintf(RMU_BUF_NAMES[RMU_BUF_NAMES_N].name,
           sizeof(RMU_BUF_NAMES[0].name), "%s", name);
  RMU_BUF_NAMES_N++;
}

// Returns the symbolic name for buffer `t`.
//
// Resolution order:
//   1. UOP_BUFFER.instance >= 1 -> "in<instance-1>" (structural).
//   2. Legacy Term-identity map (the output's "out" entry, plus all
//      synthetic test kernels).
//   3. `buf<loc>` fallback (defensive).
static const char *rmu_buf_name(Term t) {
  // Structural path: instance>=1 means "input slot (instance-1)".
  // Lifted kernels use this for every input; the output (instance==0)
  // falls through to the legacy map.
  u32 inst = uop_buffer_inst_get(t);
  if (inst >= 1) {
    static char structural[16];
    snprintf(structural, sizeof(structural), "in%u", inst - 1);
    return structural;
  }
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

// F6: C99 lacks `half` and `uchar`; emit equivalents that math.h /
// stdint.h cover. fp16 falls back to `float` for now -- caller (cpu/jit.c)
// will need to widen DT_FP16 inputs at the host boundary if they're
// ever routed through this path.
static const char *rmu_c_type_name(u32 dtype) {
  switch (dtype) {
    case DT_FP32:  return "float";
    case DT_FP16:  return "float";   // see comment above
    case DT_INT32: return "int";
    case DT_INT64: return "long";
    case DT_UINT8: return "unsigned char";
    default:       return "float";
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
        // %.9g round-trips fp32 exactly with the shortest decimal
        // representation -- but for whole numbers it produces "2"
        // which becomes "2f" (invalid C/MSL float literal -- needs
        // a decimal point or exponent).  Detect that case and add
        // ".0" so the literal stays valid.  Replaces the previous
        // %f .6 default which truncated tiny constants like 1e-7
        // to "0.000000f", silently zeroing the addend in e.g.
        // cross-entropy eps clamps.
        char numbuf[32];
        snprintf(numbuf, sizeof(numbuf), "%.9g", (double)pun.f);
        int has_decimal = (strchr(numbuf, '.') != NULL
                           || strchr(numbuf, 'e') != NULL
                           || strchr(numbuf, 'E') != NULL
                           || strchr(numbuf, 'n') != NULL  /* nan/inf */
                           || strchr(numbuf, 'i') != NULL);
        if (has_decimal) {
          fprintf(fp, "%sf", numbuf);
        } else {
          fprintf(fp, "%s.0f", numbuf);
        }
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
      const char *type_name = RMU_TARGET_C ? rmu_c_type_name(dst_dt)
                                           : rmu_msl_type_name(dst_dt);
      if (op == UOP_BITCAST) {
        if (RMU_TARGET_C) {
          // C99 bitcast via the THVM_BITCAST statement-expression
          // macro emitted in the C-target prologue.  Pattern:
          //   THVM_BITCAST(<dst>, <expr>)
          //  -> ({ <dst> _tmp; memcpy(&_tmp, &(_src), sizeof(_tmp)); _tmp; })
          // Statement-expressions are a GCC/clang extension; both
          // compilers we target accept them.
          fprintf(fp, "THVM_BITCAST(%s, ", type_name);
          rmu_emit_term(src, fp);
          fputs(")", fp);
        } else {
          fprintf(fp, "as_type<%s>(", type_name);
          rmu_emit_term(src, fp);
          fputs(")", fp);
        }
      } else {
        fprintf(fp, "((%s)", type_name);
        rmu_emit_term(src, fp);
        fputs(")", fp);
      }
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
      // FAST_MATH peels: when wrapping a unary EXP2/LOG2/SQRT, emit
      // the Apple `fast::*` intrinsic instead of the precise variant.
      // Apple's `fast::` namespace skips edge-case handling (denorms /
      // NaNs / OOB inputs) for ~5-15% throughput on softmax / layernorm
      // / attention where the result is renormalised anyway.  See
      // mlx/backend/metal/kernels/softmax.h for the reference pattern.
      Term inner = heap_read(loc + 0);
      u32 opt_kind = (u32)term_val(heap_read(loc + 1));
      if (opt_kind == UOP_OPT_FAST_MATH && !RMU_TARGET_C
          && term_tag(inner) == TAG_UOP) {
        u32 inner_op = term_ext(inner);
        if (inner_op == UOP_EXP2 || inner_op == UOP_LOG2
            || inner_op == UOP_SQRT) {
          const char *fn_name = (inner_op == UOP_EXP2) ? "fast::exp2"
                              : (inner_op == UOP_LOG2) ? "fast::log2"
                              :                          "fast::sqrt";
          fprintf(fp, "%s(", fn_name);
          rmu_emit_term(heap_read(term_val(inner) + 0), fp);
          fputs(")", fp);
          return;
        }
      }
      // VEC_LOAD peel: wrap UOP_INDEX_E with a floatN reinterpret_cast
      // at the load site.  Pattern (correctness-preserving slice):
      //   ((device const floatN*)(buf))[(addr) / N][(addr) % N]
      // Semantically identical to buf[addr]; lets Metal coalesce N
      // consecutive scalar loads into one vector load when the address
      // is contiguous.  See docs/plans/mlx_features_to_port.md (4) +
      // mlx/backend/metal/kernels/softmax.h.  factor = 2/4/8/16.
      if (opt_kind == UOP_OPT_VEC_LOAD && !RMU_TARGET_C
          && term_tag(inner) == TAG_UOP && term_ext(inner) == UOP_INDEX_E) {
        u32 width = (u32)term_val(heap_read(loc + 2));
        Term buf  = heap_read(term_val(inner) + 0);
        Term addr = heap_read(term_val(inner) + 1);
        u32 dt    = uop_buffer_dtype(buf);
        const char *base = rmu_msl_type_name(dt);
        if ((dt == DT_FP32 || dt == DT_FP16)
            && (width == 2 || width == 4 || width == 8 || width == 16)) {
          fprintf(fp, "((device const %s%u*)(%s))[(",
                  base, width, rmu_buf_name(buf));
          rmu_emit_term(addr, fp);
          fprintf(fp, ") / %u][(", width);
          rmu_emit_term(addr, fp);
          fprintf(fp, ") %% %u]", width);
          return;
        }
      }
      rmu_emit_term(inner, fp);
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

// Walks the term subgraph rooted at `t` (descending through every
// operand slot we care about) and returns 1 iff `needle` appears as
// a node anywhere inside.  Used to detect reduce nesting (the
// nested-reduce / body-rewrap shape) when computing `required_pos`
// in the generic store path.  Bounded recursion by a `depth` budget
// so a malformed cyclic DAG can't drive an infinite descent.
static int rmu_term_contains_rec(Term t, Term needle, u32 depth) {
  if (depth > 256) return 0;
  if (t == needle) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return rmu_term_contains_rec(heap_read(loc + 0), needle, depth + 1)
          || rmu_term_contains_rec(heap_read(loc + 1), needle, depth + 1);
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      return rmu_term_contains_rec(heap_read(loc + 0), needle, depth + 1);
    case UOP_IWHERE:
      return rmu_term_contains_rec(heap_read(loc + 0), needle, depth + 1)
          || rmu_term_contains_rec(heap_read(loc + 1), needle, depth + 1)
          || rmu_term_contains_rec(heap_read(loc + 2), needle, depth + 1);
    case UOP_REDUCE:
      // Recurse into the body: nesting can be transitive.
      return rmu_term_contains_rec(heap_read(loc + 0), needle, depth + 1);
    default:
      return 0;
  }
}
static int rmu_term_contains(Term t, Term needle) {
  return rmu_term_contains_rec(t, needle, 0);
}

// Returns 1 if `t`'s subtree contains a UOP_REDUCE node anywhere.
// Used by rmu_emit_store_reduce to decline the single-reduce
// specialisation when the reduce body itself contains a nested
// reduce -- those need the generic rmu_emit_store path, which
// post-order-collects every reduce and hoists each accumulator's
// declaration ahead of its consumers (rmu_emit_store_reduce only
// emits the outer accumulator, leaving the inner `_accN`
// undeclared -> MSL "use of undeclared identifier" -> per-op
// fallback for that kernel).
static int rmu_term_has_reduce(Term t, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) return 1;
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return rmu_term_has_reduce(heap_read(loc + 0), depth + 1)
          || rmu_term_has_reduce(heap_read(loc + 1), depth + 1);
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      return rmu_term_has_reduce(heap_read(loc + 0), depth + 1);
    case UOP_IWHERE:
      return rmu_term_has_reduce(heap_read(loc + 0), depth + 1)
          || rmu_term_has_reduce(heap_read(loc + 1), depth + 1)
          || rmu_term_has_reduce(heap_read(loc + 2), depth + 1);
    default:
      return 0;
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
    // Post-order add: recurse into the body FIRST so any nested
    // REDUCE (e.g. body-rewrap form `IWHERE(_, INNER_REDUCE, INVALID)`
    // from kernel_lift's reduce-over-broadcast-axis fix) is collected
    // and emitted before this outer reduce that depends on its
    // `_acc<N>` placeholder.  Otherwise the renderer emits `_acc4`
    // for the inner reduce term while only `_acc5` (the outer) is
    // declared, yielding `undeclared identifier '_acc4'`.
    rmu_collect_reduces(heap_read(loc + 0), reduces, n_out);
    if (*n_out >= MAX_DIM) return;
    // Re-check dedup after recursion (in case the body referenced
    // this same outer term -- shouldn't happen but cheap to guard).
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) return;
    }
    reduces[*n_out] = t;
    (*n_out)++;
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

// Variant of rmu_collect_reduces that ALSO tags each collected REDUCE
// with whether its immediate parent is OPT(_, SIMD_REDUCE, _).  Used by
// rmu_emit_store to fire the simd_sum/simd_max collective-reduce
// emission instead of a scalar for-loop accumulator.  `simd_flags[i]`
// is set when reduces[i] was reached through an OPT_SIMD_REDUCE wrap.
static void rmu_collect_reduces_with_simd(Term t, int parent_is_simd,
                                          Term *reduces, u8 *simd_flags,
                                          u32 *n_out) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= MAX_DIM) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) {
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) {
        if (parent_is_simd) simd_flags[i] = 1;
        return;
      }
    }
    // Post-order add: recurse into the body FIRST.  See the long
    // comment in rmu_collect_reduces; same nested-REDUCE invariant
    // applies (inner `_acc<N>` must be declared before outer body
    // emits the reference).  Pass parent_is_simd=0 through the body
    // -- SIMD_REDUCE only applies to the immediately-wrapped reduce,
    // not to siblings nested deeper.
    rmu_collect_reduces_with_simd(heap_read(loc + 0), 0, reduces,
                                  simd_flags, n_out);
    if (*n_out >= MAX_DIM) return;
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) {
        if (parent_is_simd) simd_flags[i] = 1;
        return;
      }
    }
    reduces[*n_out]    = t;
    simd_flags[*n_out] = parent_is_simd ? 1 : 0;
    (*n_out)++;
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_collect_reduces_with_simd(heap_read(loc + 0), 0, reduces,
                                    simd_flags, n_out);
      rmu_collect_reduces_with_simd(heap_read(loc + 1), 0, reduces,
                                    simd_flags, n_out);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
      rmu_collect_reduces_with_simd(heap_read(loc + 0), 0, reduces,
                                    simd_flags, n_out);
      return;
    case UOP_OPT: {
      u32 kind = term_val(heap_read(loc + 1));
      int is_simd = (kind == UOP_OPT_SIMD_REDUCE) ? 1 : parent_is_simd;
      rmu_collect_reduces_with_simd(heap_read(loc + 0), is_simd, reduces,
                                    simd_flags, n_out);
      return;
    }
    case UOP_IWHERE:
      rmu_collect_reduces_with_simd(heap_read(loc + 0), 0, reduces,
                                    simd_flags, n_out);
      rmu_collect_reduces_with_simd(heap_read(loc + 1), 0, reduces,
                                    simd_flags, n_out);
      rmu_collect_reduces_with_simd(heap_read(loc + 2), 0, reduces,
                                    simd_flags, n_out);
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
  if (RMU_TARGET_C) return 0;  // F6: C99 target has no thread positions
  return (opt_kind == UOP_OPT_LOCAL)
      || axis_type == 4  /* legacy KAX_LOCAL  */
      || axis_type == 5  /* legacy KAX_GLOBAL */;
}

// Per-call promoted-GLOBAL context.  Output axes (axis_id in the
// store's addr expression) that arrive as plain KAX_LOOP with no OPT
// wrap get promoted to parallel grid axes: instead of a serial
// `for`-loop, each thread decodes its axis tuple from the flat 1-D
// dispatch index `tid`.  rmu_emit_store builds this once pre-loop and
// threads it through to rmu_emit_range_open_ctx.
//
//   modulus_of_axis[axis_id] = this axis's extent (0 if not promoted)
//   stride_of_axis [axis_id] = product of inner promoted-GLOBAL
//                              extents (1 for the innermost)
//   n_globals                = count of promoted axes
//   total                    = product of all promoted extents
//                              (== output_numel; the dispatcher
//                              launches >= this many threads and the
//                              kernel guards `tid >= total`)
//
// Note: explicitly-pre-stamped axis_type==KAX_GLOBAL ranges (the TC
// matmul / conv2d_flat templates bind those themselves, and the one
// `render-uop/legacy-kax-global-emits-tg` test) are NOT entered here
// -- those still emit `uint aN = tg;` via rmu_emit_range_open_ctx's
// axis_type==5 branch.
//
// tg/tt split: when the kernel ALSO carries one or more LOCAL axes (a
// KOP_LOCAL split: OPT(_, LOCAL, f) wrapping the INNER half, the OUTER
// half a plain promoted-GLOBAL axis), the promoted-GLOBAL axes must
// decode from `tg` (threadgroup_position_in_grid), NOT `tid`
// (thread_position_in_grid) -- otherwise the GLOBAL decode would
// include the `tt` bits (`tid = tg*threads + tt`) and repeat values
// within each threadgroup.  tinygrad's `has_local && has_global`
// convention: GLOBAL extents -> grid (`groups`, indexed by tg), LOCAL
// extents -> threadgroup (`threads`, indexed by tt).  `has_local` is
// set by rmu_compute_global_decode_ctx when it sees a LOCAL-OPT'd (or
// legacy axis_type==4) range; with no LOCAL axis it's 0 and the
// existing `tid` decode (equivalent to tg*threads+tt with threads
// derived from the dispatch shape) is kept unchanged.
//
// Multi-LOCAL: with >=2 LOCAL axes the threadgroup index `tt` decodes
// each one with its own (stride, modulus): `uint lK = (tt / ltK) % leK`
// where ltK is the product of LOCAL extents inner to lK (1 for the
// innermost).  Mirrors the multi-GLOBAL `tg` decode.  The single-LOCAL
// fast path (`uint l0 = tt;`) is just the n==1 case (stride 1).  The
// threadgroup size is the product of all LOCAL extents; the heuristic
// keeps it <= 256 (Apple's maxTotalThreadsPerThreadgroup is 1024).
typedef struct {
  u32 stride_of_axis [256];   // GLOBAL axes: stride into the `tg` flat decode
  u32 modulus_of_axis[256];   // GLOBAL axes: extent (0 if not promoted)
  u32 n_globals;
  u64 total;                  // product of promoted-GLOBAL extents
  int has_local;              // 1 -> decode GLOBAL axes from `tg`, not `tid`
  // LOCAL-axis decode (mirrors the GLOBAL arrays but over `tt`).
  u32 local_stride_of_axis [256];
  u32 local_modulus_of_axis[256];
  u32 n_locals;
  u64 local_total;            // product of LOCAL extents (threadgroup size)
} RmuGlobalDecode;

// Emit a loop opener (or thread-position bind) for a UOP_RANGE leaf.
// `opt_kind` is the OPT annotation (RMU_NO_OPT if none).
// `gctx` is the promoted-GLOBAL decode context (NULL for legacy callers).
// Patterns:
//   LOCAL                       -> `uint aN = tt;`
//   promoted GLOBAL, n==1       -> `uint aN = tid;`
//   promoted GLOBAL, n>=2       -> `uint aN = (tid/stride) % mod;`  (`tid % mod` for innermost)
//   axis_type==5 (legacy)       -> `uint aN = tg;`
//   axis_type==1 (REDUCE)       -> `for (...) /*reduce*/ {`
//   else                        -> `for (...) {`
//
// kvar wedge: when the range's raw extent has bit 31 (KVAR_FLAG) set,
// the low 31 bits hold a kvar id instead of a literal extent.  In
// that case the for-loop bound is emitted as `V_<name>` so a single
// MSL string covers all runtime values for that variable; the kernel
// signature emit in cg_render_uop_kernel_root adds the matching
// `constant uint &V_<name>` arg and the Metal dispatcher binds the
// per-fire runtime values via setBytes:.  Promoted-GLOBAL / LOCAL /
// legacy-GLOBAL paths use the worst-case kvar_hi(id) for the `ext=%u`
// comment; the parent thread owns wiring symbolic extents through
// those paths (the symbolic demo test only exercises the LOOP bound).
static void rmu_emit_range_open_ctx(Term r, FILE *fp, u32 depth,
                                    u32 opt_kind,
                                    RmuGlobalDecode const *gctx) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return;
  u64 loc = term_val(r);
  u32 axis_id    = term_val(heap_read(loc + 0));
  u32 axis_type  = term_val(heap_read(loc + 1));
  u32 raw_extent = term_val(heap_read(loc + 2));
  int is_var = kvar_extent_is_var(raw_extent);
  u32 var_id = is_var ? kvar_extent_var_id(raw_extent) : 0;
  // `extent` is the integer to bake into the literal /* ext=%u */
  // comment (and as the for-loop bound for non-symbolic ranges).
  // For symbolic ranges the actual loop bound is `bound[]` below.
  u32 extent = is_var ? kvar_hi(var_id) : raw_extent;
  char bound[32];
  if (is_var) {
    const char *vn = kvar_name(var_id);
    if (vn == NULL) vn = "V";
    snprintf(bound, sizeof(bound), "V_%s", vn);
  } else {
    snprintf(bound, sizeof(bound), "%u", raw_extent);
  }
  for (u32 i = 0; i < depth; i++) fputs("  ", fp);
  // LOCAL via OPT annotation OR via axis_type == 4 (legacy KAX_LOCAL).
  // With a single LOCAL axis: `uint aN = tt;`.  With >=2, decode each
  // from `tt` with its own (stride, modulus) -- mirrors multi-GLOBAL.
  if (opt_kind == UOP_OPT_LOCAL || axis_type == 4) {
    if (gctx != NULL && axis_id < 256
        && gctx->local_modulus_of_axis[axis_id] != 0 && gctx->n_locals >= 2) {
      u32 lstride = gctx->local_stride_of_axis[axis_id];
      u32 lmod    = gctx->local_modulus_of_axis[axis_id];
      if (lstride <= 1) {
        fprintf(fp, "uint a%u = tt %% %uu; /* local ext=%u */\n",
                axis_id, lmod, extent);
      } else {
        fprintf(fp, "uint a%u = (tt / %uu) %% %uu; /* local ext=%u */\n",
                axis_id, lstride, lmod, extent);
      }
      return;
    }
    fprintf(fp, "uint a%u = tt; /* local ext=%u */\n", axis_id, extent);
    return;
  }
  // Promoted output axis -> parallel grid axis.  Decoded from `tid`
  // (thread_position_in_grid) normally; from `tg`
  // (threadgroup_position_in_grid) when the kernel also has a LOCAL
  // axis (one threadgroup per GLOBAL tuple, LOCAL axis over `tt`).
  if (gctx != NULL && axis_id < 256 && gctx->modulus_of_axis[axis_id] != 0) {
    u32 stride = gctx->stride_of_axis[axis_id];
    u32 mod    = gctx->modulus_of_axis[axis_id];
    char const *idx = gctx->has_local ? "tg" : "tid";
    if (gctx->n_globals == 1) {
      fprintf(fp, "uint a%u = %s; /* global ext=%u */\n", axis_id, idx, extent);
    } else if (stride <= 1) {
      fprintf(fp, "uint a%u = %s %% %uu; /* global ext=%u */\n",
              axis_id, idx, mod, extent);
    } else {
      fprintf(fp, "uint a%u = (%s / %uu) %% %uu; /* global ext=%u */\n",
              axis_id, idx, stride, mod, extent);
    }
    return;
  }
  if (axis_type == 5 /* legacy KAX_GLOBAL (TC/conv templates, test) */) {
    fprintf(fp, "uint a%u = tg; /* global ext=%u */\n", axis_id, extent);
    return;
  }
  if (axis_type == 1 /*REDUCE*/) {
    fprintf(fp, "for (uint a%u = 0; a%u < %s; a%u++) /*reduce*/ {\n",
            axis_id, axis_id, bound, axis_id);
  } else {
    fprintf(fp, "for (uint a%u = 0; a%u < %s; a%u++) {\n",
            axis_id, axis_id, bound, axis_id);
  }
}

// Legacy entry: dispatches to ctx-aware version with NULL ctx.
static void rmu_emit_range_open(Term r, FILE *fp, u32 depth,
                                u32 opt_kind) {
  rmu_emit_range_open_ctx(r, fp, depth, opt_kind, NULL);
}

// Returns 1 if range index `i` is a LOCAL axis (LOCAL-OPT'd, or legacy
// axis_type==4).  `opt_kinds[]` may be NULL.
static int rmu_range_is_local(Term const *ranges, u32 const *opt_kinds, u32 i) {
  Term r = ranges[i];
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  if (opt_kinds != NULL && opt_kinds[i] == UOP_OPT_LOCAL) return 1;
  u32 axis_type = (u32)term_val(heap_read(term_val(r) + 1));
  return axis_type == 4 /* legacy KAX_LOCAL */;
}

// Build the promoted-GLOBAL decode context.  `promote[]` is parallel
// to `ranges[]`: 1 means "this output axis was a plain KAX_LOOP with
// no OPT and should become a parallel grid axis".  Walks right-to-left
// so the innermost promoted axis gets stride 1 and each one to the
// left multiplies its inner's extent.  `opt_kinds[]` (parallel to
// `ranges[]`, may be NULL) is scanned for LOCAL-OPT'd ranges; when
// present, out->has_local is set so the GLOBAL decode uses `tg`
// instead of `tid` (one threadgroup per GLOBAL tuple, LOCAL axes
// over `tt`).  Legacy axis_type==4 (KAX_LOCAL) ranges also count.  The
// LOCAL axes get their own (stride, modulus) decode over `tt` -- the
// same right-to-left scan, in `ranges[]` order, so axis ids match the
// emission order; a single LOCAL axis ends up stride 1 (`uint aN = tt`).
static void rmu_compute_global_decode_ctx(Term const *ranges, u32 n_ranges,
                                          u8 const *promote,
                                          u32 const *opt_kinds,
                                          RmuGlobalDecode *out) {
  memset(out, 0, sizeof(*out));
  u32 stride = 1;
  u32 n_glb  = 0;
  u64 total  = 1;
  for (i32 i = (i32)n_ranges - 1; i >= 0; i--) {
    if (!promote[i]) continue;
    Term r = ranges[i];
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
    u64 loc = term_val(r);
    u32 axis_id = (u32)term_val(heap_read(loc + 0));
    u32 extent  = (u32)term_val(heap_read(loc + 2));
    if (axis_id >= 256 || extent == 0) continue;
    out->stride_of_axis [axis_id] = stride;
    out->modulus_of_axis[axis_id] = extent;
    n_glb++;
    stride *= extent;
    total  *= extent;
  }
  out->n_globals = n_glb;
  out->total     = (n_glb > 0) ? total : 0;
  if (!RMU_TARGET_C) {
    // LOCAL axes: right-to-left so the innermost (last in ranges[]) gets
    // stride 1.  `tt = sum_K lK * local_stride[K]` mirrors the GLOBAL
    // flat decode over `tg`.
    u32 lstride = 1;
    u32 n_loc   = 0;
    u64 ltotal  = 1;
    for (i32 i = (i32)n_ranges - 1; i >= 0; i--) {
      if (!rmu_range_is_local(ranges, opt_kinds, (u32)i)) continue;
      Term r = ranges[i];
      u64 loc = term_val(r);
      u32 axis_id = (u32)term_val(heap_read(loc + 0));
      u32 extent  = (u32)term_val(heap_read(loc + 2));
      if (axis_id >= 256 || extent == 0) continue;
      out->local_stride_of_axis [axis_id] = lstride;
      out->local_modulus_of_axis[axis_id] = extent;
      n_loc++;
      lstride *= extent;
      ltotal  *= extent;
    }
    out->n_locals     = n_loc;
    out->local_total  = (n_loc > 0) ? ltotal : 0;
    out->has_local    = (n_loc > 0);
  }
}

// Back-compat shim: no LOCAL axis (opt_kinds = NULL -> has_local = 0).
static void rmu_compute_global_decode(Term const *ranges, u32 n_ranges,
                                      u8 const *promote,
                                      RmuGlobalDecode *out) {
  rmu_compute_global_decode_ctx(ranges, n_ranges, promote, NULL, out);
}

// Shared prelude for the reduce-shaped emit paths (rmu_emit_conv and
// the generic accumulator path in rmu_emit_store_reduce).  Given the
// output-axis ranges (the reduce axis already split off) and the
// store position `addr`, this:
//   1. promotes every plain-LOOP output axis that actually indexes
//      the store position to a parallel grid axis (decoded from tid),
//   2. emits the `if (tid >= total) return;` bounds guard,
//   3. emits each output axis (thread-position bind for promoted /
//      LOCAL / explicit-GLOBAL, `for`-loop otherwise),
//   4. fills needs_close[] and returns the post-prelude body depth.
// Auxiliary loop axes (in red_src but NOT in addr -- rare) stay
// serial: promoting them would make threads race on the same output
// address.
static u32 rmu_emit_output_loops(Term addr, Term const *out_ranges,
                                 u32 const *out_kinds,
                                 u32 const *out_factors,
                                 u32 n_out, u32 depth, FILE *fp,
                                 int *needs_close) {
  Term addr_ranges[MAX_DIM];
  u32  addr_n = 0;
  rmu_collect_ranges(addr, addr_ranges, &addr_n);
  u32 addr_axes[MAX_DIM];
  for (u32 i = 0; i < addr_n; i++) {
    addr_axes[i] = (term_tag(addr_ranges[i]) == TAG_UOP
                    && term_ext(addr_ranges[i]) == UOP_RANGE)
                 ? (u32)term_val(heap_read(term_val(addr_ranges[i]) + 0))
                 : 0xFFFFFFFFu;
  }
  // C99 target has no thread positions: keep serial loops.  Otherwise:
  // promote every plain-KAX_LOOP output axis (no OPT wrapper) that
  // indexes the store position to a parallel grid axis decoded from
  // `tid`.  This composes with OPT'd axes: a UPCAST/LOCAL split leaves
  // the OUTER half as a plain KAX_LOOP (which we promote here) and the
  // INNER half as KAX_UPCAST/KAX_LOCAL with an OPT wrapper (which the
  // per-axis emit below handles -- `#pragma unroll` loop / `tt` bind).
  // So a UPCAST'd matmul still gets one-output-element-per-thread on
  // its M / N-outer axes while the N-inner axis is the unroll loop.
  u8 promote[MAX_DIM] = {0};
  if (!RMU_TARGET_C) {
    for (u32 i = 0; i < n_out; i++) {
      Term r = out_ranges[i];
      if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
      if (out_kinds[i] != RMU_NO_OPT) continue;
      u32 axis_id   = (u32)term_val(heap_read(term_val(r) + 0));
      u32 axis_type = (u32)term_val(heap_read(term_val(r) + 1));
      if (axis_type != 0 /* KAX_LOOP */) continue;
      int is_output = 0;
      for (u32 j = 0; j < addr_n; j++) if (addr_axes[j] == axis_id) { is_output = 1; break; }
      if (is_output) promote[i] = 1;
    }
  }
  RmuGlobalDecode gd;
  rmu_compute_global_decode_ctx(out_ranges, n_out, promote, out_kinds, &gd);
  if (gd.n_globals > 0 && gd.total > 0) {
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fprintf(fp, "if (%s >= %lluu) return;\n",
            gd.has_local ? "tg" : "tid", (unsigned long long)gd.total);
  }
  u32 body_depth = depth;
  for (u32 i = 0; i < n_out; i++) {
    Term r = out_ranges[i];
    u32 axis_id   = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 0)) : 0xFFFFFFFFu;
    u32 axis_type = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 1)) : 0;
    int promoted    = (axis_id < 256 && gd.modulus_of_axis[axis_id] != 0);
    int threadbound = rmu_axis_is_threadbound(out_kinds[i], axis_type)
                   || promoted;
    if (out_kinds[i] != RMU_NO_OPT
        && (out_kinds[i] == UOP_OPT_UNROLL || out_kinds[i] == UOP_OPT_UPCAST)
        && !threadbound) {
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      if (out_factors[i] > 0) fprintf(fp, "#pragma unroll(%u)\n", out_factors[i]);
      else                    fputs("#pragma unroll\n", fp);
    }
    rmu_emit_range_open_ctx(r, fp, body_depth, out_kinds[i], &gd);
    if (!threadbound) { needs_close[i] = 1; body_depth++; }
  }
  return body_depth;
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

// === Flattened-conv-reduce splitter ====================================
//
// The im2col `_pool` conv lowering compresses (cIn, kh, kw) into a single
// flattened reduce axis a5 with extent cIn*kh*kw, then the consumer's
// xCol address (and the wFlat address) decompose it back via IDIV/IMOD:
//   a5/25, (a5/5)%5, a5%5  for (cIn=32, kh=5, kw=5)
// Those div/mod ops cost 2 idiv + 2 imod per inner-loop iteration and
// keep the address non-affine in the reduce var, blocking the TC matmul
// template (recognise_tc rejects div/mod addresses).
//
// Recovery: collect the constant divisors {c : IDIV(a5,c) or IMOD(a5,c)
// appears in the body}.  Sorted ascending [d1 < d2 < ... < d_{n-1}],
// the radix structure is:
//   stride[0]=1, stride[i]=d_i                    (for i in 1..n-1)
//   ext[0]=d1, ext[i]=d_{i+1}/d_i, ext[n-1]=E/d_{n-1}
//   a5 = sum_i axis_i * stride[i]
// Then substitute a5 -> that composite everywhere in the body; the
// constructor-time int simplifier collapses (cin*25+kh*5+kw)/25 -> cin
// etc.  Caller emits `for cin { for kh { for kw { acc += body } } }`.
#define RMU_CONV_SPLIT_MAX 6
typedef struct {
  u32 n;                          // number of recovered axes (>=2)
  u32 axis_id[RMU_CONV_SPLIT_MAX];// fresh axis ids, innermost..outermost
  u32 extent[RMU_CONV_SPLIT_MAX]; // per-axis extent, innermost..outermost
  u32 stride[RMU_CONV_SPLIT_MAX]; // per-axis stride into the flat index
} RmuConvSplit;

// Walk `t` collecting constants `c` from IDIV(x,c) / IMOD(x,c) where x
// is a UOP_RANGE leaf with axis_id == want_axis.  Bounded recursion.
static void rmu_collect_divmod_consts(Term t, u32 want_axis,
                                      u32 *consts, u32 *n_consts,
                                      u32 cap, u32 depth) {
  if (depth > 64) return;
  if (term_tag(t) != TAG_UOP) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_IDIV || op == UOP_IMOD) {
    Term a = heap_read(loc + 0);
    Term b = heap_read(loc + 1);
    if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_RANGE
        && (u32)term_val(heap_read(term_val(a) + 0)) == want_axis
        && term_tag(b) == TAG_UOP && term_ext(b) == UOP_CONST) {
      Term num = heap_read(term_val(b));
      if (term_tag(num) == TAG_NUM) {
        u32 c = (u32)term_val(num);
        if (c > 1) {
          int seen = 0;
          for (u32 i = 0; i < *n_consts; i++) if (consts[i] == c) seen = 1;
          if (!seen && *n_consts < cap) consts[(*n_consts)++] = c;
        }
      }
    }
    // also descend (a div/mod operand could itself contain more).
    rmu_collect_divmod_consts(a, want_axis, consts, n_consts, cap, depth + 1);
    rmu_collect_divmod_consts(b, want_axis, consts, n_consts, cap, depth + 1);
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_ILT: case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_collect_divmod_consts(heap_read(loc + 0), want_axis, consts, n_consts, cap, depth + 1);
      rmu_collect_divmod_consts(heap_read(loc + 1), want_axis, consts, n_consts, cap, depth + 1);
      return;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2: case UOP_LOG2: case UOP_SQRT:
    case UOP_CAST: case UOP_BITCAST: case UOP_OPT: case UOP_LOAD:
      rmu_collect_divmod_consts(heap_read(loc + 0), want_axis, consts, n_consts, cap, depth + 1);
      return;
    case UOP_IWHERE:
      rmu_collect_divmod_consts(heap_read(loc + 0), want_axis, consts, n_consts, cap, depth + 1);
      rmu_collect_divmod_consts(heap_read(loc + 1), want_axis, consts, n_consts, cap, depth + 1);
      rmu_collect_divmod_consts(heap_read(loc + 2), want_axis, consts, n_consts, cap, depth + 1);
      return;
    default:
      return;
  }
}

// Find the largest UOP_RANGE axis_id reachable from `t`.  Used to
// allocate fresh axis ids for the split's new RANGE leaves.  Bounded.
static u32 rmu_max_axis_id(Term t, u32 cur, u32 depth) {
  if (depth > 256) return cur;
  if (term_tag(t) != TAG_UOP) return cur;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 a = (u32)term_val(heap_read(loc + 0));
    return a > cur ? a : cur;
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    cur = rmu_max_axis_id(heap_read(loc + i), cur, depth + 1);
  }
  return cur;
}

// Try to recover the radix structure of a flattened reduce axis.
// Returns 1 on success (filling *out), 0 if the body doesn't decompose
// the axis via div/mod, the divisor set isn't a valid divisor chain,
// or the structure exceeds RMU_CONV_SPLIT_MAX axes.
static int rmu_recover_conv_split(Term red_src, u32 red_axis, u32 red_extent,
                                  Term store_addr, RmuConvSplit *out) {
  if (red_extent < 2) return 0;
  u32 consts[RMU_CONV_SPLIT_MAX];
  u32 n_consts = 0;
  rmu_collect_divmod_consts(red_src, red_axis, consts, &n_consts,
                            RMU_CONV_SPLIT_MAX, 0);
  if (n_consts == 0) return 0;
  // Insertion sort ascending (n is tiny).
  for (u32 i = 1; i < n_consts; i++) {
    u32 v = consts[i]; i32 j = (i32)i - 1;
    while (j >= 0 && consts[j] > v) { consts[j + 1] = consts[j]; j--; }
    consts[j + 1] = v;
  }
  // Validate divisor chain: d_{i+1} % d_i == 0, d_i | red_extent.
  for (u32 i = 0; i + 1 < n_consts; i++) {
    if (consts[i] == 0 || consts[i + 1] % consts[i] != 0) return 0;
  }
  if (red_extent % consts[n_consts - 1] != 0) return 0;
  u32 n = n_consts + 1;            // axes: kw .. cin  (one more than divisors)
  if (n > RMU_CONV_SPLIT_MAX || n < 2) return 0;
  // strides: [1, d1, d2, ..., d_{n-1}]
  // extents: [d1, d2/d1, ..., d_{n-1}/d_{n-2}, E/d_{n-1}]
  u32 strides[RMU_CONV_SPLIT_MAX];
  u32 extents[RMU_CONV_SPLIT_MAX];
  strides[0] = 1;
  for (u32 i = 1; i < n; i++) strides[i] = consts[i - 1];
  extents[0] = consts[0];
  for (u32 i = 1; i + 1 < n; i++) extents[i] = consts[i] / consts[i - 1];
  extents[n - 1] = red_extent / consts[n_consts - 1];
  u64 prod = 1;
  for (u32 i = 0; i < n; i++) {
    if (extents[i] == 0) return 0;
    prod *= extents[i];
  }
  if (prod != (u64)red_extent) return 0;   // not a clean factorization
  // Allocate fresh axis ids (max+1 .. max+n) so they never collide with
  // any existing axis or with red_axis (red_axis is consumed by the
  // composite substitution and disappears).
  u32 maxid = rmu_max_axis_id(red_src, red_axis, 0);
  maxid = rmu_max_axis_id(store_addr, maxid, 0);
  out->n = n;
  for (u32 i = 0; i < n; i++) {
    out->axis_id[i] = maxid + 1 + i;
    out->extent[i]  = extents[i];
    out->stride[i]  = strides[i];
  }
  return 1;
}

// Build the composite linear-index Term  sum_i a_i * stride_i  from the
// split's fresh RANGE leaves.  These leaves carry axis_type KAX_REDUCE.
static Term rmu_build_conv_split_composite(RmuConvSplit const *sp) {
  Term acc = 0;
  for (u32 i = 0; i < sp->n; i++) {
    Term r = uop_range(sp->axis_id[i], KAX_REDUCE, sp->extent[i]);
    Term term_i = (sp->stride[i] == 1)
                ? r
                : uop_int_binary(UOP_IMUL, r, uop_const(DT_INT32, sp->stride[i]));
    acc = (acc == 0) ? term_i : uop_int_binary(UOP_IADD, acc, term_i);
  }
  return acc;
}

// uop_graph_rewrite rule: replace every UOP_RANGE leaf whose axis_id ==
// red_axis with `composite`.  The constructor-time int simplifier folds
// the re-substituted div/mod in the consumer addresses.
typedef struct { u32 red_axis; Term composite; } RmuConvSubstCtx;
static Term rmu_conv_subst_range_rule(Term t, void *user) {
  RmuConvSubstCtx *cx = (RmuConvSubstCtx *)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  if ((u32)term_val(heap_read(term_val(t) + 0)) != cx->red_axis) return 0;
  return cx->composite;
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

  // Identify M-axis (in addr_a, not red) and N-axis (in addr_b, not
  // red).  We need each extent + axis_id + axis_type so the outer
  // emission can either open for-loops (LOOP / default) or bind to
  // thread-position (LOCAL / GLOBAL) for parallel multi-SG dispatch.
  u32 m_axis_id = 0xFFFFFFFFu, m_extent = 0, m_axis_type = 0;
  u32 n_axis_id_v = 0xFFFFFFFFu, n_extent = 0, n_axis_type = 0;
  {
    Term ra[MAX_DIM]; u32 ra_n = 0;
    rmu_collect_ranges(addr_a, ra, &ra_n);
    for (u32 i = 0; i < ra_n; i++) {
      u32 aid = term_val(heap_read(term_val(ra[i]) + 0));
      if (aid != red_axis) {
        m_axis_id = aid;
        m_axis_type = (u32)term_val(heap_read(term_val(ra[i]) + 1));
        m_extent = (u32)term_val(heap_read(term_val(ra[i]) + 2));
        break;
      }
    }
    Term rb[MAX_DIM]; u32 rb_n = 0;
    rmu_collect_ranges(addr_b, rb, &rb_n);
    for (u32 i = 0; i < rb_n; i++) {
      u32 aid = term_val(heap_read(term_val(rb[i]) + 0));
      if (aid != red_axis) {
        n_axis_id_v = aid;
        n_axis_type = (u32)term_val(heap_read(term_val(rb[i]) + 1));
        n_extent = (u32)term_val(heap_read(term_val(rb[i]) + 2));
        break;
      }
    }
  }
  if (m_extent == 0 || n_extent == 0
      || (m_extent % 8) != 0 || (n_extent % 8) != 0
      || m_axis_id == 0xFFFFFFFFu || n_axis_id_v == 0xFFFFFFFFu) {
    // M/N also need to be 8-tiled for simdgroup_matrix<8,8>; bail.
    return 0;
  }

  // Parallel-TC selector.  If the M and N axes carry GLOBAL axis_type
  // (Phase E annotation), the caller's dispatch shape binds each
  // threadgroup to a unique 8x8 output tile, so the multi-SG write
  // race is impossible -- drop the `sgi==0 && tg==0` guard and emit
  // position-bound m/n declarations instead of for-loops.  Coverage:
  //   m=GLOBAL && n=GLOBAL  -> 2D-folded `tg` linearises tiles:
  //                            m = (tg / N_tiles)*8, n = (tg % N_tiles)*8
  //   m=GLOBAL && n=LOOP    -> m = tg*8, n loops over N_tiles*8
  //   m=LOOP   && n=GLOBAL  -> n = tg*8, m loops over M_tiles*8
  //   else                  -> guarded sequential (legacy / safe)
  //
  // Dispatch (Phase E parallel TC, both GLOBAL):
  //   grid       = (num_m_tiles * num_n_tiles * 32, 1, 1)
  //   threadgroup = (32, 1, 1)
  // i.e. one simdgroup per TG, one TG per output tile.
  int m_par = (m_axis_type == 5 /* KAX_GLOBAL */);
  int n_par = (n_axis_type == 5 /* KAX_GLOBAL */);
  int parallel_tc = (m_par || n_par);
  u32 n_tiles_n = n_extent / 8;

  if (parallel_tc) {
    // No guard.  Bind axes per axis_type.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("/* parallel TC: m/n bound to tg; one SG per output tile */\n", fp);
    if (m_par && n_par) {
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (tg / %uu) * 8u;\n", m_axis_id, n_tiles_n);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = (tg %% %uu) * 8u;\n", n_axis_id_v, n_tiles_n);
    } else if (m_par) {
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = tg * 8u;\n", m_axis_id);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
              n_axis_id_v, n_axis_id_v, n_extent, n_axis_id_v);
    } else { /* n_par */
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "uint a%u = tg * 8u;\n", n_axis_id_v);
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
              m_axis_id, m_axis_id, m_extent, m_axis_id);
    }
  } else {
    // Legacy guarded sequential path.  Multi-simdgroup race guard:
    // simdgroup_matrix ops cooperate on the calling simdgroup's 32
    // threads.  When the dispatch shape binds multiple SGs/TGs (the
    // default for output_numel >= 32), every SG runs the same code
    // and writes to the same output addresses concurrently; on M3
    // this race yields garbage outputs.  Gate the body so only the
    // first SG of the first TG runs; others idle.  Wasteful but
    // correct under any dispatch shape.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("if (sgi == 0u && tg == 0u) {\n", fp);
    for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
            m_axis_id, m_axis_id, m_extent, m_axis_id);
    for (u32 d = 0; d < depth + 2; d++) fputs("  ", fp);
    fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u += 8) {\n",
            n_axis_id_v, n_axis_id_v, n_extent, n_axis_id_v);
  }

  // body_depth depends on path:
  //   parallel_tc & both GLOBAL: depth (no opener)
  //   parallel_tc & one GLOBAL : depth + 1 (one for-loop opener)
  //   guarded                  : depth + 3 (guard + 2 for-loops)
  u32 body_depth;
  if (parallel_tc) {
    body_depth = (m_par && n_par) ? depth : (depth + 1);
  } else {
    body_depth = depth + 3;
  }

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
  fprintf(fp, "], %u);\n", k_extent);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_load(_b_mat, &%s[", rmu_buf_name(buf_b));
  rmu_emit_term(addr_b, fp);
  fprintf(fp, "], %u);\n", n_extent);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  fputs("simdgroup_multiply_accumulate(_c_mat, _a_mat, _b_mat, _c_mat);\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fputs("}\n", fp);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "simdgroup_store(_c_mat, &%s[", rmu_buf_name(buf_c));
  rmu_emit_term(addr_c, fp);
  fprintf(fp, "], %u);\n", n_extent);

  // Close any blocks opened above based on which path we took.
  if (parallel_tc) {
    if (!(m_par && n_par)) {
      // One for-loop opener (the non-GLOBAL axis); close it.
      for (u32 d = 0; d < depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    // both-GLOBAL case: nothing to close (no openers).
  } else {
    // Guarded path: close N, M, then the sgi/tg guard.
    for (u32 d = 0; d < depth + 2; d++) fputs("  ", fp);
    fputs("}\n", fp);
    for (u32 d = 0; d < depth + 1; d++) fputs("  ", fp);
    fputs("}\n", fp);
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fputs("}\n", fp);
  }
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

// Recognise the canonical conv2d-flat shape:
//   STORE(C, addr_C, OPT(REDUCE(MUL(INDEX_E(W, _), X_VAL), SUM,
//                              k_axis), CONV, _))
// where X_VAL is INDEX_E(X, _) (single-input conv) or a UOP_IWHERE
// chain (multi-input im2col).  The OPT wrapper is installed by
// uop_recognise_conv when it spots IDIV/IMOD in either INDEX_E
// address tree (the structural marker for decomposed conv axes).
// Detection is structural; if it matches, returns 1 and fills
// `*out_red_value` with the inner REDUCE term so the caller can
// emit through the conv template.
static int rmu_detect_conv(Term store, Term *out_red_value) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  Term value = heap_read(term_val(store) + 2);
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_OPT) return 0;
  if (uop_opt_kind(value) != UOP_OPT_CONV) return 0;
  Term inner = uop_opt_target(value);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_REDUCE) return 0;
  u64 rloc = term_val(inner);
  u32 kind = term_val(heap_read(rloc + 1));
  if (kind != REDUCE_SUM) return 0;
  Term mul = heap_read(rloc + 0);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  if (out_red_value != NULL) *out_red_value = inner;
  return 1;
}

// Conv2d-flat template.  Emits the same loop nest as the generic
// rmu_emit_store_reduce path -- output for-loop over r_out, scalar
// accumulator over r_q -- with two perf-oriented additions:
//
//   1. `#pragma unroll` on the inner reduce loop so the compiler can
//      unroll the (typically small: 9 for 3x3x1, 27 for 3x3x3) KRED
//      iterations into straight-line MUL+ADDs.  Tinygrad's CONV
//      template does the same.
//   2. A `/* CONV2D template */` marker comment so dispatch traces
//      can confirm which path fired.
//
// The decision to emit `#pragma unroll` versus stay on the generic
// path is gated on KRED <= RMU_REDUCE_UNROLL_MAX so we don't blow up
// the generated body size on huge KREDs (very deep convs).
//
// Returns 1 on success; 0 if the shape can't be emitted through this
// template and the caller should fall back to the generic accumulator.
static int rmu_emit_conv(Term store, Term conv_red, FILE *fp, u32 depth) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  if (term_tag(conv_red) != TAG_UOP || term_ext(conv_red) != UOP_REDUCE) return 0;
  u64 sloc = term_val(store);
  Term buf  = heap_read(sloc + 0);
  Term addr = heap_read(sloc + 1);
  u64 rloc = term_val(conv_red);
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
    // No reduce range in the body -- conv with degenerate K=0; bail.
    return 0;
  }
  Term red_range = ranges[reduce_idx];
  u32 red_extent = (term_tag(red_range) == TAG_UOP
                    && term_ext(red_range) == UOP_RANGE)
                 ? (u32)term_val(heap_read(term_val(red_range) + 2)) : 0;
  if (red_extent == 0) return 0;

  // Flattened multi-axis reduce: if the body decomposes the reduce var
  // via IDIV/IMOD (the im2col `_pool` conv tell), split it into its
  // component axes, substitute the composite linear index, and emit
  // nested reduce loops with a single accumulator -- removing the
  // 2 idiv + 2 imod per inner iteration and making every address
  // affine in each component axis (so the TC matmul template can fire).
  RmuConvSplit sp = {0};
  if (!RMU_TARGET_C
      && rmu_recover_conv_split(red_src, red_axis, red_extent, addr, &sp)) {
    Term composite = rmu_build_conv_split_composite(&sp);
    RmuConvSubstCtx cx = { red_axis, composite };
    UOpGraphRewriteRule rules[1] = { { "conv_split_subst", rmu_conv_subst_range_rule } };
    Term red_src2 = uop_graph_rewrite(red_src, rules, 1, &cx);
    // Marker for dispatch tracing.
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fprintf(fp, "/* CONV2D template (KRED=%u split=", red_extent);
    for (u32 i = 0; i < sp.n; i++) fprintf(fp, "%s%u", i ? "x" : "", sp.extent[i]);
    fputs(") */\n", fp);
    int needs_close[MAX_DIM] = {0};
    u32 body_depth = rmu_emit_output_loops(addr, out_ranges, out_kinds,
                                           out_factors, n_out, depth, fp,
                                           needs_close);
    // Single accumulator outside ALL the reduce loops.
    char acc_name[32];
    snprintf(acc_name, sizeof(acc_name), "_acc%u", red_axis);
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fprintf(fp, "float %s = ", acc_name);
    rmu_emit_reduce_init(red_kind, fp);
    fputs(";\n", fp);
    // Decide which axes to #pragma-unroll: walk innermost (stride 1) ->
    // outermost, unrolling while the cumulative unrolled iteration count
    // stays within RMU_REDUCE_UNROLL_MAX -- so kh/kw straight-line into
    // ~25 MAD ops but the larger cin loop stays rolled.  Once an axis
    // can't be unrolled, no outer axis is either.
    int do_unroll[RMU_CONV_SPLIT_MAX] = {0};
    {
      u64 prod = 1;
      for (u32 i = 0; i < sp.n; i++) {
        if ((u64)sp.extent[i] * prod <= RMU_REDUCE_UNROLL_MAX) {
          do_unroll[i] = 1; prod *= sp.extent[i];
        } else break;
      }
    }
    // Emit the reduce loops outermost (largest stride) -> innermost.
    u32 loop_depth = body_depth;
    for (i32 i = (i32)sp.n - 1; i >= 0; i--) {
      Term r = uop_range(sp.axis_id[i], KAX_REDUCE, sp.extent[i]);
      if (do_unroll[i]) {
        for (u32 d = 0; d < loop_depth; d++) fputs("  ", fp);
        fprintf(fp, "#pragma unroll(%u)\n", sp.extent[i]);
      }
      rmu_emit_range_open(r, fp, loop_depth, RMU_NO_OPT);
      loop_depth++;
    }
    for (u32 d = 0; d < loop_depth; d++) fputs("  ", fp);
    rmu_emit_reduce_combine(acc_name, red_kind, red_src2, fp);
    for (i32 i = (i32)sp.n - 1; i >= 0; i--) {
      loop_depth--;
      for (u32 d = 0; d < loop_depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    // Final store.
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fprintf(fp, "%s[", rmu_buf_name(buf));
    rmu_emit_term(addr, fp);
    fprintf(fp, "] = %s;\n", acc_name);
    for (i32 i = (i32)n_out - 1; i >= 0; i--) {
      if (!needs_close[i]) continue;
      body_depth--;
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      fputs("}\n", fp);
    }
    return 1;
  }

  // Marker for dispatch tracing.
  for (u32 d = 0; d < depth; d++) fputs("  ", fp);
  fprintf(fp, "/* CONV2D template (KRED=%u) */\n", red_extent);

  // Emit output ranges: promoted-GLOBAL (tid decode) for plain-LOOP
  // output axes, threadbinds for LOCAL/explicit-GLOBAL, serial loops
  // otherwise.  Emits the `if (tid >= total) return;` bounds guard.
  int needs_close[MAX_DIM] = {0};
  u32 body_depth = rmu_emit_output_loops(addr, out_ranges, out_kinds,
                                         out_factors, n_out, depth, fp,
                                         needs_close);
  // Accumulator decl; reduce-axis loop with #pragma unroll when KRED
  // is small.  Skip the pragma on the C target -- C99 has no
  // #pragma unroll; clang accepts `#pragma clang loop unroll(full)`
  // but we prefer not to gate per-target inside this helper.
  char acc_name[32];
  snprintf(acc_name, sizeof(acc_name), "_acc%u", red_axis);
  for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
  fprintf(fp, "float %s = ", acc_name);
  rmu_emit_reduce_init(red_kind, fp);
  fputs(";\n", fp);
  if (!RMU_TARGET_C && red_extent <= RMU_REDUCE_UNROLL_MAX) {
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    fprintf(fp, "#pragma unroll(%u)\n", red_extent);
  }
  rmu_emit_range_open(red_range, fp, body_depth, RMU_NO_OPT);
  for (u32 d = 0; d < body_depth + 1; d++) fputs("  ", fp);
  rmu_emit_reduce_combine(acc_name, red_kind, red_src, fp);
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

// REDUCE-shaped emission: STORE(buf, addr, REDUCE(src, kind, axis)).
// Hoists an accumulator outside the reduce-axis loop and references it
// in the store statement.  Returns 1 if the shape matched and was
// emitted; 0 if the caller should fall back to the generic path.
//
// Dispatches to specialised templates when the recogniser pre-pass
// (uop_recognise_tc / uop_recognise_conv) wrapped the value:
//   OPT(_, TC,   _) -> rmu_emit_matmul_tc  (simdgroup_matrix MMA)
//   OPT(_, CONV, _) -> rmu_emit_conv       (conv2d-flat #pragma unroll)
// TC falls through to the scalar accumulator on tile-size mismatch
// (K%8 != 0); CONV always succeeds for shapes the recogniser installs.
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
  // F4: dispatch to conv2d template when CONV-annotated.  rmu_emit_conv
  // only fails on degenerate KRED=0 shapes that uop_recognise_conv never
  // installs, so any CONV-wrapped root that arrives here emits through
  // the template; no fallback path is exercised in production or tests.
  Term conv_red = 0;
  if (rmu_detect_conv(store, &conv_red) && rmu_emit_conv(store, conv_red, fp, depth)) {
    return 1;
  }
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  Term buf  = heap_read(sloc + 0);
  Term addr = heap_read(sloc + 1);
  u64 rloc      = term_val(value);
  Term red_src  = heap_read(rloc + 0);
  // Decline if the reduce body contains a nested reduce: this
  // specialisation only emits the OUTER accumulator, so an inner
  // `_accN` referenced in the body would be undeclared in the MSL.
  // Fall through to the generic rmu_emit_store path, which
  // post-order-collects every reduce and hoists each accumulator's
  // declaration ahead of its consumers (handles e.g. the BN-fused
  // forward kernel `sum_w((ReLU(conv_out_w) * bn_scale)^2)` where
  // bn_scale = mean/var reduces nested inside the w-sum's body).
  if (rmu_term_has_reduce(red_src, 0)) return 0;
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

  // Emit output ranges: promoted-GLOBAL (tid decode) for plain-LOOP
  // output axes, threadbinds for LOCAL/explicit-GLOBAL, serial loops
  // otherwise.  Emits the `if (tid >= total) return;` bounds guard.
  int needs_close[MAX_DIM] = {0};
  u32 body_depth = rmu_emit_output_loops(addr, out_ranges, out_kinds,
                                         out_factors, n_out, depth, fp,
                                         needs_close);
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
  // Reduce-axis loop.  When the autotuner wrapped the axis in
  // OPT(UNROLL, factor) honour that exactly.  Otherwise, default to a
  // full unroll when the extent is small (matmul K=25, etc.) so the
  // MSL compiler can straight-line the contraction MADs.
  if (red_kind_opt != RMU_NO_OPT && red_kind_opt == UOP_OPT_UNROLL) {
    for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
    if (red_factor_opt > 0) fprintf(fp, "#pragma unroll(%u)\n", red_factor_opt);
    else                    fputs("#pragma unroll\n", fp);
  } else if (red_kind_opt == RMU_NO_OPT && !RMU_TARGET_C) {
    u32 red_extent = uop_range_extent(red_range);
    if (red_extent > 0 && red_extent <= RMU_REDUCE_UNROLL_MAX) {
      for (u32 d = 0; d < body_depth; d++) fputs("  ", fp);
      fprintf(fp, "#pragma unroll(%u)\n", red_extent);
    }
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

  // Collect output-axis ids: ranges in the addr expression are
  // output axes (they index the store position).  Ranges that
  // appear ONLY in the value expression are reduce or auxiliary.
  Term addr_ranges[MAX_DIM];
  u32  addr_n = 0;
  rmu_collect_ranges(addr, addr_ranges, &addr_n);
  u32 addr_axes[MAX_DIM];
  for (u32 i = 0; i < addr_n; i++) {
    addr_axes[i] = (term_tag(addr_ranges[i]) == TAG_UOP
                    && term_ext(addr_ranges[i]) == UOP_RANGE)
                 ? (u32)term_val(heap_read(term_val(addr_ranges[i]) + 0))
                 : 0xFFFFFFFFu;
  }

  // Default-parallelise: every output axis (axis_id in addr_axes)
  // that's still a plain KAX_LOOP with no OPT wrap becomes a GLOBAL
  // grid axis.  Without this the renderer emits a serial for-loop
  // nest -- the dispatcher launches N threads (one per output
  // element) but every thread re-runs the full nest, producing ~N x
  // over-work.  Reduce / UPCAST / UNROLL / LOCAL / GROUP axes and
  // any axis already KAX_GLOBAL are left alone (the TC matmul and
  // conv2d_flat templates run before this and bind their own
  // parallel axes).  The decode context below treats the promoted
  // ranges identically to ranges that arrived axis_type==KAX_GLOBAL.
  // This composes with OPT'd axes: a UPCAST/LOCAL split's OUTER half
  // is a plain KAX_LOOP and gets promoted; the INNER half carries the
  // OPT wrapper and emits via its OPT-specific path (`#pragma unroll`
  // for UPCAST/UNROLL, `tt` bind for LOCAL).
  u8 promote_global[MAX_DIM] = {0};
  if (!RMU_TARGET_C) {
    for (u32 i = 0; i < n_ranges; i++) {
      Term r = ranges[i];
      if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
      if (opt_kinds[i] != RMU_NO_OPT) continue;
      u32 axis_id   = (u32)term_val(heap_read(term_val(r) + 0));
      u32 axis_type = (u32)term_val(heap_read(term_val(r) + 1));
      if (axis_type != 0 /* KAX_LOOP */) continue;
      int is_output = 0;
      for (u32 j = 0; j < addr_n; j++) if (addr_axes[j] == axis_id) { is_output = 1; break; }
      if (is_output) promote_global[i] = 1;
    }
  }

  // Multi-GLOBAL decode context: when >=2 GLOBAL axes flow through
  // tid, each needs its own (stride, modulus) decode from the flat
  // 1-D dispatch index.  Single-GLOBAL kernels emit `uint a0 = tid;`
  // (one axis, stride 1).  If the kernel also has a LOCAL axis the
  // GLOBAL axes decode from `tg` instead -- see RmuGlobalDecode.
  RmuGlobalDecode g_decode;
  rmu_compute_global_decode_ctx(ranges, n_ranges, promote_global,
                                opt_kinds, &g_decode);

  // Collect reduces and compute per-reduce *required emission depth*.
  // A reduce must emit AFTER all output-axis loops it depends on are
  // open (so its body can reference those axes), and as EARLY as
  // possible thereafter (so it doesn't redundantly recompute per
  // inner-loop iteration).  We measure "depth" as the position+1 of
  // the deepest output range the reduce body references (in the
  // order ranges[] are emitted below).  required_pos[i] == 0 means
  // the reduce is fully hoistable (no output-axis dependence) -- the
  // legacy "hoistable softmax sum" case.  required_pos[i] == n_ranges
  // means it depends on every output axis (innermost emission).
  // Anything in between (e.g. softmax max_r depends only on the row
  // axis) emits BETWEEN output loops, avoiding redundant recompute
  // per inner-axis iteration.
  Term reduces[MAX_DIM];
  u8   reduce_simd_flag[MAX_DIM] = {0};
  u32  n_reduces = 0;
  rmu_collect_reduces_with_simd(value, 0, reduces, reduce_simd_flag,
                                &n_reduces);
  u32 required_pos[MAX_DIM] = {0};
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src = heap_read(term_val(reduces[i]) + 0);
    Term r_ranges[MAX_DIM];
    u32  r_n = 0;
    rmu_collect_ranges(r_src, r_ranges, &r_n);
    u32 max_pos = 0;
    for (u32 j = 0; j < r_n; j++) {
      if (term_tag(r_ranges[j]) != TAG_UOP
          || term_ext(r_ranges[j]) != UOP_RANGE) continue;
      u32 axis = (u32)term_val(heap_read(term_val(r_ranges[j]) + 0));
      for (u32 k = 0; k < n_ranges; k++) {
        if (term_tag(ranges[k]) != TAG_UOP
            || term_ext(ranges[k]) != UOP_RANGE) continue;
        u32 oaxis = (u32)term_val(heap_read(term_val(ranges[k]) + 0));
        if (oaxis == axis && (k + 1) > max_pos) max_pos = k + 1;
      }
    }
    required_pos[i] = max_pos;
  }
  // Transitive propagation for nested reduces.  rmu_collect_ranges
  // stops at UOP_REDUCE boundaries (the body's `_accN` reference is
  // a leaf to the outer scope), so the direct max_pos above does not
  // see output axes that an INNER reduce's body uses.  But the outer
  // reduce's emission references `_acc<inner>` at its own emit depth,
  // so the inner reduce must already be declared at that point.  We
  // achieve this by pushing the outer's required_pos to at least the
  // inner's required_pos for every reduce nested inside it.
  // Reduces are in post-order (inner before outer; see
  // rmu_collect_reduces_with_simd's post-order add), so a single
  // forward pass suffices: by the time we visit `i`, every reduce
  // appearing in reduces[i]'s body subtree has been fully updated.
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src_i = heap_read(term_val(reduces[i]) + 0);
    for (u32 j = 0; j < i; j++) {
      // Is reduces[j] structurally inside reduces[i]'s body?  Walk the
      // body subgraph (including descending into other UOP_REDUCE
      // bodies, since nesting can be transitive) and check for term
      // identity.  Bounded recursion -- DAG is finite + small.
      if (rmu_term_contains(r_src_i, reduces[j])
          && required_pos[j] > required_pos[i]) {
        required_pos[i] = required_pos[j];
      }
    }
  }

  // Helper used by the interleaved emission below: emit one reduce
  // (`float _accN = init; for (...) _accN = combine(...); }`) at a
  // given depth.  Pulled into its own block so we can call it both
  // before any output-axis loops and between successive ones.
  // When `is_simd` is set (the REDUCE was wrapped in OPT_SIMD_REDUCE),
  // emits the per-thread-strided form + Apple's simd_<op> intrinsic
  // (1-instruction simdgroup-collective reduce across 32 lanes)
  // instead of the scalar for-loop accumulator.
  #define RMU_EMIT_ONE_REDUCE(red, emit_depth, is_simd) do { \
    Term _red = (red); \
    u64  _rloc = term_val(_red); \
    u32  _r_kind = term_val(heap_read(_rloc + 1)); \
    u32  _r_axis = term_val(heap_read(_rloc + 2)); \
    Term _r_src = heap_read(_rloc + 0); \
    char _acc_name[32]; \
    snprintf(_acc_name, sizeof(_acc_name), "_acc%u", _r_axis); \
    for (u32 _d = 0; _d < (emit_depth); _d++) fputs("  ", fp); \
    fprintf(fp, "float %s = ", _acc_name); \
    rmu_emit_reduce_init(_r_kind, fp); \
    fputs(";\n", fp); \
    Term _r_ranges[MAX_DIM]; \
    u32  _r_kinds[MAX_DIM] = {0}; \
    u32  _r_factors[MAX_DIM] = {0}; \
    u32  _n_r_ranges = 0; \
    rmu_collect_ranges_with_opts(_r_src, _r_ranges, _r_kinds, \
                                 _r_factors, &_n_r_ranges); \
    Term _reduce_range_term = 0; \
    u32  _reduce_extent = 0; \
    for (u32 _j = 0; _j < _n_r_ranges; _j++) { \
      u32 _ax = term_val(heap_read(term_val(_r_ranges[_j]) + 0)); \
      if (_ax == _r_axis) { \
        _reduce_range_term = _r_ranges[_j]; \
        _reduce_extent     = uop_range_extent(_r_ranges[_j]); \
        break; \
      } \
    } \
    if (_reduce_range_term != 0) { \
      if ((is_simd) && !RMU_TARGET_C) { \
        /* SIMD-collective shape: each lane processes a 1/32 slice of */ \
        /* extent, then simd_<op> combines the 32 lane partials in a */ \
        /* single instruction. */ \
        for (u32 _d = 0; _d < (emit_depth); _d++) fputs("  ", fp); \
        fprintf(fp, "for (uint a%u = thread_index_in_simdgroup; " \
                "a%u < %u; a%u += 32u) {\n", \
                _r_axis, _r_axis, _reduce_extent, _r_axis); \
        for (u32 _d = 0; _d < (emit_depth) + 1; _d++) fputs("  ", fp); \
        rmu_emit_reduce_combine(_acc_name, _r_kind, _r_src, fp); \
        for (u32 _d = 0; _d < (emit_depth); _d++) fputs("  ", fp); \
        fputs("}\n", fp); \
        for (u32 _d = 0; _d < (emit_depth); _d++) fputs("  ", fp); \
        if (_r_kind == REDUCE_MAX) { \
          fprintf(fp, "%s = simd_max(%s);\n", _acc_name, _acc_name); \
        } else { \
          fprintf(fp, "%s = simd_sum(%s);\n", _acc_name, _acc_name); \
        } \
      } else { \
        if (!RMU_TARGET_C && _reduce_extent > 0 \
            && _reduce_extent <= RMU_REDUCE_UNROLL_MAX) { \
          for (u32 _d = 0; _d < (emit_depth); _d++) fputs("  ", fp); \
          fprintf(fp, "#pragma unroll(%u)\n", _reduce_extent); \
        } \
        rmu_emit_range_open(_reduce_range_term, fp, (emit_depth), 0); \
        for (u32 _d = 0; _d < (emit_depth) + 1; _d++) fputs("  ", fp); \
        rmu_emit_reduce_combine(_acc_name, _r_kind, _r_src, fp); \
        for (u32 _d = 0; _d < (emit_depth); _d++) fputs("  ", fp); \
        fputs("}\n", fp); \
      } \
    } \
  } while (0)

  // Bounds guard for promoted-GLOBAL kernels: the dispatcher launches
  // ceil(total/256)*256 threads (one per output element, rounded up to
  // a threadgroup multiple), so threads with tid >= total must do
  // nothing.  With a LOCAL axis the grid is exactly `total`
  // threadgroups (one per GLOBAL tuple), so the `tg >= total` form is
  // a no-op -- but emit it for symmetry.  Emit before any reduce/loop.
  if (g_decode.n_globals > 0 && g_decode.total > 0) {
    for (u32 d = 0; d < depth; d++) fputs("  ", fp);
    fprintf(fp, "if (%s >= %lluu) return;\n",
            g_decode.has_local ? "tg" : "tid",
            (unsigned long long)g_decode.total);
  }

  // Pass 0: emit reduces with required_pos == 0 BEFORE any output
  // loop.  These are fully hoistable -- their body uses no output axis.
  for (u32 i = 0; i < n_reduces; i++) {
    if (required_pos[i] != 0) continue;
    RMU_EMIT_ONE_REDUCE(reduces[i], depth, reduce_simd_flag[i]);
  }

  // Track which output ranges opened a `{` (thread-bound axes
  // don't), so we close the right number at the end.
  u32 body_depth = depth;
  int needs_close[MAX_DIM] = {0};
  for (u32 i = 0; i < n_ranges; i++) {
    Term r = ranges[i];
    u32 axis_id   = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 0)) : 0xFFFFFFFFu;
    u32 axis_type = (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE)
                  ? (u32)term_val(heap_read(term_val(r) + 1)) : 0;
    // Promoted output axes are thread-bound (decoded from `tid`, no
    // `{` block) just like LOCAL / explicit-GLOBAL axes.
    int promoted = (axis_id < 256 && g_decode.modulus_of_axis[axis_id] != 0);
    int threadbound = rmu_axis_is_threadbound(opt_kinds[i], axis_type)
                   || promoted;
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
    rmu_emit_range_open_ctx(r, fp, body_depth, opt_kinds[i], &g_decode);
    if (!threadbound) {
      needs_close[i] = 1;
      body_depth++;
    }
    // After opening output axis at position i, emit any reduces whose
    // required_pos == i+1.  They depend on the axes opened so far
    // (and only on those), so emitting them between i and i+1
    // avoids redundant recompute inside deeper output loops.
    for (u32 r_i = 0; r_i < n_reduces; r_i++) {
      if (required_pos[r_i] != i + 1) continue;
      RMU_EMIT_ONE_REDUCE(reduces[r_i], body_depth, reduce_simd_flag[r_i]);
    }
  }
  #undef RMU_EMIT_ONE_REDUCE
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

// Walk the DAG rooted at `root` and collect the unique UOP_BUFFER
// terms keyed by `instance` (kernel_lift.c sets instance=0 on the
// output and instance=slot+1 on input slot `slot`).  Slot 0 of the
// returned `slot_bufs[]` array holds the output (instance=0), slot
// k>=1 holds input (k-1).  Returns the highest input slot+1 used,
// i.e. n_inputs.
//
// For lifted kernels every BUFFER carries a structural instance, so
// this walk is the source of truth for the kernel signature: shapes
// + dtypes come from the BUFFER terms themselves, not from any
// external in_bufs[] array.  Tests that build BUFFERs with
// instance==0 throughout (no slot disambiguation) cannot use this
// helper -- they go through the explicit cg_render_uop_kernel(root,
// out_buf, in_bufs[]) entry instead.
//
// Capacity matches RMU_BUF_MAX (32 slots: 1 output + up to 31 inputs,
// matching the Metal buffer-attribute cap).
#define RMU_DISCOVER_MAX RMU_BUF_MAX
static void rmu_discover_bufs_rec(Term t, Term *slot_bufs, u32 *n_inputs_out) {
  if (term_tag(t) != TAG_UOP) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_BUFFER) {
    u32 inst = uop_buffer_inst_get(t);
    if (inst >= RMU_DISCOVER_MAX) return;
    if (slot_bufs[inst] == 0) {
      slot_bufs[inst] = t;
      if (inst >= 1 && inst > *n_inputs_out) *n_inputs_out = inst;
    }
    return;
  }
  // Recurse over operand slots.  Mirrors rmu_collect_ranges' op
  // coverage; conservative -- walks any UOp's heap operands.  Each
  // UOp's heap layout puts operand Terms in successive slots after a
  // small header; we walk a fixed window large enough to cover the
  // widest existing UOp shape (UOP_INDEX_E + UOP_STORE = 3 operands;
  // UOP_REDUCE = 3; UOP_IWHERE = 3; UOP_OPT = 1 + 2 NUM headers).
  // BUFFER terms are leaves so they only ever appear in operand
  // slots, never in header NUM slots.
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 1), slot_bufs, n_inputs_out);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      return;
    case UOP_CAST:  case UOP_BITCAST:
      // [src, NUM(dst_dtype)]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      return;
    case UOP_IWHERE:
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 1), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 2), slot_bufs, n_inputs_out);
      return;
    case UOP_OPT:
      // [target, NUM(kind), NUM(factor)]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      return;
    case UOP_REDUCE:
      // [src, NUM(kind), NUM(axis)]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      return;
    case UOP_STORE:
      // [buf, addr, value]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 1), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 2), slot_bufs, n_inputs_out);
      return;
    case UOP_AFTER:
      // [node, after_node]
      rmu_discover_bufs_rec(heap_read(loc + 0), slot_bufs, n_inputs_out);
      rmu_discover_bufs_rec(heap_read(loc + 1), slot_bufs, n_inputs_out);
      return;
    case UOP_RANGE:
    case UOP_CONST: case UOP_INVALID:
    case UOP_BUFFER:
      return;
    default:
      return;
  }
}

// Render a kernel rooted at `root`.  The root is typically a
// UOP_STORE (single-store kernel) or UOP_AFTER chain (multi-store
// kernel).  `kernel_name` and a list of input buffers + the output
// buffer drive the kernel signature.
//
// This is the legacy entry point retained for synthetic test
// kernels (instance==0 across all BUFFERs) and call sites that
// haven't yet migrated.  Production callers in render_metal.c +
// backend/cpu/jit.c use cg_render_uop_kernel_root() below, which
// discovers buffer slots from the DAG via UOP_BUFFER.instance and
// no longer requires the caller to pass in_bufs[].
fn void cg_render_uop_kernel(Term root, const char *kernel_name,
                             Term out_buf, Term const *in_bufs,
                             u32 n_inputs, FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  // Populate buffer-name map: out_buf -> "out", in_bufs[i] -> "inN".
  // For lifted kernels (every BUFFER has instance != 0 except
  // out_buf) the in_bufs[] entries are ignored at lookup time --
  // rmu_buf_name decodes instance directly.  Registration here is
  // load-bearing only for the synthetic test path.
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
  fputs("    uint tt [[ thread_position_in_threadgroup ]],\n", fp);
  fputs("    uint sgi [[ simdgroup_index_in_threadgroup ]],\n", fp);
  fputs("    uint thread_index_in_simdgroup "
        "[[ thread_index_in_simdgroup ]]) {\n", fp);
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

// Phase C slice 3: structural-mode MSL renderer.  Walks `root` to
// discover every UOP_BUFFER node by `instance` (output at slot 0,
// input at slot k+1).  No `out_buf`/`in_bufs[]` parameters: the
// caller passes the post-lift root (e.g. ke->compute_root /
// ke->cached_lift.store_root) and the renderer derives the kernel
// signature from the DAG itself.
//
// Production callers (cg_emit_via_uop in render_metal.c) use this
// entry; it produces output bit-equal with the legacy entry point
// when invoked on the same root, since rmu_buf_name's structural
// resolution path is identical for input slots and the output is
// registered the same way.
fn void cg_render_uop_kernel_root(Term root, const char *kernel_name,
                                  FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  Term slot_bufs[RMU_DISCOVER_MAX] = {0};
  u32 n_inputs = 0;
  rmu_discover_bufs_rec(root, slot_bufs, &n_inputs);
  Term out_buf = slot_bufs[0];
  rmu_buf_names_reset();
  // Output's instance is 0; rmu_buf_name falls through to the
  // identity map for it.  Inputs are resolved structurally so we
  // don't bother registering them.
  if (out_buf != 0) rmu_buf_names_set(out_buf, "out");
  fputs("#include <metal_stdlib>\n", fp);
  fputs("using namespace metal;\n\n", fp);
  fprintf(fp, "kernel void %s(\n", kernel_name);
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "    device %s *out [[ buffer(0) ]]",
          rmu_msl_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    Term in_buf = slot_bufs[i + 1];
    u32 dt = uop_buffer_dtype(in_buf);
    fprintf(fp, ",\n    device const %s *in%u [[ buffer(%u) ]]",
            rmu_msl_type_name(dt), i, i + 1);
  }
  // kvar wedge: scan the DAG for variable-bound ranges and emit
  // matching `constant uint &V_<name> [[ buffer(K) ]]` args.  The
  // metal_tile_jit_encode dispatcher binds them at the same buffer
  // indices via setBytes:; both sides walk the DAG with
  // kvar_collect_from_dag so the order is stable across calls.
  // Buffer indices land directly after the input buffers
  // (1 + n_inputs ..); kernels that are also conv-shaped today
  // bind their conv cfg at 1+n_inputs in the encoder but the
  // renderer signature omits it, so this slice doesn't have to
  // interleave -- the demo path is non-conv.
  {
    u32 used_vars[KVAR_USED_CAP];
    u32 n_vars = kvar_collect_from_dag(root, used_vars, KVAR_USED_CAP);
    for (u32 i = 0; i < n_vars; i++) {
      const char *vn = kvar_name(used_vars[i]);
      if (vn == NULL) vn = "V";
      fprintf(fp, ",\n    constant uint &V_%s [[ buffer(%u) ]]",
              vn, (u32)(1 + n_inputs + i));
    }
  }
  fputs(",\n    uint tid [[ thread_position_in_grid ]],\n", fp);
  fputs("    uint tg [[ threadgroup_position_in_grid ]],\n", fp);
  fputs("    uint tt [[ thread_position_in_threadgroup ]],\n", fp);
  fputs("    uint sgi [[ simdgroup_index_in_threadgroup ]],\n", fp);
  fputs("    uint thread_index_in_simdgroup "
        "[[ thread_index_in_simdgroup ]]) {\n", fp);
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

// F6: render the same UOp DAG as a C99 kernel for the CPU JIT.
// Buffer-binding convention is the CPU-JIT contract dlsym'd by
// cpu_jit_dispatch: `void k(void *out_v, const void *const *ins_v,
//                           unsigned n, const unsigned *in_numels)`.
// The body emit shares all rmu_emit_* helpers with the MSL path; the
// RMU_TARGET_C flag flips axis-binding (LOCAL/GLOBAL -> for-loop).
// `uint` is typedef'd to `unsigned int` so the body emit's `uint aN`
// / `for (uint a; ...)` patterns compile as C99.
//
// Scope: single-store elementwise / reduce-tail kernels (matmul TC
// template stays MSL-only since C lacks simdgroup_matrix). Caller
// gates on uop_recognise_tc NOT having wrapped the root.
fn void cg_render_uop_kernel_c(Term root, const char *kernel_name,
                               Term out_buf, Term const *in_bufs,
                               u32 n_inputs, FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  rmu_buf_names_reset();
  rmu_buf_names_set(out_buf, "out");
  for (u32 i = 0; i < n_inputs; i++) {
    char name[16];
    snprintf(name, sizeof(name), "in%u", i);
    rmu_buf_names_set(in_bufs[i], name);
  }
  fputs("#include <stdint.h>\n", fp);
  fputs("#include <math.h>\n", fp);
  fputs("#include <string.h>\n", fp);
  fputs("typedef unsigned int uint;\n", fp);
  // UOP_BITCAST renders to THVM_BITCAST(dst, expr); macro expands
  // to a memcpy-based reinterpret. Statement-expression form keeps
  // the use-site syntactically an expression (composes with the
  // surrounding emit). GCC/clang extension; both compilers we
  // target accept it.
  fputs("#define THVM_BITCAST(t, x) "
        "({ t _t; __typeof__(x) _x = (x); "
        "memcpy(&_t, &_x, sizeof(_t)); _t; })\n", fp);
  // CPU-JIT entry-point signature; cpu/jit.c dlsyms "k" and calls
  // it directly with caller pointers.
  fprintf(fp, "void %s(void *out_v, const void *const *ins_v,\n", kernel_name);
  fputs("              unsigned n, const unsigned *in_numels) {\n", fp);
  fputs("  (void)n; (void)in_numels;\n", fp);
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "  %s *out = (%s *)out_v;\n",
          rmu_c_type_name(out_dtype), rmu_c_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    u32 dt = uop_buffer_dtype(in_bufs[i]);
    fprintf(fp, "  const %s *in%u = (const %s *)ins_v[%u];\n",
            rmu_c_type_name(dt), i, rmu_c_type_name(dt), i);
  }
  RMU_TARGET_C = 1;
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
  RMU_TARGET_C = 0;
  fputs("}\n", fp);
}

// Phase C slice 3: structural-mode C99 renderer.  Counterpart of
// cg_render_uop_kernel_root for the CPU JIT path.  Discovers buffer
// slots from `root` via UOP_BUFFER.instance instead of trusting an
// out_buf/in_bufs[] tuple from the caller.  cpu/jit.c uses this to
// pass ke->cached_lift.store_root directly.
fn void cg_render_uop_kernel_c_root(Term root, const char *kernel_name,
                                    FILE *fp) {
  if (fp == NULL) fp = stderr;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  Term slot_bufs[RMU_DISCOVER_MAX] = {0};
  u32 n_inputs = 0;
  rmu_discover_bufs_rec(root, slot_bufs, &n_inputs);
  Term out_buf = slot_bufs[0];
  rmu_buf_names_reset();
  if (out_buf != 0) rmu_buf_names_set(out_buf, "out");
  fputs("#include <stdint.h>\n", fp);
  fputs("#include <math.h>\n", fp);
  fputs("#include <string.h>\n", fp);
  fputs("typedef unsigned int uint;\n", fp);
  fputs("#define THVM_BITCAST(t, x) "
        "({ t _t; __typeof__(x) _x = (x); "
        "memcpy(&_t, &_x, sizeof(_t)); _t; })\n", fp);
  fprintf(fp, "void %s(void *out_v, const void *const *ins_v,\n", kernel_name);
  fputs("              unsigned n, const unsigned *in_numels) {\n", fp);
  fputs("  (void)n; (void)in_numels;\n", fp);
  u32 out_dtype = uop_buffer_dtype(out_buf);
  fprintf(fp, "  %s *out = (%s *)out_v;\n",
          rmu_c_type_name(out_dtype), rmu_c_type_name(out_dtype));
  for (u32 i = 0; i < n_inputs; i++) {
    Term in_buf = slot_bufs[i + 1];
    u32 dt = uop_buffer_dtype(in_buf);
    fprintf(fp, "  const %s *in%u = (const %s *)ins_v[%u];\n",
            rmu_c_type_name(dt), i, rmu_c_type_name(dt), i);
  }
  RMU_TARGET_C = 1;
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
  RMU_TARGET_C = 0;
  fputs("}\n", fp);
}
