// src/aot/emit.c
//
// CPS-transforming AOT emitter (Phase 2 of the Bend-style rewrite).
//
// Walks a TDef'd body in BOOK_HEAP and produces a self-contained
// C source string that, when compiled + dlopen'd via aot/build.c
// (Phase 4), provides the dispatch + per-fn entry points the
// runtime expects:
//
//   FN_<def>          opaque function-id constant per def
//   CONT_<def>_<k>    one per continuation point in the def
//   par_<def>_entry   the def's entry point: dispatches by body
//                     shape, may return AOT_R_VALUE / AOT_R_SPLIT /
//                     AOT_R_CALL
//   par_<def>_cont_<k>   continuations: take the two child results
//                        (+ any captured args) and produce the next
//                        AotResult
//   aot_dispatch      the program-wide switch fn_id -> entry/cont
//
// Phase 2 iter scope (this file grows):
//
//   iter 1 (now)   buffer helpers, top-level scaffolding, emit a
//                  trivial par_<def>_entry that just returns a
//                  constant value.  Proves the output structure
//                  compiles standalone.
//
//   iter 2         walk TDef'd MAT chains, emit if-tree dispatch
//                  on tag/value.  No SPLIT yet (each arm just
//                  returns aot_make_value of a freshly-built term).
//
//   iter 3         identify SPLIT-eligible call sites: sibling
//                  recursive calls in CTR/OP2 args with no data
//                  dep between them.  Emit aot_alloc_cont +
//                  aot_make_split + the cont entry.
//
//   iter 4         emit the eval() sequential variant alongside
//                  par_*.  Bend2's managed-CPS-stack pattern at
//                  par_tree_sum_bend2_compiled.c:336-462.  This is
//                  what closes the perf gap from Phase 1 done-with-
//                  caveats.
//
// What this DOESN'T do (Phase 3 territory):
//   - cross-def direct calls when one def is also AOT'd
//   - independence analysis beyond the simple sibling-call pattern
//   - dup hoisting / OP2 fold / etc. -- those are sequential-path
//     optimisations the legacy emitter had; we'll bring them back
//     once Phase 2 lands an end-to-end pipeline.

#ifndef THVM_AOT_EMIT_INCLUDED
#define THVM_AOT_EMIT_INCLUDED

#include <stdarg.h>

// === Output buffer ==================================================

#define AOT_EMIT_INITIAL_CAP   (1u << 14)   // 16 KB starter

typedef struct {
  char *buf;
  u32   len;
  u32   cap;
} AotEmit;

static void aot_emit_init(AotEmit *e) {
  e->cap = AOT_EMIT_INITIAL_CAP;
  e->len = 0;
  e->buf = (char *)malloc(e->cap);
  if (e->buf) e->buf[0] = '\0';
}

static void aot_emit_grow(AotEmit *e, u32 need) {
  while (e->cap - e->len < need) {
    u32 ncap = e->cap * 2;
    char *nb = (char *)realloc(e->buf, ncap);
    if (!nb) return;
    e->buf = nb;
    e->cap = ncap;
  }
}

static void aot_emit_str(AotEmit *e, const char *s) {
  u32 n = (u32)strlen(s);
  aot_emit_grow(e, n + 1);
  memcpy(e->buf + e->len, s, n);
  e->len += n;
  e->buf[e->len] = '\0';
}

static void aot_emit_fmt(AotEmit *e, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char tmp[1024];
  int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  aot_emit_str(e, tmp);
}

// === Codegen primitives ==============================================
//
// All Term-construction goes through aot_cg_* helpers instead of
// inline aot_emit_fmt strings.  This separates "what does the
// runtime DO at this point" (target-agnostic) from "how does the
// target language render it" (C now, Metal Shading Language later
// in Phase 7).
//
// Each helper takes the output buffer + the C variable name we want
// the constructed Term bound to + the indent string + the tag/ext/
// children specific to the term shape.  Returns the var name
// (passes through `out_var`) so callers can compose.
//
// To target Metal in the future: provide a parallel set of helpers
// that emit MSL syntax (`thread Term name`, atomic heap accesses,
// `device ulong*` heap pointer), keyed off a runtime flag or via
// function-pointer dispatch on an AotCodegen struct.  The TAG
// handlers in aot_emit_value_expr_named call only these helpers,
// so they don't change.

// Atomic term: no heap, fits entirely in the Term u64.
// Examples: TAG_NUM, TAG_ERA, TAG_REF, TAG_PRI, TAG_ANY, TAG_FVR.
static void aot_cg_atomic_term(AotEmit *e, const char *out_var,
                                const char *indent,
                                const char *tag_macro, u32 ext, u64 val) {
  aot_emit_fmt(e,
    "%sTerm %s = term_new(0, %s, %u, %lluu);\n",
    indent, out_var, tag_macro, ext, (unsigned long long)val);
}

// 1-child heap-backed term: heap[loc] = c0; term_new(tag, ext, loc).
// Examples: TAG_DP0, TAG_DP1 (when freshly allocating), TAG_INC.
// The "_loc" suffix on out_var becomes the heap-loc C variable.
static void aot_cg_heap_term1(AotEmit *e, const char *out_var,
                               const char *indent,
                               const char *tag_macro, u32 ext,
                               const char *child0) {
  aot_emit_fmt(e,
    "%su64 %s_loc = aot_heap_alloc(1);\n"
    "%sheap_set(%s_loc, %s);\n"
    "%sTerm %s = term_new(0, %s, %u, %s_loc);\n",
    indent, out_var,
    indent, out_var, child0,
    indent, out_var, tag_macro, ext, out_var);
}

// 2-child heap-backed term: heap[loc..loc+1] = [c0, c1]; term_new.
// Examples: TAG_OP2, TAG_APP, TAG_SUP, TAG_AND, TAG_OR, TAG_EQL,
// TAG_WHEN.
static void aot_cg_heap_term2(AotEmit *e, const char *out_var,
                               const char *indent,
                               const char *tag_macro, u32 ext,
                               const char *child0, const char *child1) {
  aot_emit_fmt(e,
    "%su64 %s_loc = aot_heap_alloc(2);\n"
    "%sheap_set(%s_loc + 0, %s);\n"
    "%sheap_set(%s_loc + 1, %s);\n"
    "%sTerm %s = term_new(0, %s, %u, %s_loc);\n",
    indent, out_var,
    indent, out_var, child0,
    indent, out_var, child1,
    indent, out_var, tag_macro, ext, out_var);
}

// DP* projection of a SHARED dup cell (the dup memo found a prior
// allocation for this source dup_loc).  No new heap_alloc -- just
// reference the existing cell var.
static void aot_cg_dp_share(AotEmit *e, const char *out_var,
                             const char *indent,
                             const char *tag_macro, u32 label,
                             const char *cell_name) {
  aot_emit_fmt(e,
    "%sTerm %s = term_new(0, %s, %u, %s);\n",
    indent, out_var, tag_macro, label, cell_name);
}

// === Top-level: emit a single-def program ===========================
//
// Takes a def_id whose body is in DEFS[def_id], plus a string `name`
// used in symbol-name composition (FN_<name>, par_<name>_entry, ...).
// Returns a malloc'd C source string (caller frees) that:
//
//   - includes <stdint.h>, <stdlib.h>, <pthread.h>, "abi.h"
//     (the abi header lands in Phase 4; for now we comment it out
//     and emit code that only references the AOT runtime types
//     declared in src/aot/task.h + halloc.h directly)
//   - declares FN_<name> = 0
//   - emits par_<name>_entry returning a stub value
//   - emits aot_dispatch as a switch on fn_id
//   - emits aot_program_<name>_register that fills an AotProgram
//     so the host can run it via aot_run_parallel
//
// In iter 1 the body's actual shape is ignored -- the entry returns
// aot_make_value(term_new(0, TAG_NUM, DT_INT32, 0)).  Iters 2+
// replace the body with the real CPS-transformed shape.

