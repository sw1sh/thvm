// thvm_unify - most-general-unifier on TAG_CTR + TAG_FVR terms.
//
// Standard Robinson algorithm with occurs check.  Produces a
// substitution `subst` such that applying it to both inputs makes
// them structurally equal.  Returns 1 on success, 0 on failure (mgu
// doesn't exist, e.g. clash on different constructors or occurs
// check fails).
//
// `subst` is the same RewriteSubst structure used by stage 3's
// matcher (declared in src/thvm.h).  Callers should clear it (zero-
// init) before the first call.  On success, walking `bindings[id]`
// recursively yields the most general unifier.
//
// Differs from one-way matching (thvm_match): both sides may contain
// variables; bindings flow in either direction.

// === bound-id undo log ==============================================
//
// The CP formers call thvm_unify millions of times per saturation and
// each call needs an all-zero substitution -- but a unifier binds only
// a handful of ids, so zeroing the full 512-byte RewriteSubst per
// attempt is almost entirely wasted stores.  A hot caller instead
// keeps ONE persistent all-zero RewriteSubst and brackets each attempt
// with thvm_unify_undo_begin() / thvm_unify_undo(): while armed, the
// two binding sites below log every bound id, and the undo re-zeroes
// exactly those slots.  The substitution content seen by thvm_unify /
// thvm_unify_apply is byte-identical to the memset discipline (all-
// zero at attempt start either way); failed unifications leave partial
// bindings in both schemes and the undo covers them too.  Overflow
// (nested thvm_unify calls from another subsystem inside the armed
// window) falls back to a full clear -- never a stale binding.
static u32 g_un_log[128];
static u32 g_un_n     = 0;
static u8  g_un_armed = 0;
static u8  g_un_over  = 0;

fn void thvm_unify_undo_begin(void) {
  g_un_n     = 0;
  g_un_over  = 0;
  g_un_armed = 1;
}

fn void thvm_unify_undo(RewriteSubst *subst) {
  if (g_un_over) {
    memset(subst->bindings, 0, sizeof subst->bindings);
  } else {
    for (u32 i = 0; i < g_un_n; i++) subst->bindings[g_un_log[i]] = 0;
  }
  g_un_n     = 0;
  g_un_over  = 0;
  g_un_armed = 0;
}

static inline void un_log_bind(u32 id) {
  if (!g_un_armed) return;
  if (g_un_n < (u32)(sizeof g_un_log / sizeof g_un_log[0])) {
    g_un_log[g_un_n++] = id;
  } else {
    g_un_over = 1;
  }
}

// Recursively follow subst chains: while `t` is FVR with a binding,
// step to the binding.  Returns the final non-bound term.
static Term unify_walk(Term t, const RewriteSubst *subst) {
  while (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    if (id >= REWRITE_MAX_VAR) return t;
    Term b = subst->bindings[id];
    if (b == 0) return t;
    t = b;
  }
  return t;
}

