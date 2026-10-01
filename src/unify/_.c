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
// anywhere in `t`?  Pending subterms sit on an explicit stack.
static u8 unify_occurs(u32 var_id, Term t, const RewriteSubst *subst) {
  Term  st_inl[REWRITE_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = REWRITE_WALK_INL;
  u8    hit = 0u;
  for (;;) {
    t = unify_walk(t, subst);
    switch (term_tag(t)) {
      case TAG_FVR:
        if (term_ext(t) == var_id) hit = 1u;
        break;
      case TAG_CTR: {
        // Direct heap_read for arity + children.
        u64  base   = term_val(t);
        Term n_cell = heap_read(base);
        if (term_tag(n_cell) != TAG_NUM) break;
        u32 n = (u32)term_val(n_cell);
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) st[nst++] = heap_read(base + 1u + (u64)i);
        break;
      }
      default:
        break;
    }
    if (hit || nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return hit;
}

fn u8 thvm_unify(Term s, Term t, RewriteSubst *subst) {
  RewritePairFrame  fr_inl[REWRITE_WALK_INL];
  RewritePairFrame *fr  = fr_inl;
  u32               nfr = 0u;
  u32               cfr = REWRITE_WALK_INL;
  u8                ok  = 1u;
  for (;;) {
    s = unify_walk(s, subst);
    t = unify_walk(t, subst);
    if (kbo_eq(s, t)) {
      // equal pair: nothing to bind
    } else if (term_tag(s) == TAG_FVR) {
      u32 id = term_ext(s);
      if (id >= REWRITE_MAX_VAR || unify_occurs(id, t, subst)) {
        ok = 0u;
        break;
      }
      subst->bindings[id] = t;
      un_log_bind(id);
    } else if (term_tag(t) == TAG_FVR) {
      u32 id = term_ext(t);
      if (id >= REWRITE_MAX_VAR || unify_occurs(id, s, subst)) {
        ok = 0u;
        break;
      }
      subst->bindings[id] = s;
      un_log_bind(id);
    } else if (term_tag(s) == TAG_CTR && term_tag(t) == TAG_CTR) {
      if (term_ext(s) != term_ext(t)) {
        ok = 0u;
        break;
      }
      // Direct heap_read for both arities + children.
      u64 sbase = term_val(s);
      u64 tbase = term_val(t);
      Term sn_cell = heap_read(sbase);
      Term tn_cell = heap_read(tbase);
      if (term_tag(sn_cell) != TAG_NUM || term_tag(tn_cell) != TAG_NUM) {
        ok = 0u;
        break;
      }
      u32 ns = (u32)term_val(sn_cell);
      u32 nt = (u32)term_val(tn_cell);
      if (ns != nt) {
        ok = 0u;
        break;
      }
      if (ns > 0u) {
        if (nfr == cfr) {
          fr = (RewritePairFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].pbase = sbase;
        fr[nfr].tbase = tbase;
        fr[nfr].i     = 0u;
        fr[nfr].n     = ns;
        nfr++;
      }
    } else {
      ok = 0u;
      break;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    RewritePairFrame *f = &fr[nfr - 1u];
    s = heap_read(f->pbase + 1u + (u64)f->i);
    t = heap_read(f->tbase + 1u + (u64)f->i);
    f->i++;
  }
  term_stack_release(fr, fr_inl);
  return ok;
}

// Rebuild `t` with every FVR id shifted by `offset` so two rules can
// be unified without variable-name collisions.  Stage 4 uses this to
// rename rule_j's variables apart from rule_i's before the overlap.
fn Term thvm_rename_vars(Term t, u32 offset) {
  if (offset == 0) return t;  // identity rename -- no work needed
  RewriteNodeFrame  fr_inl[REWRITE_WALK_INL];
  Term              val_inl[REWRITE_WALK_INL];
  RewriteNodeFrame *fr   = fr_inl;
  Term             *val  = val_inl;
  u32               nfr  = 0u;
  u32               cfr  = REWRITE_WALK_INL;
  u32               nval = 0u;
  u32               cval = REWRITE_WALK_INL;
  for (;;) {
    u8   leaf = 1u;
    Term v    = t;
    switch (term_tag(t)) {
      case TAG_FVR:
        v = term_new_fvr(term_ext(t) + offset);
        break;
      case TAG_CTR: {
        // Direct heap_read for arity + children (skip term_ctr_at's
        // redundant arity refetch).  A node whose children all come
        // back unchanged (no variables below) is returned as-is -- no
        // wasted term_new_ctr on a closed subterm.
        u64  base   = term_val(t);
        Term n_cell = heap_read(base);
        u32  n      = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0u;
        if (n > 0u) {
          if (nfr == cfr) {
            fr = (RewriteNodeFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].t    = t;
          fr[nfr].base = base;
          fr[nfr].i    = 0u;
          fr[nfr].n    = n;
          nfr++;
          leaf = 0u;
        }
        break;
      }
      default:
        break;
    }
    if (leaf) {
      if (nval == cval) {
        val = (Term *)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = v;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      RewriteNodeFrame *f = &fr[nfr - 1u];
      nval -= f->n;
      val[nval] = rewrite_node_rebuild(f, &val[nval]);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    RewriteNodeFrame *f = &fr[nfr - 1u];
    t = heap_read(f->base + 1u + (u64)f->i);
    f->i++;
  }
  Term out = val[0];
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
}

// Node/taint tally for thvm_unify_apply: one invocation resolves
// exactly ONE output node (a bound var resolves to its binding's root
// and recurses into the binding's children), so an invocation-count
// delta across a call IS the output term's exact node count -- the
// same count atp_symbol_count_upto computes by re-walking the result.
// The ATP CP former brackets its two side-builds with these to hand
// the KPBehandelt size gate a free exact size (see sw_form_push).
// `taint` marks the defensive early-returns that under-count a
// subtree (torn arity header / over-arity): a bracketing consumer
// must discard the delta when taint moved.  One predictable add per
// node -- fused into the walk the function already does.
u64 g_unify_apply_cells = 0;
u64 g_unify_apply_taint = 0;

// Apply a unifier (which may contain chained FVR -> FVR -> term
// links) recursively, returning a fully-resolved term.  Differs from
// thvm_subst_apply in that it follows chains via unify_walk.
fn Term thvm_unify_apply(Term t, const RewriteSubst *subst) {
  RewriteNodeFrame  fr_inl[REWRITE_WALK_INL];
  Term              val_inl[REWRITE_WALK_INL];
  RewriteNodeFrame *fr   = fr_inl;
  Term             *val  = val_inl;
  u32               nfr  = 0u;
  u32               cfr  = REWRITE_WALK_INL;
  u32               nval = 0u;
  u32               cval = REWRITE_WALK_INL;
  for (;;) {
    t = unify_walk(t, subst);
    g_unify_apply_cells++;
    u8 leaf = 1u;
    if (term_tag(t) == TAG_CTR) {
      // Read arity and base once via direct heap_read -- term_ctr_at
      // would re-fetch the arity cell per call.  An untouched subterm
      // comes back as itself (hash-cons skip: no term_new_ctr per
      // untouched subterm during CP-gen).
      u64  base   = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) {
        g_unify_apply_taint++;
      } else if ((u32)term_val(n_cell) > 0u) {
        if (nfr == cfr) {
          fr = (RewriteNodeFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].t    = t;
        fr[nfr].base = base;
        fr[nfr].i    = 0u;
        fr[nfr].n    = (u32)term_val(n_cell);
        nfr++;
        leaf = 0u;
      }
    }
    if (leaf) {                // unbound FVR / atom / leaf: as-is
      if (nval == cval) {
        val = (Term *)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = t;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      RewriteNodeFrame *f = &fr[nfr - 1u];
      nval -= f->n;
      val[nval] = rewrite_node_rebuild(f, &val[nval]);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    RewriteNodeFrame *f = &fr[nfr - 1u];
    t = heap_read(f->base + 1u + (u64)f->i);
    f->i++;
  }
  Term out = val[0];
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
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
  RewriteNodeFrame  fr_inl[REWRITE_WALK_INL];
  Term              val_inl[REWRITE_WALK_INL];
  RewriteNodeFrame *fr   = fr_inl;
  Term             *val  = val_inl;
  u32               nfr  = 0u;
  u32               cfr  = REWRITE_WALK_INL;
  u32               nval = 0u;
  u32               cval = REWRITE_WALK_INL;
  for (;;) {
    u8   leaf = 1u;
    Term v    = t;
    switch (term_tag(t)) {
      case TAG_FVR: {
        u32 old = term_ext(t);
        u32 i   = 0u;
        while (i < m->n && m->old_id[i] != old) i++;
        if (i == m->n) {
          if (m->n >= REWRITE_MAX_VAR) break;   // full map: keep id
          m->old_id[m->n++] = old;
        }
        if (i != old) v = term_new_fvr(i);
        break;
      }
      case TAG_CTR: {
        // Direct heap_read for arity + children.
        u64  base   = term_val(t);
        Term n_cell = heap_read(base);
        u32  n      = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0u;
        if (n > 0u) {
          if (nfr == cfr) {
            fr = (RewriteNodeFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].t    = t;
          fr[nfr].base = base;
          fr[nfr].i    = 0u;
          fr[nfr].n    = n;
          nfr++;
          leaf = 0u;
        }
        break;
      }
      default:
        break;
    }
    if (leaf) {
      if (nval == cval) {
        val = (Term *)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = v;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      RewriteNodeFrame *f = &fr[nfr - 1u];
      nval -= f->n;
      val[nval] = rewrite_node_rebuild(f, &val[nval]);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    RewriteNodeFrame *f = &fr[nfr - 1u];
    t = heap_read(f->base + 1u + (u64)f->i);
    f->i++;
  }
  Term out = val[0];
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
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