// === Per-emit state ==================================================
//
// `AotProgState` carries everything threading through the recursive
// emit:
//   - two output buffers (entry body + cont function defs)
//   - cont_count for assigning fresh CONT_<name>_K ids
//   - self def context (id, arity, name) for self-call recognition
//
// Two buffers exist because cont function definitions need to land
// in the output BEFORE the entry function uses them (or a forward
// decl set lets them land after).  Easier to accumulate them
// separately during the single-pass walk and concatenate at the
// end of thvm_aot_emit_program.

// Dup memo: maps source dup_loc -> emitted C var name for the
// runtime dup cell.  When DP0 and DP1 reference the same source
// dup, both projections point at the SAME runtime cell so the
// dup actually shares (Bend2 semantics).  Without this we'd
// allocate a fresh runtime dup per encounter -- still functionally
// correct, but 2x heap churn per multi-use bound var.
//
// Capacity 32 is well over what any reasonable def needs (each
// dup adds 1 entry; bodies don't usually have >32 distinct dup
// sites).  Linear probe; O(N) lookup is fine at this scale.
#define AOT_DUP_MEMO_CAP  32

typedef struct {
  u64  src_loc;        // source dup_loc (book heap loc of the dup)
  char var_name[24];   // runtime C var name (e.g., "dup_3_loc")
} AotDupMemoEntry;

typedef struct {
  AotEmit         *body;         // entry function body
  AotEmit         *cont_defs;    // par_<name>_cont_K function defs
  u32              cont_count;   // next K to assign
  u32              self_id;
  u32              self_arity;
  const char      *self_name;
  AotDupMemoEntry  dup_memo[AOT_DUP_MEMO_CAP];
  u32              dup_memo_n;
  u32              dup_fresh;    // monotonic counter for dup_K names
} AotProgState;

// Returns the runtime C var name for a previously-seen source
// dup_loc, or NULL if not yet allocated.
static const char *aot_dup_memo_find(AotProgState *st, u64 src_loc) {
  for (u32 i = 0; i < st->dup_memo_n; i++) {
    if (st->dup_memo[i].src_loc == src_loc) {
      return st->dup_memo[i].var_name;
    }
  }
  return NULL;
}

// Records a new (src_loc, var_name) pair.  Caller checks capacity
// via dup_memo_n; over-cap inserts silently drop (callers fall back
// to fresh-per-encounter semantics, which is correct just less
// efficient).
static void aot_dup_memo_push(AotProgState *st, u64 src_loc,
                              const char *var_name) {
  if (st->dup_memo_n >= AOT_DUP_MEMO_CAP) return;
  st->dup_memo[st->dup_memo_n].src_loc = src_loc;
  snprintf(st->dup_memo[st->dup_memo_n].var_name,
           sizeof st->dup_memo[st->dup_memo_n].var_name,
           "%s", var_name);
  st->dup_memo_n++;
}

// === Bindings ========================================================
//
// Each TLam binder we peel records (lam_loc, "args[K]") so a TVar
// reference inside the body can resolve to the C parameter name.
// MAT-CTR destructure (iter 4+) will also push bindings of the form
// (lam_loc, "term_ctr_at(arg, K)") for the destructured fields.

#define AOT_BIND_CAP  32

typedef struct {
  u64  lam_loc;
  // Value expression to inline at TVar lookup sites.  Top-level
  // LAM peels: "args[K]" or "term_ctr_at(dv, K)".  Value-position
  // LAM (HOFs): "term_new(0, TAG_VAR, 0, lam_K_loc)" (~36 chars
  // with 1-2 digit K, larger with more).  64 leaves ample margin.
  char name[64];
} AotBinding;

typedef struct {
  AotBinding entries[AOT_BIND_CAP];
  u32        n;
} AotBindings;

static void aot_bind_init(AotBindings *b) { b->n = 0; }

static void aot_bind_push(AotBindings *b, u64 lam_loc, const char *name) {
  if (b->n >= AOT_BIND_CAP) return;
  b->entries[b->n].lam_loc = lam_loc;
  snprintf(b->entries[b->n].name, sizeof(b->entries[b->n].name),
           "%s", name);
  b->n++;
}

// Lookup most-recent matching binding (allows shadowing); NULL if
// the loc isn't bound (signals an emit-time bug).
static const char *aot_bind_lookup(AotBindings *b, u64 lam_loc) {
  for (u32 i = b->n; i > 0; i--) {
    if (b->entries[i - 1].lam_loc == lam_loc) {
      return b->entries[i - 1].name;
    }
  }
  return NULL;
}

// Peel outer TAG_LAM binders from `root` and push (lam_loc,
// "t->args[K]") bindings for each.  Returns the cursor at the
// post-LAM body and writes `*out_argc` with the peel count.
static Term aot_peel_lams(Term root, u32 *out_argc, AotBindings *b) {
  Term cursor = root;
  u32 n = 0;
  while (term_tag(cursor) == TAG_LAM) {
    char nm[24];
    snprintf(nm, sizeof nm, "t->args[%u]", n);
    aot_bind_push(b, term_val(cursor), nm);
    cursor = book_read(term_val(cursor));
    n++;
  }
  *out_argc = n;
  return cursor;
}

// === Value-expression emit ==========================================
//
// Translates a Term in BOOK_HEAP into a C expression that, when
// evaluated at runtime, produces the same Term (allocated via the
// thread-local AOT bump allocator).  Output is appended to `e`'s
// buffer as one or more `Term v_N = ...;` declarations followed by
// the final `out_var` name.  Caller passes `out_var` (e.g. "v_0")
// and `indent`.
//
// Coverage in iter 3: TNum, TVar, TRef, TCtr (arity 0/1/2).  Other
// tags emit a stub `term_new(0, TAG_ERA, 0, 0)` so we don't
// silently drop them; a comment in the output flags what was
// stubbed for the next iter to pick up.
static u32 aot_value_fresh = 0;

// Per-emit state pointer.  Set at the top of thvm_aot_emit_program;
// read by value-expr emit for the TAG_DP0/DP1 case so it can share
// runtime dup cells across DP0/DP1 of the same source dup_loc
// (Bend2-style real sharing instead of fresh-per-encounter copies).
// Threading this through every value-expr call site would touch ~10
// signatures; a static pointer mirrors how aot_value_fresh works.
static AotProgState *aot_current_st = NULL;

static const char *aot_emit_value_expr(AotEmit *e, Term term,
                                       AotBindings *bind,
                                       const char *indent);

