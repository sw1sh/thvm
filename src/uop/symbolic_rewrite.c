// uop/symbolic_rewrite.c - port of (a subset of) tinygrad's "sym"
// PatternMatcher family from tinygrad/uop/symbolic.py.
//
// The constructor-time rewrites in uop/rewrite.c + uop/index_simplify.c
// already cover most identity / constant-fold cases at build time, but
// they only fire when a fresh node is *constructed*.  The pipeline
// piece #4/#5 stages (expander -> devectorize -> load_store_fold)
// rebuild ALU around already-hash-consed children -- so an ADD with a
// late-arriving CONST(0) sibling (e.g. accumulator init from
// PLACEHOLDER, GEP-extracted VCONST slot, fold byproduct) never
// re-triggers the constructor simplifier.
//
// `uop_symbolic_rewrite(root)` runs a bottom-up graph rewrite that
// reinvokes the simplifying constructors, plus a handful of extra
// rules that the constructor consciously skips because their tensor-
// level shape semantics could break consumers.  Post-devectorize the
// graph is scalar-per-lane, so those rules are safe.
//
// Ported rules (cf. tinygrad/uop/symbolic.py:76-155 symbolic_simple
// and :222-300 symbolic):
//   * UOp.var(x) + 0          -> x
//   * UOp.var(x) * 1          -> x
//   * UOp.var(x) * 0          -> 0     (scalar-lane only, see below)
//   * CONST(a) op CONST(b)    -> CONST(exec_alu(a, b))   (float + int)
//   * NEG(NEG(x))             -> x
//   * RECIP(RECIP(x))         -> x
//   * GEP(STACK(s_0...), (i,)) -> s_i
//   * STACK(s_0)              -> s_0
//   * Integer: IADD(x, 0), IMUL(x, 1), IMUL(x, 0), constant fold
//     (delegated to uop_simplify_int_binary via uop_int_binary
//     constructor; this pass just re-invokes the constructor on a
//     bottom-up walk so the simplification fires on rebuilt nodes).
//
// Rules NOT yet ported (defer to next sym arc if profile flags them):
//   * (x + c1) + c2 -> x + (c1+c2) for float ADD (int already covered
//     by uop_simplify_int_binary's IADD-of-IADD-with-const fold)
//   * x + (-x) -> 0 / x - x -> 0 for float (only int ISUB covers this)
//   * lt_folding / canonicalize_simplex / gep_through_wmma (symbolic 2.0)
//   * boolean algebra, where folding, cast/bitcast chains
//
// The pipeline calls this AFTER the expander+devectorize sequence (see
// render_uop.c cg_render_uop_kernel_*_root) so the wider graph has been
// reduced to scalar-per-lane shapes before sym runs.

// === Helpers (file-scoped to avoid colliding with uop/rewrite.c) =========

static int sym_is_const(Term t) {
  return term_tag(t) == TAG_UOP && term_ext(t) == UOP_CONST;
}

static u32 sym_const_dtype(Term t) {
  return term_ext(heap_read(term_val(t)));
}

static u32 sym_const_bits(Term t) {
  return (u32)term_val(heap_read(term_val(t)));
}

static int sym_const_is_zero_f32(Term t) {
  if (!sym_is_const(t)) return 0;
  if (sym_const_dtype(t) != DT_FP32) return 0;
  f32 v;
  u32 b = sym_const_bits(t);
  memcpy(&v, &b, sizeof v);
  return v == 0.0f;
}

static int sym_const_is_one_f32(Term t) {
  if (!sym_is_const(t)) return 0;
  if (sym_const_dtype(t) != DT_FP32) return 0;
  f32 v;
  u32 b = sym_const_bits(t);
  memcpy(&v, &b, sizeof v);
  return v == 1.0f;
}

static u32 sym_f32_bits(f32 v) { u32 b; memcpy(&b, &v, sizeof b); return b; }

// === Rules ==================================================================

// Float ADD/MUL with CONST sibling.  Re-invokes uop_binary which routes
// through uop_rewrite_binary (constant fold + ADD-zero / MUL-one).  Plus
// the EXTRA rule the constructor skips: MUL(x, 0.0) -> 0.0 (scalar lane).
//
// Mirror: tinygrad/uop/symbolic.py:78 (UPat.var("x") + 0 -> x),
//         tinygrad/uop/symbolic.py:79 (UPat.var("x") * 1 -> x),
//         tinygrad/uop/symbolic.py:130 (UPat.var("x") * 0 -> 0).
static Term sym_pm_alu_const(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op != UOP_ADD && op != UOP_MUL) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  // Re-attempt the constructor-time rewrite (handles ADD-zero, MUL-one,
  // and CONST*CONST fold).  Returns 0 if no rule matched.
  Term r = uop_rewrite_binary(op, a, b);
  if (r != 0 && r != t) return r;
  // EXTRA: MUL(x, 0.0) -> 0.0.  At tensor level the CONST has shape {1}
  // and the consumer expects x's shape, but post-devectorize each lane
  // is scalar so the broadcast is gone.  Mirrors symbolic.py:130-131.
  if (op == UOP_MUL) {
    if (sym_const_is_zero_f32(b)) return b;
    if (sym_const_is_zero_f32(a)) return a;
  }
  return 0;
}