// Occurs check: does the free variable `var_id` appear (after walking)
// anywhere in `t`?
static u8 unify_occurs(u32 var_id, Term t, const RewriteSubst *subst) {
  t = unify_walk(t, subst);
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == var_id;
    case TAG_CTR: {
      // Direct heap_read for arity + children.
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return 0;
      u32 n = (u32)term_val(n_cell);
      for (u32 i = 0; i < n; i++) {
        Term c = heap_read(base + 1u + (u64)i);
        if (unify_occurs(var_id, c, subst)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

fn u8 thvm_unify(Term s, Term t, RewriteSubst *subst) {
  s = unify_walk(s, subst);
  t = unify_walk(t, subst);
  if (kbo_eq(s, t)) return 1;

  if (term_tag(s) == TAG_FVR) {
    u32 id = term_ext(s);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (unify_occurs(id, t, subst)) return 0;
    subst->bindings[id] = t;
    un_log_bind(id);
    return 1;
  }
  if (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (unify_occurs(id, s, subst)) return 0;
    subst->bindings[id] = s;
    un_log_bind(id);
    return 1;
  }
  if (term_tag(s) == TAG_CTR && term_tag(t) == TAG_CTR) {
    if (term_ext(s) != term_ext(t)) return 0;
    // Direct heap_read for both arities + children.
    u64 sbase = term_val(s);
    u64 tbase = term_val(t);
    Term sn_cell = heap_read(sbase);
    Term tn_cell = heap_read(tbase);
    if (term_tag(sn_cell) != TAG_NUM || term_tag(tn_cell) != TAG_NUM) return 0;
    u32 ns = (u32)term_val(sn_cell);
    u32 nt = (u32)term_val(tn_cell);
    if (ns != nt) return 0;
    for (u32 i = 0; i < ns; i++) {
      Term sc = heap_read(sbase + 1u + (u64)i);
      Term tc = heap_read(tbase + 1u + (u64)i);
      if (!thvm_unify(sc, tc, subst)) return 0;
    }
    return 1;
  }
  return 0;
}

// Rebuild `t` with every FVR id shifted by `offset` so two rules can
// be unified without variable-name collisions.  Stage 4 uses this to
// rename rule_j's variables apart from rule_i's before the overlap.
fn Term thvm_rename_vars(Term t, u32 offset) {
  if (offset == 0) return t;  // identity rename -- no work needed
  switch (term_tag(t)) {
    case TAG_FVR: return term_new_fvr(term_ext(t) + offset);
    case TAG_CTR: {
      // Direct heap_read for arity + children (skip term_ctr_at's
      // redundant arity refetch).
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return t;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term orig = heap_read(base + 1u + (u64)i);
        children[i] = thvm_rename_vars(orig, offset);
        if (children[i] != orig) changed = 1;
      }
      // If t had no variables, every child returned itself -- avoid the
      // wasted term_new_ctr (closed-term case, e.g. ground goal sides).
      if (!changed) return t;
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// Apply a unifier (which may contain chained FVR -> FVR -> term
// links) recursively, returning a fully-resolved term.  Differs from
// thvm_subst_apply in that it follows chains via unify_walk.
fn Term thvm_unify_apply(Term t, const RewriteSubst *subst) {
  t = unify_walk(t, subst);
  switch (term_tag(t)) {
    case TAG_FVR: return t;  // unbound -- leave as-is
    case TAG_CTR: {
      // Read arity and base once via direct heap_read -- term_ctr_at
      // would re-fetch the arity cell per call.
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return t;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term orig = heap_read(base + 1u + (u64)i);
        children[i] = thvm_unify_apply(orig, subst);
        if (children[i] != orig) changed = 1;
      }
      // Hash-cons skip: if no child rewrote, return original t (saves
      // a term_new_ctr allocation per untouched subterm during CP-gen).
      if (!changed) return t;
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// === 7c: canonical variable normalization ==========================
//
// `thvm_normalize_vars` alpha-renames the variables of a stored
// (lhs, rhs) equation/rule so they carry a canonically dense set of
// ids `[0, k)`, k = the count of DISTINCT variables across BOTH
// sides.  This is the convergence-fix unblocker (milestone 7c):
//
//   - The matcher / subst-apply (`src/rewrite/_.c`) silently treat
//     any FVR with id >= REWRITE_MAX_VAR (=64) as an unmatchable
//     constant.  The CP enumerator renames rule j by
//     CP_RENAME_OFFSET and BAKES that offset into the stored CP, so
//     deep overlaps carry ids that cross 64 -- past which the
//     rewriter can no longer fire, killing joinability / subsumption.
//   - Renumbering every stored rule and CP to a dense `[0, k)` set
//     guarantees no stored variable ever crosses the cliff.
//   - It is alpha-renaming: it does NOT change a term's meaning.  A
//     variable occurring in both sides is ONE variable and gets ONE
//     id (lhs and rhs share the numbering).
//   - It makes alpha-equivalent rules/CPs BYTE-IDENTICAL, so the
//     dedup / subsumption criteria start catching them.
//
// The walk is a fixed deterministic preorder: lhs fully, then rhs.
// Each distinct old id is assigned the next free dense id by first
// occurrence.

// nv_map: old-id -> dense-id, as a first-occurrence-ordered list of
// pairs.  Linear search is fine -- stored rules carry few distinct
// variables.  REWRITE_MAX_VAR slots is ample headroom (canonical
// output is always [0, k) with k far below REWRITE_MAX_VAR).
typedef struct {
  u32 old_id[REWRITE_MAX_VAR];
  u32 n;
} NvMap;

// Single fused pass: rebuild `t` in preorder, assigning each distinct
// FVR id its dense id ON FIRST SIGHT and rewriting in the same walk.
// Because dense ids are handed out by preorder first occurrence, the
// map state at the moment a variable is rewritten is exactly what a
// separate collect pass would have produced -- the output is identical
// to the historical collect-then-rewrite two-pass version, at half the
// tree walks (the CP push path calls this millions of times per
// saturation).  Unchanged subtrees are returned as-is (no allocation).
// A variable past a FULL map keeps its id -- the two-pass version's
// collect folded it without registering and its rewrite lookup then
// missed, leaving the term untouched (pathological: more distinct vars
// than REWRITE_MAX_VAR).
static Term nv_rewrite(Term t, NvMap *m) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 old = term_ext(t);
      for (u32 i = 0; i < m->n; i++) {
        if (m->old_id[i] == old) {
          if (i == old) return t;
          return term_new_fvr(i);
        }
      }
      if (m->n >= REWRITE_MAX_VAR) return t;   // full map: keep id
      u32 i = m->n;
      m->old_id[m->n++] = old;
      if (i == old) return t;
      return term_new_fvr(i);
    }
    case TAG_CTR: {
      // Direct heap_read for arity + children.
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return t;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term orig = heap_read(base + 1u + (u64)i);
        children[i] = nv_rewrite(orig, m);
        if (children[i] != orig) changed = 1;
      }
      if (!changed) return t;
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// Canonically renumber the variables of the equation/rule
// `(*lhs, *rhs)` in place.  The two sides SHARE the numbering: a
// variable occurring in both is one variable with one dense id.
// After the call, every FVR id is in `[0, k)` for some k <=
// REWRITE_MAX_VAR.  Alpha-renaming -- preserves meaning.
fn void thvm_normalize_vars(Term *lhs, Term *rhs) {
  if (lhs == NULL || rhs == NULL) return;
  NvMap m;
  m.n = 0;
  *lhs = nv_rewrite(*lhs, &m);
  *rhs = nv_rewrite(*rhs, &m);
}