static const char *aot_emit_value_expr_named(AotEmit *e, Term term,
                                             AotBindings *bind,
                                             const char *indent,
                                             char *out_buf, u32 out_cap) {
  u32 idx = aot_value_fresh++;
  snprintf(out_buf, out_cap, "v_%u", idx);

  u8 tag = term_tag(term);
  switch (tag) {
    case TAG_NUM: {
      aot_cg_atomic_term(e, out_buf, indent, "TAG_NUM",
                          term_ext(term), term_val(term));
      return out_buf;
    }
    case TAG_VAR: {
      // Iter 10: inline TAG_VAR refs.  The bound name (e.g.,
      // "t->args[0]" or "term_ctr_at(dv, 0)") is itself a valid C
      // expression -- copy it into the caller's buffer and skip
      // the `Term v_K = ...;` indirection.  Eliminates ~30% of the
      // emit's named-temp churn on count-shaped bodies, cutting
      // the AOT-vs-handcoded perf gap noted in iter 9.
      u64 loc = term_val(term);
      const char *bound = aot_bind_lookup(bind, loc);
      if (bound != NULL) {
        // Fits per AotBinding.name being [40]; ring slot is [64].
        snprintf(out_buf, out_cap, "%s", bound);
      } else {
        aot_emit_fmt(e,
          "%sTerm %s = term_new(0, TAG_ERA, 0, 0);"
                "  /* unbound TVar(%llu) -- emit-time bug */\n",
          indent, out_buf, (unsigned long long)loc);
      }
      return out_buf;
    }
    case TAG_REF: {
      aot_cg_atomic_term(e, out_buf, indent, "TAG_REF", term_ext(term), 0);
      return out_buf;
    }
    case TAG_ERA: {
      // Inline: TAG_ERA carries no payload, so emit the constant
      // expression directly (caller embeds it; no Term v_K = ...
      // declaration needed).
      snprintf(out_buf, out_cap, "term_new(0, TAG_ERA, 0, 0)");
      return out_buf;
    }
    case TAG_DP0:
    case TAG_DP1: {
      // Auto-dup of multiply-used LAM args: source uses DP0 / DP1
      // of a heap-side dup cell to share one body across multiple
      // consumers.  Memo source dup_loc -> runtime cell var so DP0
      // and DP1 of the same source share ONE runtime dup cell
      // (Bend2 semantics).  First encounter allocates + stores the
      // body; subsequent encounters just re-reference the cell.
      u64 dup_loc = term_val(term);
      u32 label   = term_ext(term);
      const char *tag_macro = (tag == TAG_DP0) ? "TAG_DP0" : "TAG_DP1";
      const char *cell_name = (aot_current_st != NULL)
          ? aot_dup_memo_find(aot_current_st, dup_loc) : NULL;
      if (cell_name != NULL) {
        // Sibling DP-projection of the same source dup_loc: reuse cell.
        aot_cg_dp_share(e, out_buf, indent, tag_macro, label, cell_name);
        return out_buf;
      }
      // First encounter: emit body, allocate runtime dup cell, memo.
      // Use a custom name so DP0 / DP1 with shared sources later can
      // re-reference (aot_cg_heap_term1 names cell as "<out>_loc",
      // which differs across DP0 / DP1 calls -- breaks the share).
      Term body = book_read(dup_loc);
      char body_buf[64];
      const char *body_var = aot_emit_value_expr_named(
          e, body, bind, indent, body_buf, sizeof body_buf);
      char cell_buf[24];
      if (aot_current_st != NULL) {
        snprintf(cell_buf, sizeof cell_buf, "dup_%u_loc",
                 aot_current_st->dup_fresh++);
        aot_dup_memo_push(aot_current_st, dup_loc, cell_buf);
      } else {
        snprintf(cell_buf, sizeof cell_buf, "%s_loc", out_buf);
      }
      aot_emit_fmt(e,
        "%su64 %s = aot_heap_alloc(1);\n"
        "%sheap_set(%s, %s);\n"
        "%sTerm %s = term_new(0, %s, %u, %s);\n",
        indent, cell_buf,
        indent, cell_buf, body_var,
        indent, out_buf, tag_macro, label, cell_buf);
      return out_buf;
    }
    case TAG_CTR: {
      u32 label = term_ext(term);
      u64 loc   = term_val(term);
      Term n_cell = book_read(loc);
      u32  n      = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0;
      if (n == 0) {
        aot_emit_fmt(e, "%sTerm %s = aot_make_ctr0(%u);\n",
                     indent, out_buf, label);
        return out_buf;
      }
      if (n == 1) {
        char child_buf[24];
        const char *cv = aot_emit_value_expr_named(e, book_read(loc + 1),
                                                    bind, indent,
                                                    child_buf, sizeof child_buf);
        aot_emit_fmt(e, "%sTerm %s = aot_make_ctr1(%u, %s);\n",
                     indent, out_buf, label, cv);
        return out_buf;
      }
      if (n == 2) {
        char b0[24], b1[24];
        const char *c0 = aot_emit_value_expr_named(e, book_read(loc + 1),
                                                    bind, indent,
                                                    b0, sizeof b0);
        const char *c1 = aot_emit_value_expr_named(e, book_read(loc + 2),
                                                    bind, indent,
                                                    b1, sizeof b1);
        aot_emit_fmt(e, "%sTerm %s = aot_make_ctr2(%u, %s, %s);\n",
                     indent, out_buf, label, c0, c1);
        return out_buf;
      }
      if (n == 3) {
        char b0[24], b1[24], b2[24];
        const char *c0 = aot_emit_value_expr_named(e, book_read(loc + 1),
                                                    bind, indent, b0, sizeof b0);
        const char *c1 = aot_emit_value_expr_named(e, book_read(loc + 2),
                                                    bind, indent, b1, sizeof b1);
        const char *c2 = aot_emit_value_expr_named(e, book_read(loc + 3),
                                                    bind, indent, b2, sizeof b2);
        aot_emit_fmt(e, "%sTerm %s = aot_make_ctr3(%u, %s, %s, %s);\n",
                     indent, out_buf, label, c0, c1, c2);
        return out_buf;
      }
      if (n == 4) {
        char b0[24], b1[24], b2[24], b3[24];
        const char *c0 = aot_emit_value_expr_named(e, book_read(loc + 1),
                                                    bind, indent, b0, sizeof b0);
        const char *c1 = aot_emit_value_expr_named(e, book_read(loc + 2),
                                                    bind, indent, b1, sizeof b1);
        const char *c2 = aot_emit_value_expr_named(e, book_read(loc + 3),
                                                    bind, indent, b2, sizeof b2);
        const char *c3 = aot_emit_value_expr_named(e, book_read(loc + 4),
                                                    bind, indent, b3, sizeof b3);
        aot_emit_fmt(e, "%sTerm %s = aot_make_ctr4(%u, %s, %s, %s, %s);\n",
                     indent, out_buf, label, c0, c1, c2, c3);
        return out_buf;
      }
      // arity > 4: emit generic aot_make_ctrn with a stack-allocated
      // children array.  Capped at the AotTask MAX_ARGS-equivalent
      // 16 (matches HVM4's CTR limit, declared in src/term/new_ctr.c).
      if (n <= 16) {
        aot_emit_fmt(e, "%s{ Term __ctr_kids_%u[%u] = {",
                     indent, aot_value_fresh, n);
        for (u32 i = 0; i < n; i++) {
          char bi[24];
          const char *ci = aot_emit_value_expr_named(
              e, book_read(loc + 1 + i), bind, indent, bi, sizeof bi);
          aot_emit_fmt(e, "%s%s", (i == 0 ? "" : ", "), ci);
        }
        aot_emit_fmt(e,
          "};\n"
          "%s  Term %s = aot_make_ctrn(%u, %u, __ctr_kids_%u); }\n",
          indent, out_buf, label, n, aot_value_fresh - 1);
        return out_buf;
      }
      // arity > 16: not supported (HVM4 cap).  Stub fallback.
      aot_emit_fmt(e,
        "%sTerm %s = term_new(0, TAG_ERA, 0, 0);"
              "  /* CTR arity %u exceeds 16 (HVM4 cap) */\n",
        indent, out_buf, n);
      return out_buf;
    }
    case TAG_OP2: {
      // Build a heap-backed TAG_OP2.  Used when OP2 appears as a
      // sub-expression rather than as the outer node of a saturated
      // self-call SPLIT (which aot_match_split_op2 catches earlier).
      // Common case: passing `n - 1` as the arg of a recursive call,
      // e.g. fib(n-1).  The receiving dispatch's wnf-fast-path forces
      // it to NUM before tag-checking, so semantics line up with the
      // interpreter.
      u32 op  = term_ext(term);
      u64 loc = term_val(term);
      char b0[64], b1[64];
      const char *lv = aot_emit_value_expr_named(
          e, book_read(loc + 0), bind, indent, b0, sizeof b0);
      const char *rv = aot_emit_value_expr_named(
          e, book_read(loc + 1), bind, indent, b1, sizeof b1);
      aot_cg_heap_term2(e, out_buf, indent, "TAG_OP2", op, lv, rv);
      return out_buf;
    }
    case TAG_PRI: {
      // Atomic primitive function reference (no heap).  Used as the
      // function of an APP chain that builds a TPri redex; the
      // runtime reduces the chain at fire time via PRIM_TABLE.
      // Phase 6 iter A registers core prims in the dylib at run().
      aot_cg_atomic_term(e, out_buf, indent, "TAG_PRI", term_ext(term), 0);
      return out_buf;
    }
    case TAG_ANY:
    case TAG_FVR: {
      // Atomic combinators -- no heap.  TAG_ANY is the wildcard
      // (matches anything under EQL); TAG_FVR is a first-order
      // logic variable (ext = variable id).
      aot_cg_atomic_term(e, out_buf, indent,
                          tag == TAG_ANY ? "TAG_ANY" : "TAG_FVR",
                          term_ext(term), 0);
      return out_buf;
    }
    case TAG_INC: {
      // Priority wrapper `inc{body}`: 1 child at heap[loc].
      // Observed by collapse_ordered to bias which side fires
      // first under non-determinism.
      u64 loc = term_val(term);
      char b0[64];
      const char *bv = aot_emit_value_expr_named(
          e, book_read(loc), bind, indent, b0, sizeof b0);
      aot_cg_heap_term1(e, out_buf, indent, "TAG_INC", 0, bv);
      return out_buf;
    }
    case TAG_AND:
    case TAG_OR:
    case TAG_EQL:
    case TAG_WHEN: {
      // 2-child heap-backed combinators.  AND/OR are short-circuit,
      // EQL is strict-eq, WHEN is a boolean filter.  All four reduce
      // via wnf when force-strict on their args; the AOT'd code
      // just builds them and lets the runtime descend.
      u64 loc = term_val(term);
      char b0[64], b1[64];
      const char *av = aot_emit_value_expr_named(
          e, book_read(loc + 0), bind, indent, b0, sizeof b0);
      const char *bv = aot_emit_value_expr_named(
          e, book_read(loc + 1), bind, indent, b1, sizeof b1);
      const char *tn = (tag == TAG_AND ? "TAG_AND"
                       : tag == TAG_OR  ? "TAG_OR"
                       : tag == TAG_EQL ? "TAG_EQL"
                       :                  "TAG_WHEN");
      aot_cg_heap_term2(e, out_buf, indent, tn, 0, av, bv);
      return out_buf;
    }
    case TAG_SUP: {
      // Superposition `&L{a, b}`: 2-child heap-backed term with a
      // label in ext.  Used by IC's non-linear duplication machinery
      // and explicit ATP-style search (each branch is one alternative).
      u32 label = term_ext(term);
      u64 loc   = term_val(term);
      char b0[64], b1[64];
      const char *av = aot_emit_value_expr_named(
          e, book_read(loc + 0), bind, indent, b0, sizeof b0);
      const char *bv = aot_emit_value_expr_named(
          e, book_read(loc + 1), bind, indent, b1, sizeof b1);
      aot_cg_heap_term2(e, out_buf, indent, "TAG_SUP", label, av, bv);
      return out_buf;
    }
    case TAG_LAM: {
      // Value-position lambda (HOF return).  Emit:
      //   u64  lam_K_loc = aot_heap_alloc(1);
      //   <body emit -- references to the binder produce
      //                 term_new(0, TAG_VAR, 0, lam_K_loc)>
      //   heap_set(lam_K_loc, body_var);
      //   Term v_K = term_new(0, TAG_LAM, 0, lam_K_loc);
      //
      // Order matters: the heap alloc + binding happen BEFORE the
      // body emit so the body's TVar references can resolve to the
      // synthesised loc var.  heap_set runs AFTER, since we need
      // the body's value first.  Standard lambda-emit pattern.
      u64 lam_loc = term_val(term);
      Term body   = book_read(lam_loc);
      u32 lam_k   = aot_value_fresh++;
      char loc_name[24];
      snprintf(loc_name, sizeof loc_name, "lam_%u_loc", lam_k);
      char var_expr[64];
      snprintf(var_expr, sizeof var_expr,
               "term_new(0, TAG_VAR, 0, %s)", loc_name);
      aot_emit_fmt(e,
        "%su64 %s = aot_heap_alloc(1);\n",
        indent, loc_name);
      u32 saved_bind_n = bind->n;
      aot_bind_push(bind, lam_loc, var_expr);
      char body_buf[64];
      const char *bv = aot_emit_value_expr_named(
          e, body, bind, indent, body_buf, sizeof body_buf);
      bind->n = saved_bind_n;
      aot_emit_fmt(e,
        "%sheap_set(%s, %s);\n"
        "%sTerm %s = term_new(0, TAG_LAM, 0, %s);\n",
        indent, loc_name, bv,
        indent, out_buf, loc_name);
      return out_buf;
    }
    case TAG_APP: {
      // Build a heap-backed TAG_APP.  Like TAG_OP2 above: outer
      // App-of-Mat / saturated-self-call patterns match earlier;
      // this is the fallback for nested apps in sub-expressions.
      u64 loc = term_val(term);
      char b0[64], b1[64];
      const char *fv = aot_emit_value_expr_named(
          e, book_read(loc + 0), bind, indent, b0, sizeof b0);
      const char *av = aot_emit_value_expr_named(
          e, book_read(loc + 1), bind, indent, b1, sizeof b1);
      aot_cg_heap_term2(e, out_buf, indent, "TAG_APP", 0, fv, av);
      return out_buf;
    }
    default: {
      aot_emit_fmt(e,
        "%sTerm %s = term_new(0, TAG_ERA, 0, 0);"
              "  /* unsupported tag %u in value position */\n",
        indent, out_buf, tag);
      return out_buf;
    }
  }
}