// Unary self-inverse and constant fold.  uop_rewrite_unary already
// covers NEG(NEG(x)) / RECIP(RECIP(x)) and CONST fold; just reinvoke.
// The walker only calls our rule when the input matches; the rewrite
// returns 0 (no-op) when nothing fires.
//
// Mirror: tinygrad/uop/symbolic.py:110 (Unary on CONST -> CONST).
static Term sym_pm_unary_const(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op != UOP_NEG && op != UOP_RECIP && op != UOP_EXP2 &&
      op != UOP_LOG2 && op != UOP_SQRT) return 0;
  Term s = heap_read(term_val(t) + 0);
  Term r = uop_rewrite_unary(op, s);
  if (r != 0 && r != t) return r;
  return 0;
}

// Integer binary: reinvoke uop_int_binary so uop_simplify_int_binary
// fires (covers IADD-zero, IMUL-zero/-one, constant fold, IADD-IMUL
// distributive over shared x, etc.).
//
// Mirror: tinygrad/uop/symbolic.py:222-300 (symbolic phase 2: combine
// terms, div rules), implemented in uop/index_simplify.c.
static Term sym_pm_int_binary(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op != UOP_IADD && op != UOP_ISUB && op != UOP_IMUL &&
      op != UOP_IDIV && op != UOP_IMOD && op != UOP_ILT  &&
      op != UOP_IAND && op != UOP_IOR  && op != UOP_IXOR) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  Term r = uop_simplify_int_binary(op, a, b);
  if (r != 0 && r != t) return r;
  return 0;
}

// GEP(STACK(s_0, ..., s_{k-1}), (i,)) -> s_i for singleton-index GEPs,
// and GEP(STACK(...), (i_0, ..., i_{n-1})) -> STACK(s_{i_0}, ..., s_{i_{n-1}})
// for multi-index.  Mirrors tinygrad/uop/symbolic.py:196 (STACK . GEP
// rewrite) and tinygrad/codegen/late/devectorizer.py:281-295
// (devec_pm_gep_unwrap).
//
// Also folds GEP(scalar, (0,)) -> scalar when the source is not a
// STACK/UNROLL/VCONST -- catches devectorizer leftovers around per-
// lane scalar LOADs.
static Term sym_pm_gep_stack(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_GEP) return 0;
  u32 n = uop_gep_n_idx(t);
  Term src = heap_read(term_val(t) + 0);
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_STACK) {
    u32 src_n = uop_stack_n(src);
    if (n == 1) {
      u32 idx = uop_gep_idx(t, 0);
      if (idx < src_n) return uop_stack_src(src, idx);
    }
    if (n > 1 && n <= 256) {
      Term out[256];
      for (u32 i = 0; i < n; i++) {
        u32 idx = uop_gep_idx(t, i);
        if (idx >= src_n) return 0;
        out[i] = uop_stack_src(src, idx);
      }
      return uop_stack(n, out);
    }
  }
  if (n == 1 && uop_gep_idx(t, 0) == 0) {
    if (term_tag(src) == TAG_UOP) {
      u32 sop = term_ext(src);
      if (sop != UOP_STACK && sop != UOP_UNROLL && sop != UOP_VCONST) {
        return src;
      }
    } else {
      return src;
    }
  }
  return 0;
}

// STACK with a single source unwraps.  The uop_stack constructor
// already does this for fresh constructions, but bottom-up rebuilds
// can produce a singleton STACK whose elements were folded later.
//
// Mirror: tinygrad/codegen/late/devectorizer.py:316 (STACK singleton).
static Term sym_pm_stack_singleton(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  if (uop_stack_n(t) != 1) return 0;
  return uop_stack_src(t, 0);
}

// === Entry point ============================================================

// One pass of all sym rules.  Each rule returns 0 to mean "no match",
// and the rewriter restarts when any rule fires (graph_rewrite.c
// UOP_GRAPH_REWRITE_MAX_RESTARTS).  Order matters only weakly --
// graph_rewrite re-traverses children after each restart, so a leaf
// fold that exposes a new parent fold catches on the next restart.
fn Term uop_symbolic_rewrite(Term root) {
  static UOpGraphRewriteRule const SYM_RULES[] = {
    { "sym_pm_alu_const",       sym_pm_alu_const       },
    { "sym_pm_unary_const",     sym_pm_unary_const     },
    { "sym_pm_int_binary",      sym_pm_int_binary      },
    { "sym_pm_gep_stack",       sym_pm_gep_stack       },
    { "sym_pm_stack_singleton", sym_pm_stack_singleton },
  };
  return uop_graph_rewrite(root, SYM_RULES,
                           sizeof(SYM_RULES) / sizeof(SYM_RULES[0]),
                           NULL);
}