// Convenience wrapper: caller doesn't need to allocate a name
// buffer.  Returns a pointer into a small per-call ring so calls
// can be nested without buffer aliasing concerns; safe for chained
// recursive emits since each call gets a fresh slot.
//
// Slot size 64 is enough to fit bound names like
// "term_ctr_at(some_arg_name, 0)" which TAG_VAR inlining (iter 10)
// returns directly instead of going through a named v_K temp.
static const char *aot_emit_value_expr(AotEmit *e, Term term,
                                       AotBindings *bind,
                                       const char *indent) {
  static char ring[256][64];
  static u32  ring_idx = 0;
  char *slot = ring[(ring_idx++) & 0xFF];
  return aot_emit_value_expr_named(e, term, bind, indent, slot, 64);
}

// === Self-call detection ============================================
//
// A saturated self-call is `App(App(...App(TRef[self_id], a_0),
// a_1), ..., a_{N-1})` with N == self_arity.  Walk the App-chain
// from the outside in, collecting args in reverse order; the
// innermost fun must be TRef pointing at the def we're emitting.
//
// Returns the count of args collected (== arity on a match) or 0
// otherwise.  out_args[] receives the arg Terms in CALL order
// (a_0 first, ..., a_{N-1} last).

#define AOT_MAX_CALL_ARGS  AOT_MAX_ARGS

static u32 aot_match_self_call(Term term, u32 self_id, u32 self_arity,
                               Term *out_args) {
  if (self_arity == 0 || self_arity > AOT_MAX_CALL_ARGS) return 0;
  Term cursor = term;
  Term args_rev[AOT_MAX_CALL_ARGS];
  u32 n = 0;
  while (term_tag(cursor) == TAG_APP) {
    if (n >= self_arity) return 0;
    u64 loc = term_val(cursor);
    args_rev[n++] = book_read(loc + 1);
    cursor = book_read(loc + 0);
  }
  if (n != self_arity) return 0;
  if (term_tag(cursor) != TAG_REF) return 0;
  if (term_ext(cursor) != self_id) return 0;
  // Reverse into call order.
  for (u32 i = 0; i < n; i++) out_args[i] = args_rev[n - 1 - i];
  return n;
}

// === Sibling-pair SPLIT detection ===================================
//
// Two patterns we recognise as fork candidates in iter 5:
//
//   TCtr2(L, sibling_call_0, sibling_call_1)
//       node{call0, call1}-style construction.  Cont reassembles
//       both child results into TCtr2(L, c0, c1).
//
//   TOp2(op, sibling_call_0, sibling_call_1)
//       fold-style.  Cont takes both NUM children and folds via
//       the given op.  Iter 5b -- this iter does TCtr2 only.
//
// Each sibling must be a saturated self-call (matched via
// aot_match_self_call from iter 4).  For mismatched / asymmetric
// patterns we fall back to value-expr emit.
//
// Returns 1 if we emitted a SPLIT path; 0 if the caller should
// fall back to value-expr emit.

static u32 aot_match_split_ctr2(Term term, AotProgState *st,
                                u32 *out_label,
                                Term *out_call_0_args,
                                Term *out_call_1_args) {
  if (term_tag(term) != TAG_CTR) return 0;
  u32 label = term_ext(term);
  u64 loc   = term_val(term);
  Term n_cell = book_read(loc);
  if (term_tag(n_cell) != TAG_NUM) return 0;
  if ((u32)term_val(n_cell) != 2) return 0;   // arity 2 only

  Term c0 = book_read(loc + 1);
  Term c1 = book_read(loc + 2);

  if (aot_match_self_call(c0, st->self_id, st->self_arity,
                          out_call_0_args) != st->self_arity) return 0;
  if (aot_match_self_call(c1, st->self_id, st->self_arity,
                          out_call_1_args) != st->self_arity) return 0;

  *out_label = label;
  return 1;
}

// Emit a fresh par_<self>_cont_K function into st->cont_defs that
// reassembles two child Terms into TCtr2(label, args[0], args[1]).
// Returns the K so the call site can reference CONT_<self>_K.
static u32 aot_emit_ctr2_cont(AotProgState *st, u32 label) {
  u32 k = st->cont_count++;
  aot_emit_fmt(st->cont_defs,
    "static AotResult par_%s_cont_%u(AotProgram *p, AotTask *t) {\n"
    "  (void)p;\n"
    "  return aot_make_value(aot_make_ctr2(%uu, t->args[0], t->args[1]));\n"
    "}\n\n",
    st->self_name, k, label);
  return k;
}

// Match TOp2(op, sibling_call_0, sibling_call_1) -- the
// sum-style-add pattern.  Cont folds the two NUM children via the
// op (constant-folded at emit time).
//
// Returns 1 on a match, fills *out_op + the two arg arrays.
static u32 aot_match_split_op2(Term term, AotProgState *st,
                               u32 *out_op,
                               Term *out_call_0_args,
                               Term *out_call_1_args) {
  if (term_tag(term) != TAG_OP2) return 0;
  u32 op  = term_ext(term);
  u64 loc = term_val(term);
  Term c0 = book_read(loc + 0);
  Term c1 = book_read(loc + 1);

  if (aot_match_self_call(c0, st->self_id, st->self_arity,
                          out_call_0_args) != st->self_arity) return 0;
  if (aot_match_self_call(c1, st->self_id, st->self_arity,
                          out_call_1_args) != st->self_arity) return 0;

  *out_op = op;
  return 1;
}

// Emit a fresh par_<self>_cont_K function into st->cont_defs that
// folds two NUM child results via `op`.  Returns the K so the call
// site can reference CONT_<self>_K.
//
// The op's C expression is constant-folded at emit time -- no
// runtime switch on the op id, no lookup table.
static const char *aot_op2_c_expr(u32 op) {
  switch (op) {
    case OP_ADD: return "lv + rv";
    case OP_SUB: return "lv - rv";
    case OP_MUL: return "lv * rv";
    case OP_EQ:  return "(lv == rv) ? 1u : 0u";
    case OP_LT:  return "(lv <  rv) ? 1u : 0u";
    default:     return "0u  /* unsupported OP2 op */";
  }
}

static u32 aot_emit_op2_cont(AotProgState *st, u32 op) {
  u32 k = st->cont_count++;
  aot_emit_fmt(st->cont_defs,
    "static AotResult par_%s_cont_%u(AotProgram *p, AotTask *t) {\n"
    "  (void)p;\n"
    "  /* OP2 fold (op id %u) -- assumes both child results are NUM */\n"
    "  u32 lv = (u32)term_val(t->args[0]);\n"
    "  u32 rv = (u32)term_val(t->args[1]);\n"
    "  return aot_make_value(\n"
    "      term_new(0, TAG_NUM, term_ext(t->args[0]), %s));\n"
    "}\n\n",
    st->self_name, k, op, aot_op2_c_expr(op));
  return k;
}

// Emit the SPLIT call-site code into st->body: alloc cont, build
// two child tasks via FN_<self>, return aot_make_split.  Generic
// over cont type (TCtr2 / TOp2 / future) -- only differs from a
// regular call site in routing slot 0/1 returns into the cont.
static void aot_emit_split_call(AotProgState *st, u32 cont_k,
                                Term *call_0_args, Term *call_1_args,
                                AotBindings *bind, const char *indent) {
  // Emit each call's arg expressions to fresh v_K vars.  CSE: when
  // both calls reference the SAME source Term (common in count(p,p)
  // / tree_sum-on-l-and-r where the SPLIT args inline a peeled
  // binder like term_ctr_at(dv, 0)), emit it once and reuse the
  // var name.  Saves a redundant heap_read per fire on hot paths.
  const char *a0_names[AOT_MAX_CALL_ARGS];
  const char *a1_names[AOT_MAX_CALL_ARGS];
  for (u32 i = 0; i < st->self_arity; i++) {
    a0_names[i] = aot_emit_value_expr(st->body, call_0_args[i],
                                       bind, indent);
  }
  for (u32 i = 0; i < st->self_arity; i++) {
    if (call_0_args[i] == call_1_args[i]) {
      // Same Term -- reuse the var name from the first call.
      a1_names[i] = a0_names[i];
      continue;
    }
    a1_names[i] = aot_emit_value_expr(st->body, call_1_args[i],
                                       bind, indent);
  }

  aot_emit_fmt(st->body,
    "%su64 dp = aot_alloc_cont(CONT_%s_%u, 0, t->ret);\n",
    indent, st->self_name, cont_k);
  aot_emit_fmt(st->body,
    "%sreturn aot_make_split(\n"
    "%s    aot_make_task(FN_%s, aot_enc_ret((u32)dp, 0)",
    indent,
    indent, st->self_name);
  for (u32 i = 0; i < st->self_arity; i++) {
    aot_emit_fmt(st->body, ", %s", a0_names[i]);
  }
  for (u32 i = st->self_arity; i < AOT_MAX_ARGS; i++) {
    aot_emit_fmt(st->body, ", 0");
  }
  aot_emit_fmt(st->body, "),\n%s    aot_make_task(FN_%s, aot_enc_ret((u32)dp, 1)",
               indent, st->self_name);
  for (u32 i = 0; i < st->self_arity; i++) {
    aot_emit_fmt(st->body, ", %s", a1_names[i]);
  }
  for (u32 i = st->self_arity; i < AOT_MAX_ARGS; i++) {
    aot_emit_fmt(st->body, ", 0");
  }
  aot_emit_fmt(st->body, "));\n");
}

// === Arm-handler emit ================================================
//
// Handler shapes recognised:
//
//   TLam[v_0, ..., TLam[v_K, body]]
//                  -- multi-binder destructure for CTR-arms.  Each
//                     v_i binds to term_ctr_at(arg_var, i); body
//                     is the post-peel expression.  NUM-arms peel
//                     a single LAM with v binding to arg_var.
//
//   saturated self-call           -- aot_make_call (R_CALL).
//   sibling-pair TCtr2(call,call) -- aot_alloc_cont +
//                                    aot_make_split (R_SPLIT) +
//                                    fresh par_<self>_cont_K fn.
//   anything else                 -- aot_make_value(value_expr).
//
// Iter 5b will add TOp2(op, call, call) sibling pattern (sum-style
// add).  Other patterns continue to fall back to value_expr.
// Forward decl: arm-handler can recurse into nested App-of-Mat
// dispatch (defined further down in this file).
static u32 aot_emit_app_of_mat_named(AotProgState *st, Term cursor,
                                      AotBindings *bind,
                                      const char *dv_name,
                                      const char *indent);

static void aot_emit_arm_handler(AotProgState *st, Term handler,
                                 const char *arg_var, u32 is_ctr_arm,
                                 AotBindings *bind, const char *indent) {
  Term cursor = handler;
  u32 saved_n = bind->n;
  // Each arm is wrapped in its own `if (...) { ... }` block by the
  // mat-chain dispatcher (or runs as the default tail).  Cells
  // allocated inside one arm aren't visible to siblings, so reset
  // the dup memo at arm entry.  dup_fresh stays monotonic so cell
  // names don't collide if a future caller re-emits across arms.
  u32 saved_dup_memo_n = st->dup_memo_n;
  st->dup_memo_n = 0;

  // Peel binders.
  //
  // CTR arm (kind=1):  MAT-CTR applies the handler positionally to
  //                    the CTR's children.  Peel each LAM binding
  //                    the binder loc to term_ctr_at(arg_var, i).
  //
  // NUM arm (kind=0):  MAT-NUM returns the handler AS-IS -- NUM has
  //                    no children to apply to.  Don't peel; emit
  //                    handler as a plain value expression.
  //
  // Default arm (kind=2): MAT-MIS applies the fallback to the
  //                    matched value.  Peel one LAM binding to
  //                    arg_var; treat additional LAMs as part of
  //                    the body (HOF return).
  u32 peel_idx = 0;
  if (is_ctr_arm == 1) {
    while (term_tag(cursor) == TAG_LAM) {
      char destr[64];
      snprintf(destr, sizeof destr,
               "term_ctr_at(%s, %u)", arg_var, peel_idx);
      aot_bind_push(bind, term_val(cursor), destr);
      cursor = book_read(term_val(cursor));
      peel_idx++;
    }
  } else if (is_ctr_arm == 2 && term_tag(cursor) == TAG_LAM) {
    aot_bind_push(bind, term_val(cursor), arg_var);
    cursor = book_read(term_val(cursor));
  }

  // Sibling-pair TCtr2(call, call)?  Emit SPLIT + cont.
  u32 ctr2_label;
  u32 op2_op;
  Term call_0_args[AOT_MAX_CALL_ARGS];
  Term call_1_args[AOT_MAX_CALL_ARGS];

  if (aot_match_split_ctr2(cursor, st, &ctr2_label,
                            call_0_args, call_1_args)) {
    u32 cont_k = aot_emit_ctr2_cont(st, ctr2_label);
    aot_emit_split_call(st, cont_k, call_0_args, call_1_args,
                         bind, indent);
    bind->n = saved_n;
    st->dup_memo_n = saved_dup_memo_n;
    return;
  }

  if (aot_match_split_op2(cursor, st, &op2_op,
                           call_0_args, call_1_args)) {
    u32 cont_k = aot_emit_op2_cont(st, op2_op);
    aot_emit_split_call(st, cont_k, call_0_args, call_1_args,
                         bind, indent);
    bind->n = saved_n;
    st->dup_memo_n = saved_dup_memo_n;
    return;
  }

  // Saturated self-call?  Emit aot_make_call.
  Term call_args[AOT_MAX_CALL_ARGS];
  u32 n = aot_match_self_call(cursor, st->self_id, st->self_arity,
                              call_args);
  if (n > 0) {
    const char *arg_names[AOT_MAX_CALL_ARGS];
    for (u32 i = 0; i < n; i++) {
      arg_names[i] = aot_emit_value_expr(st->body, call_args[i],
                                          bind, indent);
    }
    aot_emit_fmt(st->body,
      "%sreturn aot_make_call(aot_make_task(\n"
      "%s    FN_%s, t->ret",
      indent, indent, st->self_name);
    for (u32 i = 0; i < n; i++) {
      aot_emit_fmt(st->body, ", %s", arg_names[i]);
    }
    for (u32 i = n; i < AOT_MAX_ARGS; i++) {
      aot_emit_fmt(st->body, ", 0");
    }
    aot_emit_fmt(st->body, "));\n");
    bind->n = saved_n;
    st->dup_memo_n = saved_dup_memo_n;
    return;
  }

  // Inner App-of-Mat dispatch.  Common pattern: outer match's
  // default arm body is itself an App-of-MatChain on another
  // bound var (e.g. ack: outer dispatch on m, default-arm body is
  // App(inner_mat, n)).  Recurse with a fresh dv name so the
  // nested switch doesn't shadow the outer `dv`.
  static u32 nested_counter = 0;
  char dv2_name[16];
  snprintf(dv2_name, sizeof dv2_name, "dv%u", ++nested_counter);
  if (aot_emit_app_of_mat_named(st, cursor, bind, dv2_name, indent)) {
    bind->n = saved_n;
    st->dup_memo_n = saved_dup_memo_n;
    return;
  }

  // Default: emit as a value expression.  When cursor is a tag
  // that isn't already in WHNF (e.g. TAG_APP from a cross-def call
  // like `f(v)` in a leaf arm, TAG_OP2 from arithmetic, TAG_REF),
  // wrap the result with a force so the parent SPLIT cont / caller
  // sees a NUM/CTR rather than an unreduced term.  Atomic tags
  // (NUM/CTR/LAM/SUP/...) skip the force -- already WHNF.
  const char *rv = aot_emit_value_expr(st->body, cursor, bind, indent);
  u8 cursor_tag = term_tag(cursor);
  _Bool needs_force = (cursor_tag == TAG_APP || cursor_tag == TAG_OP2 ||
                       cursor_tag == TAG_REF || cursor_tag == TAG_MAT ||
                       cursor_tag == TAG_DP0 || cursor_tag == TAG_DP1);
  if (needs_force) {
    aot_emit_fmt(st->body,
      "%sTerm rv = %s;\n"
      "%sif (term_tag(rv) != TAG_CTR && term_tag(rv) != TAG_NUM) rv = cnf(rv);\n"
      "%sreturn aot_make_value(rv);\n",
      indent, rv, indent, indent);
  } else {
    aot_emit_fmt(st->body, "%sreturn aot_make_value(%s);\n", indent, rv);
  }

  bind->n = saved_n;
  st->dup_memo_n = saved_dup_memo_n;
}

// Emit an if-tree dispatching on `arg_var` over the TMatChain rooted
// at `mat`.  Each arm now emits its real handler via
// aot_emit_arm_handler (iter 3) -- each arm currently returns an
// R_VALUE; saturated calls + sibling-pair splits land in iter 4.
//
// MAT-chain shape (term/tag.c:108):
//   TAG_MAT.ext = match value (the constructor label / NUM value)
//   heap[val + 0] = handler (arm body)
//   heap[val + 1] = fallback (next MAT in chain, or LAM(default))
// Walk the chain's explicit arms (skip default) and return 1 iff
// ANY handler is a TAG_LAM -- i.e., the source destructures a CTR
// child.  In that case the whole chain only dispatches CTRs (you
// don't usually mix NUM and CTR cases in one match) so we can skip
// emitting the TAG_NUM-arm if-blocks per match value.
//
// Conservative: returns 0 ("ambiguous") for chains where every
// handler is a bare expr (numeric fib-style: `match k { 0 -> 0,
// 1 -> 1, k -> ... }`).  Those chains keep emitting both arm
// shapes, which is slower at runtime but always correct.
static u32 aot_chain_is_ctr_only(Term mat) {
  Term cursor = mat;
  while (term_tag(cursor) == TAG_MAT) {
    Term handler = book_read(term_val(cursor) + 0);
    // Conservative test: only prune the NUM-arm emit when the
    // handler has 2+ nested LAMs.  A single TLam is ambiguous --
    // could be a CTR destructure binding the matched value, OR a
    // HOF return where the LAM is the value.  Multi-LAM chains
    // (TLam[l, TLam[r, ...]]) only make sense for CTRs with
    // multiple children, so those are unambiguous.
    if (term_tag(handler) == TAG_LAM) {
      Term inner = book_read(term_val(handler));
      if (term_tag(inner) == TAG_LAM) return 1;
    }
    cursor = book_read(term_val(cursor) + 1);
  }
  return 0;
}

static void aot_emit_mat_chain(AotProgState *st, Term mat,
                               const char *arg_var,
                               AotBindings *bind, const char *indent) {
  char inner_indent[64];
  snprintf(inner_indent, sizeof inner_indent, "%s  ", indent);

  u32 ctr_only = aot_chain_is_ctr_only(mat);
  if (ctr_only) {
    aot_emit_fmt(st->body,
      "%s/* dead-arm pruned: chain is CTR-only (some arm destructures) */\n",
      indent);
  }

  Term cursor = mat;
  while (term_tag(cursor) == TAG_MAT) {
    u32 match    = term_ext(cursor);
    u64 loc      = term_val(cursor);
    Term handler = book_read(loc + 0);
    Term fb      = book_read(loc + 1);

    if (!ctr_only) {
      aot_emit_fmt(st->body,
        "%sif (term_tag(%s) == TAG_NUM && (u32)term_val(%s) == %uu) {\n",
        indent, arg_var, arg_var, match);
      aot_emit_arm_handler(st, handler, arg_var, /*is_ctr_arm=*/0,
                            bind, inner_indent);
      aot_emit_fmt(st->body, "%s}\n", indent);
    }

    aot_emit_fmt(st->body,
      "%sif (term_tag(%s) == TAG_CTR && term_ext(%s) == %uu) {\n",
      indent, arg_var, arg_var, match);
    aot_emit_arm_handler(st, handler, arg_var, /*is_ctr_arm=*/1,
                          bind, inner_indent);
    aot_emit_fmt(st->body, "%s}\n", indent);

    cursor = fb;
  }

  aot_emit_fmt(st->body, "%s/* default arm */\n", indent);
  aot_emit_arm_handler(st, cursor, arg_var, /*kind=default*/2,
                        bind, indent);
}

// Detect the App-of-Mat shape: `App(MAT, x)` where MAT is the def's
// TMatChain and x is a TVar referencing one of the bound LAM args.
// Returns 1 if we emitted dispatch code; 0 otherwise (caller falls
// back to the iter 1 stub return).
// Emit App-of-Mat dispatch: `App(MatChain, var)` where var is a
// bound binder.  At the top of the def the caller passes
// dv_name="dv" / indent="  "; nested dispatches (e.g. an inner
// MatChain in the default arm of an outer match) get fresh names
// like "dv2", "dv3" so they don't shadow each other.
//
// Returns 1 if the shape matched and dispatch was emitted;
// 0 otherwise (caller falls back to value-expr emit).
static u32 aot_emit_app_of_mat_named(AotProgState *st, Term cursor,
                                      AotBindings *bind,
                                      const char *dv_name,
                                      const char *indent) {
  if (term_tag(cursor) != TAG_APP) return 0;
  u64 app_loc = term_val(cursor);
  Term fun = book_read(app_loc + 0);
  Term arg = book_read(app_loc + 1);
  if (term_tag(fun) != TAG_MAT) return 0;

  // arg can be: a bare TVar referencing a bound name, OR any Term
  // we need to emit as a value first then dispatch on.  The bare-
  // TVar fast-path avoids an alloc; the general path handles
  // recursive arg shapes (e.g., dispatch result feeds the next
  // dispatch).
  char arg_name_buf[64];
  const char *arg_name = NULL;
  if (term_tag(arg) == TAG_VAR) {
    arg_name = aot_bind_lookup(bind, term_val(arg));
  }
  if (arg_name == NULL) {
    arg_name = aot_emit_value_expr_named(
        st->body, arg, bind, indent,
        arg_name_buf, sizeof arg_name_buf);
  }

  aot_emit_fmt(st->body, "%s/* dispatch on %s */\n", indent, arg_name);
  // Force the dispatched value when not already in WHNF.  Use cnf
  // (not wnf) so DP* projections fire DUP-NUM / DUP-CTR and
  // materialise as plain NUM / CTR -- otherwise the tag-check
  // chain misses (DP* is Levy-opaque under wnf alone).  CTR/NUM
  // inputs short-circuit the call; only DP*/REF/OP2/APP/MAT pay
  // the cnf cost.
  aot_emit_fmt(st->body,
    "%sTerm %s = %s;\n"
    "%sif (term_tag(%s) != TAG_CTR && term_tag(%s) != TAG_NUM) %s = cnf(%s);\n",
    indent, dv_name, arg_name,
    indent, dv_name, dv_name, dv_name, dv_name);
  aot_emit_mat_chain(st, fun, dv_name, bind, indent);
  return 1;
}

// Top-level convenience wrapper.
static u32 aot_emit_app_of_mat(AotProgState *st, Term cursor,
                               AotBindings *bind) {
  return aot_emit_app_of_mat_named(st, cursor, bind, "dv", "  ");
}

// === WHNF / PRI classification (for value-fallback auto-force) =====
//
// `aot_tag_is_whnf(tag)`: is a Term with this head tag already in
// weak head normal form?  These are the tags wnf() would return as-is
// without further reduction.  Used to skip the auto-force when the
// body is purely atomic (NUM/CTR/LAM/SUP/...).
//
// `aot_body_contains_pri(term, depth)`: walk the body's heap
// structure looking for a TAG_PRI.  TPri is the explicit
// side-effect mechanism -- callers fire it via TWnf at their chosen
// time, so the dylib must NOT auto-reduce a body containing PRI.
// Bounded recursion to keep this safe on deeply-nested or cyclic
// shapes.

static _Bool aot_tag_is_whnf(u8 tag) {
  switch (tag) {
    case TAG_NUM:
    case TAG_ERA:
    case TAG_LAM:
    case TAG_SUP:
    case TAG_CTR:
    case TAG_TEN:
    case TAG_FVR:
    case TAG_ANY:
    case TAG_VAR:
    case TAG_DP0:
    case TAG_DP1:
      return 1;
    default:
      return 0;
  }
}

#define AOT_PRI_SCAN_MAX_DEPTH  32

static _Bool aot_body_contains_pri(Term term, u32 depth) {
  if (depth > AOT_PRI_SCAN_MAX_DEPTH) return 0;
  u8  tag = term_tag(term);
  u64 loc = term_val(term);
  switch (tag) {
    case TAG_PRI:
      return 1;

    // Heap[loc..loc+1] = [a, b]
    case TAG_APP:
    case TAG_OP2:
    case TAG_MAT:
    case TAG_EQL:
    case TAG_AND:
    case TAG_OR:
    case TAG_ANN:
    case TAG_BRI:
    case TAG_WHEN:
    case TAG_SUP: {
      Term a = book_read(loc + 0);
      Term b = book_read(loc + 1);
      return aot_body_contains_pri(a, depth + 1) ||
             aot_body_contains_pri(b, depth + 1);
    }

    // Heap[loc] = body
    case TAG_LAM:
    case TAG_INC:
    case TAG_DUP:
      return aot_body_contains_pri(book_read(loc), depth + 1);

    // Heap[loc] = NUM(n); Heap[loc+1..loc+n] = children
    case TAG_CTR: {
      Term n_cell = book_read(loc);
      if (term_tag(n_cell) != TAG_NUM) return 0;
      u32 n = (u32)term_val(n_cell);
      if (n > 64) return 0;  // safety cap
      for (u32 i = 0; i < n; i++) {
        if (aot_body_contains_pri(book_read(loc + 1 + i), depth + 1))
          return 1;
      }
      return 0;
    }

    // Leaves / opaques: NUM, ERA, VAR, REF, NUM, TEN, FVR, ANY,
    // DP0/DP1, ALO, UOP, ...  None contain PRI directly.
    default:
      return 0;
  }
}

fn char *thvm_aot_emit_program(u32 def_id, const char *name) {
  if (def_id >= DEFS_CAP) return NULL;
  Term root = DEFS[def_id];
  if (root == 0) return NULL;

  // Reset the per-emit fresh-name counter so the same def emits
  // identical source across calls (cache-by-content in build.c
  // hashes the output -- monotonically-increasing v_K names would
  // otherwise miss the cache on every call).
  aot_value_fresh = 0;

  // Two output buffers: body (entry function) + cont_defs (zero or
  // more par_<name>_cont_K functions that arm-handler emit pushes
  // onto when it sees a sibling-pair SPLIT).  Final assembly stitches
  // them together with the preamble + forward decls + dispatch +
  // register.
  AotEmit body_buf, cont_buf, final_buf;
  aot_emit_init(&body_buf);
  aot_emit_init(&cont_buf);
  aot_emit_init(&final_buf);
  if (!body_buf.buf || !cont_buf.buf || !final_buf.buf) return NULL;

  AotBindings bind;
  aot_bind_init(&bind);

  AotProgState st = {
    .body       = &body_buf,
    .cont_defs  = &cont_buf,
    .cont_count = 0,
    .self_id    = def_id,
    .self_arity = 0,
    .self_name  = name,
  };
  // dup_memo[], dup_memo_n, dup_fresh are zero-initialized by the
  // designated init above (unspecified fields default to 0).
  aot_current_st = &st;

  u32  argc;
  Term body = aot_peel_lams(root, &argc, &bind);
  st.self_arity = argc;

  // Emit the entry-body interior into body_buf.
  if (!aot_emit_app_of_mat(&st, body, &bind)) {
    // Fallback: emit the body as a value expression and return it
    // as R_VALUE.  Useful for defs whose body is a TPri redex, a
    // CTR construction, an arithmetic expression, etc. -- anything
    // that isn't an outer App-of-Mat dispatch.
    const char *rv = aot_emit_value_expr(st.body, body, &bind, "  ");
    // Auto-reduce the result so TAOTRun's contract is consistent:
    // the returned Term is always in head-normal form.  Skip when
    // the body contains TAG_PRI -- those are explicit side-effect
    // redexes that callers fire via TWnf at their chosen time.
    // Skip when the body is purely atomic (NUM/CTR/LAM/SUP/...)
    // -- already WHNF, no force needed.
    _Bool needs_force = !aot_body_contains_pri(body, 0) &&
                        !aot_tag_is_whnf(term_tag(body));
    if (needs_force) {
      aot_emit_fmt(st.body,
        "  /* fallback: value expression body, auto-reduce */\n"
        "  return aot_make_value(wnf(%s));\n", rv);
    } else {
      aot_emit_fmt(st.body,
        "  /* fallback: value expression body */\n"
        "  return aot_make_value(%s);\n", rv);
    }
  }

  // === Assemble final output ========================================
  aot_emit_fmt(&final_buf,
    "// auto-generated by thvm_aot_emit_program(\"%s\")\n"
    "// def_id %u, arity %u, %u cont(s)\n"
    "//\n"
    "// This source is meant to be #include'd into a TU that has\n"
    "// already brought the thvm AOT runtime into scope (typically\n"
    "// via `#include \"src/thvm.c\"` -- which transitively pulls in\n"
    "// task.h / halloc.h / cont.c / resolve.c / worker.c).  The\n"
    "// emit deliberately omits #includes so the same string works\n"
    "// for both the in-tree path (test files) and the dylib path\n"
    "// (a generated wrapper file that includes thvm.c at the top).\n"
    "\n"
    "#define FN_%s  0u\n",
    name, def_id, argc, st.cont_count,
    name);

  // CONT_<name>_K constants.  Reserve K = 0..cont_count-1 starting
  // at 1u so they don't collide with FN_<name> = 0u.
  for (u32 k = 0; k < st.cont_count; k++) {
    aot_emit_fmt(&final_buf,
      "#define CONT_%s_%u  %uu\n", name, k, k + 1);
  }
  aot_emit_str(&final_buf, "\n");

  // Forward declare entry + each cont so dispatch can reference
  // them regardless of definition order.
  aot_emit_fmt(&final_buf,
    "static AotResult par_%s_entry(AotProgram *p, AotTask *t);\n",
    name);
  for (u32 k = 0; k < st.cont_count; k++) {
    aot_emit_fmt(&final_buf,
      "static AotResult par_%s_cont_%u(AotProgram *p, AotTask *t);\n",
      name, k);
  }
  aot_emit_str(&final_buf, "\n");

  // Cont function defs (already in cont_buf).
  if (st.cont_count > 0) {
    aot_emit_str(&final_buf, cont_buf.buf);
  }

  // Entry function def.
  aot_emit_fmt(&final_buf,
    "static AotResult par_%s_entry(AotProgram *p, AotTask *t) {\n"
    "  (void)p; (void)t;\n",
    name);
  aot_emit_str(&final_buf, body_buf.buf);
  aot_emit_str(&final_buf, "}\n\n");

  // Dispatch table (entry + each cont).
  aot_emit_fmt(&final_buf,
    "static AotResult aot_program_%s_dispatch(AotProgram *p, AotTask *t) {\n"
    "  switch (t->fn_id) {\n"
    "    case FN_%s:  return par_%s_entry(p, t);\n",
    name, name, name);
  for (u32 k = 0; k < st.cont_count; k++) {
    aot_emit_fmt(&final_buf,
      "    case CONT_%s_%u:  return par_%s_cont_%u(p, t);\n",
      name, k, name, k);
  }
  aot_emit_fmt(&final_buf,
    "    default:     return aot_make_value(0);\n"
    "  }\n"
    "}\n\n"
    "fn void aot_program_%s_register(AotProgram *p) {\n"
    "  p->dispatch = aot_program_%s_dispatch;\n"
    "}\n",
    name, name);

  free(body_buf.buf);
  free(cont_buf.buf);
  aot_current_st = NULL;
  return final_buf.buf;
}

#endif  // THVM_AOT_EMIT_INCLUDED
