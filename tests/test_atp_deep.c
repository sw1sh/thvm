// test_atp_deep.c - the ATP engine's term walks on deep terms, pinned
// to the recursion they replaced.
//
// The walks below keep an explicit work stack instead of recursing once
// per term level, so a term 100,000 levels deep is flattened, indexed,
// rebuilt, matched and printed where the recursive walks overflow the
// 8 MB C stack.  Those recursive walks are kept here, verbatim, as
// references: on a few thousand random terms each iterative walk must
// produce the same results, side effects and heap words in the same
// order.  The flatterm walks of src/atp/ft.c are pinned the same way in
// test_ft_deep.c.

#include "../src/thvm.c"

#include "test.h"

// ---- References: the recursive walks, as the engine had them -------------

// src/atp/rule_index.c:280
static u8 ref_atp_ri_flatten(Term t, Term *flat, u32 *subsz, u32 *flatsym,
                         u8 *folded, u32 cap, u32 *pos) {
  u32 here = *pos;
  if (here >= cap) return 0;
  flat[here]    = t;
  flatsym[here] = atp_ri_flatsym_raw(t, folded);
  *pos = here + 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!ref_atp_ri_flatten(term_ctr_at(t, i), flat, subsz, flatsym, folded,
                          cap, pos)) return 0;
    }
  }
  subsz[here] = *pos - here;
  return 1;
}

// src/atp/rule_index.c:686
static Term ref_atp_ri_build(const Term *flat, const u32 *subsz,
                         const u32 *flatsym, u32 pos) {
  if (flatsym[pos] < ATP_DTREE_CTR_BASE) return flat[pos];  // NUM / variable leaf
  Term src = flat[pos];
  u32  n   = term_ctr_n(src);
  Term children[REWRITE_MAX_ARITY];
  u8   changed = 0u;
  u32  c = pos + 1u;
  for (u32 i = 0; i < n; i++) {
    children[i] = ref_atp_ri_build(flat, subsz, flatsym, c);
    if (children[i] != term_ctr_at(src, i)) changed = 1u;
    c += subsz[c];
  }
  if (!changed) return src;                       // subtree untouched -- share
  return term_new_ctr(term_ext(src), children, n);
}

// src/atp/rule_index.c:846
static u8 ref_atp_subst_apply_to_flat(Term rule_rhs,
                                  const u32 *star,
                                  const Term *subj_flat,
                                  const u32 *subj_subsz,
                                  const u32 *subj_flatsym,
                                  Term *dst_flat,
                                  u32 *dst_subsz,
                                  u32 *dst_flatsym,
                                  u32 *dst_pos,
                                  u8 *folded,
                                  u32 cap) {
  u32 here = *dst_pos;
  if (here >= cap) return 0;
  switch (term_tag(rule_rhs)) {
    case TAG_FVR: {
      u32 vid = term_ext(rule_rhs);
      if (vid >= ATP_DTREE_MAXVARS) return 0;     // folded: fall back to tree path
      u32 spos = star[vid];
      if (spos == ATP_DTREE_NIL) return 0;        // unbound: fall back
      u32 ssz = subj_subsz[spos];
      if (here + ssz > cap) return 0;
      memcpy(&dst_flat[here],    &subj_flat[spos],
             (size_t)ssz * sizeof(Term));
      memcpy(&dst_subsz[here],   &subj_subsz[spos],
             (size_t)ssz * sizeof(u32));
      memcpy(&dst_flatsym[here], &subj_flatsym[spos],
             (size_t)ssz * sizeof(u32));
      *dst_pos = here + ssz;
      return 1;
    }
    case TAG_CTR: {
      dst_flat[here]    = rule_rhs;
      dst_flatsym[here] = atp_ri_flatsym_raw(rule_rhs, folded);
      *dst_pos = here + 1u;
      u64 base = term_val(rule_rhs);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) {
        dst_subsz[here] = 1u;
        return 1;
      }
      u32 n = (u32)term_val(n_cell);
      for (u32 i = 0; i < n; i++) {
        Term child = heap_read(base + 1u + (u64)i);
        if (!ref_atp_subst_apply_to_flat(child, star, subj_flat, subj_subsz,
                                     subj_flatsym, dst_flat, dst_subsz,
                                     dst_flatsym, dst_pos, folded, cap)) {
          return 0;
        }
      }
      dst_subsz[here] = *dst_pos - here;
      return 1;
    }
    default: {
      dst_flat[here]    = rule_rhs;
      dst_flatsym[here] = atp_ri_flatsym_raw(rule_rhs, folded);
      dst_subsz[here]   = 1u;
      *dst_pos = here + 1u;
      return 1;
    }
  }
}

// src/atp/fv_index.c:515
static u8 ref_atp_dt_flatten(Term t, u32 *subsz, u32 *flatsym,
                         AtpDTreeVarMap *vm, u32 cap, u32 *pos) {
  u32 here = *pos;
  if (here >= cap) return 0;
  flatsym[here] = atp_dtree_flatsym(t, vm);
  *pos = here + 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!ref_atp_dt_flatten(term_ctr_at(t, i), subsz, flatsym, vm, cap, pos))
        return 0;
    }
  }
  subsz[here] = *pos - here;          // positions covered by t's subtree
  return 1;
}
// src/kbo/_.c:64
static u8 ref_kbo_eq(Term s, Term t) {
  if (s == t) return 1;   // pointer-identical -- rewriting/subst_apply
                          // share subterm cells, so this fires often
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  // CTR-tagged: structural-hash short-circuit before the recursive walk.
  // KBO orientation + match consistency checks call ref_kbo_eq on terms that
  // have already been through the KBO weight memo, so their hashes are
  // often cached in g_kbo_thash.  When both ends have a current-epoch
  // entry and the hashes differ, the terms are guaranteed unequal -- the
  // recursive walk would discover the same answer but pay an O(|t|) cost.
  if (term_tag(s) == TAG_CTR) {
    KboTHashEnt *cs = &g_kbo_thash[kbo_thash_slot(s)];
    KboTHashEnt *ct = &g_kbo_thash[kbo_thash_slot(t)];
    if (cs->epoch == g_kbo_epoch && cs->t == s
        && ct->epoch == g_kbo_epoch && ct->t == t
        && cs->hash != ct->hash) return 0;
  }
  switch (term_tag(s)) {
    case TAG_FVR: return 1;  // same id (ext) already checked
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!ref_kbo_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

// src/rewrite/_.c:27
static u8 ref_thvm_match(Term pattern, Term term, RewriteSubst *subst) {
  switch (term_tag(pattern)) {
    case TAG_FVR: {
      u32 id = term_ext(pattern);
      if (id >= REWRITE_MAX_VAR) return 0;
      if (subst->bindings[id] == 0) {
        subst->bindings[id] = term;
        return 1;
      }
      return ref_kbo_eq(subst->bindings[id], term);
    }
    case TAG_CTR: {
      if (term_tag(term) != TAG_CTR) return 0;
      if (term_ext(pattern) != term_ext(term)) return 0;
      // Direct heap_read for both pattern and term's arity + children
      // (avoids term_ctr_at's redundant arity refetch per call).
      u64 pbase = term_val(pattern);
      u64 tbase = term_val(term);
      Term pn_cell = heap_read(pbase);
      Term tn_cell = heap_read(tbase);
      if (term_tag(pn_cell) != TAG_NUM || term_tag(tn_cell) != TAG_NUM) return 0;
      u32 np = (u32)term_val(pn_cell);
      u32 nt = (u32)term_val(tn_cell);
      if (np != nt) return 0;
      for (u32 i = 0; i < np; i++) {
        Term pc = heap_read(pbase + 1u + (u64)i);
        Term tc = heap_read(tbase + 1u + (u64)i);
        if (!ref_thvm_match(pc, tc, subst)) return 0;
      }
      return 1;
    }
    default:
      // ATP lazy-rule sentinel tripwire (THVM_ATP_SENTINEL_STRICT=1):
      // a TAG_NUM pattern with the lazy-store value tags reaching the
      // matcher means some consumer read a rule slot raw instead of
      // materializing through atp_rule_term/the mirrors -- the silent
      // never-matches failure class behind the lazy+classic formation
      // collapse (e798dba8).  Abort loudly so the blind site can be
      // enumerated empirically instead of by code audit.  Zero cost
      // unset (one cached getenv).
      if (term_tag(pattern) == TAG_NUM &&
          (term_val(pattern) == 0xFA151u || term_val(pattern) == 0xFA152u)) {
        static int strict = -1;
        if (strict < 0) {
          const char *e = getenv("THVM_ATP_SENTINEL_STRICT");
          strict = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
        }
        if (strict) {
          fprintf(stderr,
                  "SENTINEL-STRICT: lazy rule sentinel 0x%llX reached "
                  "ref_thvm_match as a pattern\n",
                  (unsigned long long)term_val(pattern));
          abort();
        }
      }
      return 0;
  }
}

// src/rewrite/_.c:88
static Term ref_thvm_subst_apply(Term t, const RewriteSubst *subst) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < REWRITE_MAX_VAR && subst->bindings[id] != 0) {
        return subst->bindings[id];
      }
      return t;
    }
    case TAG_CTR: {
      // Direct heap_read for arity + children (avoids term_ctr_at's
      // redundant arity refetch per call).
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return t;
      u32 n = (u32)term_val(n_cell);
      Term stk[REWRITE_MAX_ARITY];
      Term *children = term_kids_buf(stk, n);
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term orig = heap_read(base + 1u + (u64)i);
        children[i] = ref_thvm_subst_apply(orig, subst);
        if (children[i] != orig) changed = 1;
      }
      // Hash-cons skip: if no child rewrote, reuse original t pointer
      // (saves a term_new_ctr allocation per unchanged subtree).
      Term r = changed ? term_new_ctr(term_ext(t), children, n) : t;
      term_kids_release(children, stk);
      return r;
    }
    default: return t;
  }
}

// src/rewrite/_.c:136
static Term ref_rewrite_try_top(Term t, const Term *lhs, const Term *rhs,
                            u32 n_rules, u8 *fired) {
#ifdef THVM_ATP_AC
  u64 ac_mask = thvm_atp_get_ac_mask();
#endif
  for (u32 i = 0; i < n_rules; i++) {
    RewriteSubst subst = {{0}};
#ifdef THVM_ATP_AC
    if (ac_mask != 0ull) {
      AtpAcInfo ac = { .ac_mask = ac_mask };
      if (atp_match_ac(lhs[i], t, &ac, &subst)) {
        *fired = 1;
        return ref_thvm_subst_apply(rhs[i], &subst);
      }
      continue;
    }
#endif
    if (ref_thvm_match(lhs[i], t, &subst)) {
      *fired = 1;
      return ref_thvm_subst_apply(rhs[i], &subst);
    }
  }
  *fired = 0;
  return t;
}

// src/rewrite/_.c:171
static Term ref_thvm_rewrite_step(Term t, const Term *lhs, const Term *rhs, u32 n_rules) {
  u8 fired = 0;
  Term r = ref_rewrite_try_top(t, lhs, rhs, n_rules, &fired);
  if (fired) return r;

  // Descend into CTR children, left-to-right; first sub-rewrite wins.
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    Term stk[REWRITE_MAX_ARITY];
    Term *children = term_kids_buf(stk, n);
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      Term original = children[i];
      Term rewritten = ref_thvm_rewrite_step(original, lhs, rhs, n_rules);
      if (!ref_kbo_eq(rewritten, original)) {
        children[i] = rewritten;
        Term r = term_new_ctr(term_ext(t), children, n);
        term_kids_release(children, stk);
        return r;
      }
    }
    term_kids_release(children, stk);
  }

  return t;
}

// src/unify/_.c:78
static u8 ref_unify_occurs(u32 var_id, Term t, const RewriteSubst *subst) {
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
        if (ref_unify_occurs(var_id, c, subst)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// src/unify/_.c:98
static u8 ref_thvm_unify(Term s, Term t, RewriteSubst *subst) {
  s = unify_walk(s, subst);
  t = unify_walk(t, subst);
  if (ref_kbo_eq(s, t)) return 1;

  if (term_tag(s) == TAG_FVR) {
    u32 id = term_ext(s);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (ref_unify_occurs(id, t, subst)) return 0;
    subst->bindings[id] = t;
    un_log_bind(id);
    return 1;
  }
  if (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (ref_unify_occurs(id, s, subst)) return 0;
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
      if (!ref_thvm_unify(sc, tc, subst)) return 0;
    }
    return 1;
  }
  return 0;
}

// src/unify/_.c:143
static Term ref_thvm_rename_vars(Term t, u32 offset) {
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
      Term stk[REWRITE_MAX_ARITY];
      Term *children = term_kids_buf(stk, n);
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term orig = heap_read(base + 1u + (u64)i);
        children[i] = ref_thvm_rename_vars(orig, offset);
        if (children[i] != orig) changed = 1;
      }
      // If t had no variables, every child returned itself -- avoid the
      // wasted term_new_ctr (closed-term case, e.g. ground goal sides).
      Term r = changed ? term_new_ctr(term_ext(t), children, n) : t;
      term_kids_release(children, stk);
      return r;
    }
    default: return t;
  }
}

// src/unify/_.c:189
static Term ref_thvm_unify_apply(Term t, const RewriteSubst *subst) {
  t = unify_walk(t, subst);
  g_unify_apply_cells++;
  switch (term_tag(t)) {
    case TAG_FVR: return t;  // unbound -- leave as-is
    case TAG_CTR: {
      // Read arity and base once via direct heap_read -- term_ctr_at
      // would re-fetch the arity cell per call.
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) { g_unify_apply_taint++; return t; }
      u32 n = (u32)term_val(n_cell);
      Term stk[REWRITE_MAX_ARITY];
      Term *children = term_kids_buf(stk, n);
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term orig = heap_read(base + 1u + (u64)i);
        children[i] = ref_thvm_unify_apply(orig, subst);
        if (children[i] != orig) changed = 1;
      }
      // Hash-cons skip: if no child rewrote, return original t (saves
      // a term_new_ctr allocation per untouched subterm during CP-gen).
      Term r = changed ? term_new_ctr(term_ext(t), children, n) : t;
      term_kids_release(children, stk);
      return r;
    }
    default: return t;
  }
}

// src/unify/_.c:265
static Term ref_nv_rewrite(Term t, NvMap *m) {
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
      Term stk[REWRITE_MAX_ARITY];
      Term *children = term_kids_buf(stk, n);
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term orig = heap_read(base + 1u + (u64)i);
        children[i] = ref_nv_rewrite(orig, m);
        if (children[i] != orig) changed = 1;
      }
      Term r = changed ? term_new_ctr(term_ext(t), children, n) : t;
      term_kids_release(children, stk);
      return r;
    }
    default: return t;
  }
}

// src/unify/_.c:308
static void ref_thvm_normalize_vars(Term *lhs, Term *rhs) {
  if (lhs == NULL || rhs == NULL) return;
  NvMap m;
  m.n = 0;
  *lhs = ref_nv_rewrite(*lhs, &m);
  *rhs = ref_nv_rewrite(*rhs, &m);
}

// ---- Random terms ---------------------------------------------------------

static u32 rng32(u32 *seed) {
  u32 x = *seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *seed = x ? x : 0xdeadbeefu;
  return *seed;
}

// Leaves: variables (some past REWRITE_MAX_VAR = 64 and
// ATP_DTREE_MAXVARS), constants and NUM atoms.  `g_plain` keeps to
// variables 0..9 and constants, so an instance of a pattern matches it.
static u8  g_plain = 0u;
static Term mk_leaf(u32 *seed) {
  u32 r = rng32(seed);
  switch ((r >> 6) % (g_plain ? 7u : 10u)) {
    case 0: case 1: case 2: case 3:
      return term_new_fvr((r >> 10) % 10u);
    case 4: case 5: case 6:
      return term_new_ctr((r >> 10) % 12u, NULL, 0u);
    case 7:
      return term_new_fvr(60u + (r >> 10) % 8u);
    case 8:
      return term_new(0, TAG_NUM, (r >> 10) & 3u, (r >> 12) & 0xffu);
    default:
      return term_new(0, TAG_NUM, 2u, 0x80000005u);
  }
}

// Depth-bounded, at most `*budget` nodes, arity 1..3.  A node next to
// the leaves is sometimes wider: up to `g_wide` children (17..20 goes
// past REWRITE_MAX_ARITY; references with a REWRITE_MAX_ARITY child
// buffer are only fed terms with g_wide = 16).
static u32 g_wide = 16u;
static Term mk_term(u32 *seed, u32 depth, u32 *budget) {
  u32 r = rng32(seed);
  if (depth == 0u || *budget == 0u || (r & 7u) < 2u) {
    if (*budget > 0u) (*budget)--;
    return mk_leaf(seed);
  }
  (*budget)--;
  u32 n = 1u + ((r >> 9) % 3u);
  if (depth == 1u && ((r >> 3) & 31u) == 0u) n = g_wide - 3u + ((r >> 12) & 3u);
  Term kids[20];
  for (u32 i = 0; i < n; i++) kids[i] = mk_term(seed, depth - 1u, budget);
  return term_new_ctr((r >> 14) % 12u, kids, n);
}

static Term mk_random(u32 *seed) {
  u32 budget = 2000u;
  return mk_term(seed, 6u, &budget);
}

static Term mk_small(u32 *seed) {
  u32 budget = 30u;
  return mk_term(seed, 2u, &budget);
}

// A copy of `t` with the subterm at one random position replaced.
static Term mk_mutant(u32 *seed, Term t) {
  u32 r = rng32(seed);
  u32 n = term_tag(t) == TAG_CTR ? term_ctr_n(t) : 0u;
  if (n == 0u || (r % 3u) == 0u) {
    Term leaf = mk_leaf(seed);
    if (leaf == t) leaf = term_new_ctr(11u, NULL, 0u);
    return leaf;
  }
  Term kids[20];
  for (u32 i = 0; i < n; i++) kids[i] = term_ctr_at(t, i);
  u32 k = (r >> 4) % n;
  kids[k] = mk_mutant(seed, kids[k]);
  return term_new_ctr(term_ext(t), kids, n);
}

// A CTR word pointing into [lo, hi) moved to the same offset from `to`.
static Term reloc(Term w, u64 lo, u64 hi, u64 to) {
  if (term_tag(w) == TAG_CTR && term_val(w) >= lo && term_val(w) < hi) {
    return (w & ~VAL_MASK) | ((term_val(w) - lo + to) & VAL_MASK);
  }
  return w;
}

// Heap words [a0, a1) and [b0, b1) are the same allocation sequence.
static int heap_regions_same(u64 a0, u64 a1, u64 b0, u64 b1) {
  if (a1 - a0 != b1 - b0) return 0;
  for (u64 k = 0; k < a1 - a0; k++) {
    if (reloc(heap_read(a0 + k), a0, a1, b0) != heap_read(b0 + k)) return 0;
  }
  return 1;
}

// ---- Deep terms ------------------------------------------------------------

enum { DEEP = 100000 };

// A spine `depth` binary nodes deep: node k is f(node k-1, leaf k) for
// even k and f(leaf k, node k-1) for odd k, so the deep path runs
// through both argument positions.  Leaves cycle through a variable
// x_{k mod 5} (or vals[k mod 5] when `vals` is given), a constant and,
// unless `plain`, a NUM atom.
static Term mk_deep(u32 depth, u32 lab, Term bottom, const Term *vals,
                    u8 plain) {
  Term t = bottom;
  for (u32 k = 0; k < depth; k++) {
    Term leaf;
    switch (k % 3u) {
      case 0:  leaf = vals ? vals[k % 5u] : term_new_fvr(k % 5u); break;
      case 1:  leaf = term_new_ctr(5u, NULL, 0u); break;
      default: leaf = plain ? term_new_ctr(6u, NULL, 0u)
                            : term_new(0, TAG_NUM, 1u, k); break;
    }
    Term kids[2];
    if (k & 1u) { kids[0] = leaf; kids[1] = t; }
    else        { kids[0] = t;    kids[1] = leaf; }
    t = term_new_ctr(lab + (k & 1u), kids, 2u);
  }
  return t;
}

// Structural equality, iteratively (FVR by id, other atoms by ext+val).
static int term_eq_iter(Term a, Term b) {
  u32   cap = 1024u, n = 0u;
  Term *st  = (Term *)malloc(2u * cap * sizeof(Term));
  int   eq  = 1;
  st[n++] = a;
  st[n++] = b;
  while (n > 0u && eq) {
    Term y = st[--n];
    Term x = st[--n];
    if (term_tag(x) != term_tag(y) || term_ext(x) != term_ext(y)) { eq = 0; break; }
    if (term_tag(x) != TAG_CTR) {
      if (term_val(x) != term_val(y) && term_tag(x) != TAG_FVR) eq = 0;
      continue;
    }
    u32 k = term_ctr_n(x);
    if (k != term_ctr_n(y)) { eq = 0; break; }
    if (n + 2u * k > 2u * cap) {
      while (n + 2u * k > 2u * cap) cap *= 2u;
      st = (Term *)realloc(st, 2u * cap * sizeof(Term));
    }
    for (u32 i = 0; i < k; i++) {
      st[n++] = term_ctr_at(x, i);
      st[n++] = term_ctr_at(y, i);
    }
  }
  free(st);
  return eq;
}

// Node count, iteratively.
static u32 term_size_iter(Term t) {
  u32   cap = 1024u, n = 0u, size = 0u;
  Term *st  = (Term *)malloc(cap * sizeof(Term));
  st[n++] = t;
  while (n > 0u) {
    Term x = st[--n];
    size++;
    if (term_tag(x) != TAG_CTR) continue;
    u32 k = term_ctr_n(x);
    if (n + k > cap) {
      while (n + k > cap) cap *= 2u;
      st = (Term *)realloc(st, cap * sizeof(Term));
    }
    for (u32 i = 0; i < k; i++) st[n++] = term_ctr_at(x, i);
  }
  free(st);
  return size;
}

// ---- rule_index.c / fv_index.c: flatten, build, subst-to-flat, insert -----

// The discrimination-tree insert as the ATP_DTREE_DEFINE_OPS template
// had it.
static u32 ref_ri_insert_term(AtpRuleIndex *ix, u32 node, Term t,
                              AtpDTreeVarMap *vm) {
  node = atp_ri_child(ix, node, atp_dtree_flatsym(t, vm));
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      node = ref_ri_insert_term(ix, node, term_ctr_at(t, i), vm);
    }
  }
  return node;
}

static u32 ref_dt_insert_term(AtpFvIndex *ix, u32 node, Term t,
                              AtpDTreeVarMap *vm) {
  node = atp_dt_child(ix, node, atp_dtree_flatsym(t, vm));
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      node = ref_dt_insert_term(ix, node, term_ctr_at(t, i), vm);
    }
  }
  return node;
}

enum { RI_N = 3000, RI_CAP = 4096 };

typedef struct {
  Term flat[RI_CAP];
  u32  subsz[RI_CAP];
  u32  flatsym[RI_CAP];
} RiArrays;

static void ri_fill(RiArrays *a) {
  memset(a, 0xab, sizeof *a);
}

// Zeroed first, so a memcmp of two maps never reads stale padding.
static void vm_reset(AtpDTreeVarMap *vm) {
  memset(vm, 0, sizeof *vm);
  atp_dtree_varmap_reset(vm);
}

static void pin_ri_walks(u32 *seed) {
  RiArrays *ra = (RiArrays *)malloc(sizeof(RiArrays));
  RiArrays *na = (RiArrays *)malloc(sizeof(RiArrays));

  TEST_BEGIN("atp_deep/pin-ri-flatten");
  {
    u32 ok = 0;
    for (u32 i = 0; i < RI_N; i++) {
      Term t     = mk_random(seed);
      u32  size  = term_size_iter(t);
      u32  start = (i & 1u) ? 5u : 0u;
      u32  caps[2] = { RI_CAP, start + 1u + rng32(seed) % size };
      for (u32 k = 0; k < 2u; k++) {
        ri_fill(ra);
        ri_fill(na);
        u8  rf = 0u, nf = 0u;
        u32 rp = start, np = start;
        u8  r = ref_atp_ri_flatten(t, ra->flat, ra->subsz, ra->flatsym, &rf, caps[k], &rp);
        u8  n = atp_ri_flatten(t, na->flat, na->subsz, na->flatsym, &nf, caps[k], &np);
        if (r == n && rp == np && rf == nf && memcmp(ra, na, sizeof *ra) == 0) ok++;
      }
    }
    CHECK_EQ(ok, 2u * RI_N);
  }

  TEST_BEGIN("atp_deep/pin-dt-flatten");
  {
    u32 ok = 0;
    for (u32 i = 0; i < RI_N; i++) {
      Term t     = mk_random(seed);
      u32  size  = term_size_iter(t);
      u32  caps[2] = { RI_CAP, 1u + rng32(seed) % size };
      for (u32 k = 0; k < 2u; k++) {
        ri_fill(ra);
        ri_fill(na);
        AtpDTreeVarMap rv, nv;
        vm_reset(&rv);
        vm_reset(&nv);
        u32 rp = 0u, np = 0u;
        u8  r = ref_atp_dt_flatten(t, ra->subsz, ra->flatsym, &rv, caps[k], &rp);
        u8  n = atp_dt_flatten(t, na->subsz, na->flatsym, &nv, caps[k], &np);
        if (r == n && rp == np && memcmp(&rv, &nv, sizeof rv) == 0 &&
            memcmp(ra, na, sizeof *ra) == 0) {
          ok++;
        }
      }
    }
    CHECK_EQ(ok, 2u * RI_N);
  }

  TEST_BEGIN("atp_deep/pin-ri-build-heap-layout");
  {
    u32 ok = 0, rebuilt = 0;
    for (u32 i = 0; i < RI_N; i++) {
      Term t  = mk_random(seed);
      u8   fd = 0u;
      u32  len = 0u;
      ri_fill(na);
      (void)atp_ri_flatten(t, na->flat, na->subsz, na->flatsym, &fd, RI_CAP, &len);
      // A splice leaves a stale CTR above a changed leaf: swap one leaf.
      if (i & 1u) {
        for (u32 tries = 0; tries < 8u; tries++) {
          u32 p = rng32(seed) % len;
          if (na->flatsym[p] < ATP_DTREE_CTR_BASE) {
            na->flat[p] = term_new_fvr(40u + (p & 7u));
            break;
          }
        }
      }
      u32 pos = (i & 2u) ? rng32(seed) % len : 0u;
      u64 h0 = HEAP_NEXT;
      Term r = ref_atp_ri_build(na->flat, na->subsz, na->flatsym, pos);
      u64 h1 = HEAP_NEXT;
      Term n = atp_ri_build(na->flat, na->subsz, na->flatsym, pos);
      u64 h2 = HEAP_NEXT;
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) ok++;
      rebuilt += (u32)(h1 != h0);
    }
    CHECK_EQ(ok, (u32)RI_N);
    CHECK(rebuilt > RI_N / 8u);
  }

  TEST_BEGIN("atp_deep/pin-subst-apply-to-flat");
  {
    u32 ok = 0, hits = 0;
    RiArrays *sa = (RiArrays *)malloc(sizeof(RiArrays));
    for (u32 i = 0; i < RI_N; i++) {
      g_plain = 1u;
      Term s   = mk_random(seed);
      Term rhs = mk_random(seed);
      g_plain = 0u;
      u8  sf = 0u;
      u32 slen = 0u;
      ri_fill(sa);
      (void)atp_ri_flatten(s, sa->flat, sa->subsz, sa->flatsym, &sf, RI_CAP, &slen);
      u32 star[ATP_DTREE_MAXVARS];
      for (u32 v = 0; v < ATP_DTREE_MAXVARS; v++) star[v] = ATP_DTREE_NIL;
      u32 nbound = (i % 3u == 0u) ? 7u : 10u;
      for (u32 v = 0; v < nbound; v++) star[v] = rng32(seed) % slen;
      u32 caps[2] = { RI_CAP, 1u + rng32(seed) % 64u };
      for (u32 k = 0; k < 2u; k++) {
        ri_fill(ra);
        ri_fill(na);
        u8  rf = 0u, nf = 0u;
        u32 rp = 3u, np = 3u;
        u8 r = ref_atp_subst_apply_to_flat(rhs, star, sa->flat, sa->subsz, sa->flatsym,
                                           ra->flat, ra->subsz, ra->flatsym, &rp, &rf, caps[k]);
        u8 n = atp_subst_apply_to_flat(rhs, star, sa->flat, sa->subsz, sa->flatsym,
                                       na->flat, na->subsz, na->flatsym, &np, &nf, caps[k]);
        if (r == n && rp == np && rf == nf && memcmp(ra, na, sizeof *ra) == 0) ok++;
        hits += (u32)(n != 0u);
      }
    }
    free(sa);
    CHECK_EQ(ok, 2u * RI_N);
    CHECK(hits > RI_N / 4u);
  }

  TEST_BEGIN("atp_deep/pin-dtree-insert");
  {
    AtpRuleIndex *rx = atp_ri_new();
    AtpRuleIndex *nx = atp_ri_new();
    AtpFvIndex   *rd = atp_fv_index_new();
    AtpFvIndex   *nd = atp_fv_index_new();
    u32 ok = 0;
    for (u32 i = 0; i < RI_N; i++) {
      Term t = (i % 5u == 4u) ? mk_small(seed) : mk_random(seed);
      AtpDTreeVarMap rv, nv;
      vm_reset(&rv);
      vm_reset(&nv);
      u32 r = ref_ri_insert_term(rx, rx->root, t, &rv);
      u32 n = atp_ri_insert_term(nx, nx->root, t, &nv);
      u8 same = (u8)(r == n && memcmp(&rv, &nv, sizeof rv) == 0);
      vm_reset(&rv);
      vm_reset(&nv);
      r = ref_dt_insert_term(rd, rd->root, t, &rv);
      n = atp_dt_insert_term(nd, nd->root, t, &nv);
      if (same && r == n && memcmp(&rv, &nv, sizeof rv) == 0) ok++;
    }
    CHECK_EQ(ok, (u32)RI_N);
    CHECK_EQ(rx->n_nodes, nx->n_nodes);
    CHECK(memcmp(rx->nodes, nx->nodes, (size_t)rx->n_nodes * sizeof(AtpDTreeNode)) == 0);
    CHECK_EQ(rd->n_nodes, nd->n_nodes);
    CHECK(memcmp(rd->nodes, nd->nodes, (size_t)rd->n_nodes * sizeof(AtpDTreeNode)) == 0);
    atp_ri_free(rx);
    atp_ri_free(nx);
    atp_fv_index_free(rd);
    atp_fv_index_free(nd);
  }

  free(ra);
  free(na);
}

static void deep_ri_walks(void) {
  enum { BIG = 1u << 20 };
  Term *flat    = (Term *)malloc(BIG * sizeof(Term));
  u32  *subsz   = (u32 *)malloc(BIG * sizeof(u32));
  u32  *flatsym = (u32 *)malloc(BIG * sizeof(u32));
  Term *flat2    = (Term *)malloc(BIG * sizeof(Term));
  u32  *subsz2   = (u32 *)malloc(BIG * sizeof(u32));
  u32  *flatsym2 = (u32 *)malloc(BIG * sizeof(u32));
  Term vals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term kid = term_new_ctr(9u, NULL, 0u);
    vals[v] = term_new_ctr(20u + v, &kid, v & 1u);
  }
  Term pat  = mk_deep(DEEP, 1u, term_new_fvr(4u), NULL, 1u);
  Term subj = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  u32  size = term_size_iter(subj);

  TEST_BEGIN("atp_deep/deep-ri-flatten-build");
  {
    u32 psize = term_size_iter(pat);
    u8  fd  = 0u;
    u32 len = 0u;
    CHECK(atp_ri_flatten(pat, flat, subsz, flatsym, &fd, BIG, &len));
    CHECK_EQ(len, psize);
    CHECK_EQ(subsz[0], psize);
    CHECK(flat[0] == pat);
    CHECK(atp_ri_build(flat, subsz, flatsym, 0u) == pat);
    // Rename every variable leaf: the whole spine above them is rebuilt.
    u32 nvar = 0u;
    for (u32 p = 0; p < len; p++) {
      if (term_tag(flat[p]) == TAG_FVR) {
        flat[p] = term_new_fvr(term_ext(flat[p]) + 10u);
        nvar++;
      }
    }
    CHECK(nvar > DEEP / 3u);
    Term vars10[5];
    for (u32 v = 0; v < 5u; v++) vars10[v] = term_new_fvr(10u + v);
    Term want = mk_deep(DEEP, 1u, term_new_fvr(14u), vars10, 1u);
    CHECK(term_eq_iter(atp_ri_build(flat, subsz, flatsym, 0u), want));
    u32 cap_fail = 0u;
    CHECK(!atp_ri_flatten(subj, flat2, subsz2, flatsym2, &fd, size - 1u, &cap_fail));
    CHECK_EQ(cap_fail, size - 1u);
  }

  TEST_BEGIN("atp_deep/deep-dt-flatten-insert");
  {
    u32 psize = term_size_iter(pat);
    AtpDTreeVarMap vm;
    atp_dtree_varmap_reset(&vm);
    u32 len = 0u;
    CHECK(atp_dt_flatten(pat, subsz2, flatsym2, &vm, BIG, &len));
    CHECK_EQ(len, psize);
    CHECK_EQ(subsz2[0], psize);
    CHECK_EQ(vm.n, 5u);
    AtpRuleIndex *ix = atp_ri_new();
    atp_dtree_varmap_reset(&vm);
    u32 leaf = atp_ri_insert_term(ix, ix->root, pat, &vm);
    u32 nodes = ix->n_nodes;
    CHECK_EQ(nodes, psize + 1u);
    atp_dtree_varmap_reset(&vm);
    CHECK_EQ(atp_ri_insert_term(ix, ix->root, pat, &vm), leaf);
    CHECK_EQ(ix->n_nodes, nodes);
    atp_ri_free(ix);
  }

  TEST_BEGIN("atp_deep/deep-subst-apply-to-flat");
  {
    // Bind x_v to the subject-flat slice of vals[v].
    Term holder_kids[5];
    for (u32 v = 0; v < 5u; v++) holder_kids[v] = vals[v];
    Term holder = term_new_ctr(30u, holder_kids, 5u);
    u8  fd = 0u;
    u32 hlen = 0u;
    CHECK(atp_ri_flatten(holder, flat, subsz, flatsym, &fd, BIG, &hlen));
    u32 star[ATP_DTREE_MAXVARS];
    for (u32 v = 0; v < ATP_DTREE_MAXVARS; v++) star[v] = ATP_DTREE_NIL;
    u32 q = 1u;
    for (u32 v = 0; v < 5u; v++) { star[v] = q; q += subsz[q]; }
    u32 dlen = 0u;
    CHECK(atp_subst_apply_to_flat(pat, star, flat, subsz, flatsym,
                                  flat2, subsz2, flatsym2, &dlen, &fd, BIG));
    CHECK_EQ(dlen, size);
    // The instantiated flat is the flat of subj.
    u32 slen = 0u;
    Term *flat3    = (Term *)malloc(BIG * sizeof(Term));
    u32  *subsz3   = (u32 *)malloc(BIG * sizeof(u32));
    u32  *flatsym3 = (u32 *)malloc(BIG * sizeof(u32));
    CHECK(atp_ri_flatten(subj, flat3, subsz3, flatsym3, &fd, BIG, &slen));
    CHECK_EQ(slen, dlen);
    CHECK(memcmp(subsz2, subsz3, (size_t)slen * sizeof(u32)) == 0);
    CHECK(memcmp(flatsym2, flatsym3, (size_t)slen * sizeof(u32)) == 0);
    CHECK(term_eq_iter(atp_ri_build(flat2, subsz2, flatsym2, 0u), subj));
    free(flat3);
    free(subsz3);
    free(flatsym3);
  }

  free(flat);
  free(subsz);
  free(flatsym);
  free(flat2);
  free(subsz2);
  free(flatsym2);
}

// ---- src/rewrite, src/unify, kbo_eq: match, apply, step, unify, eq --------

// A structurally equal copy in fresh cells (shallow random terms only).
static Term copy_term(Term t) {
  if (term_tag(t) != TAG_CTR) return t;
  u32  n = term_ctr_n(t);
  Term kids[20];
  for (u32 i = 0; i < n; i++) kids[i] = copy_term(term_ctr_at(t, i));
  return term_new_ctr(term_ext(t), kids, n);
}

// A random subterm of `t`.
static Term pick_sub(u32 *seed, Term t) {
  while (term_tag(t) == TAG_CTR && term_ctr_n(t) > 0u && (rng32(seed) & 3u) != 0u) {
    t = term_ctr_at(t, rng32(seed) % term_ctr_n(t));
  }
  return t;
}

// `t` with each variable / constant leaf turned into x_{0..3} at random.
static Term generalize(u32 *seed, Term t) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) == 0u) {
    return (rng32(seed) & 1u) ? term_new_fvr(rng32(seed) & 3u) : t;
  }
  u32  n = term_ctr_n(t);
  Term kids[20];
  for (u32 i = 0; i < n; i++) kids[i] = generalize(seed, term_ctr_at(t, i));
  return term_new_ctr(term_ext(t), kids, n);
}

// A small ground term: c or c(d).
static Term mk_ground(u32 *seed) {
  u32  r   = rng32(seed);
  Term kid = term_new_ctr((r >> 4) % 12u, NULL, 0u);
  return term_new_ctr(r % 12u, &kid, (r >> 8) & 1u);
}

enum { LIB_N = 3000, LIB_RULES = 6 };

static void pin_term_libs(u32 *seed) {
  Term *src  = (Term *)malloc(sizeof(Term) * LIB_N);
  Term *psrc = (Term *)malloc(sizeof(Term) * LIB_N);
  for (u32 i = 0; i < LIB_N; i++) src[i] = mk_random(seed);
  g_plain = 1u;
  for (u32 i = 0; i < LIB_N; i++) psrc[i] = mk_random(seed);
  g_plain = 0u;

  TEST_BEGIN("atp_deep/pin-kbo-eq");
  {
    u32 ok = 0, hits = 0;
    for (u32 i = 0; i < LIB_N; i++) {
      Term t = src[i];
      Term other[4] = { t, copy_term(t), mk_mutant(seed, t), src[(i + 1u) % LIB_N] };
      for (u32 k = 0; k < 4u; k++) {
        u8 r = ref_kbo_eq(t, other[k]);
        u8 n = kbo_eq(t, other[k]);
        if (r == n) ok++;
        hits += n;
      }
    }
    CHECK_EQ(ok, 4u * LIB_N);
    CHECK(hits >= 2u * LIB_N);
  }

  TEST_BEGIN("atp_deep/pin-match");
  {
    u32 ok = 0, hits = 0;
    for (u32 c = 0; c < 2u; c++) {
      Term *ts = c ? psrc : src;
      for (u32 i = 0; i < LIB_N; i++) {
        RewriteSubst inst;
        memset(&inst, 0, sizeof inst);
        for (u32 v = 0; v < 10u; v++) inst.bindings[v] = mk_small(seed);
        Term s_inst = thvm_subst_apply(ts[i], &inst);
        Term subj[3] = { s_inst, ts[(i + 7u) % LIB_N], mk_mutant(seed, s_inst) };
        for (u32 k = 0; k < 3u; k++) {
          RewriteSubst rs, ns;
          memset(&rs, 0, sizeof rs);
          if (k == 0u && (i & 3u) == 0u) rs.bindings[(i >> 2) % 10u] = inst.bindings[(i >> 2) % 10u];
          ns = rs;
          u8 r = ref_thvm_match(ts[i], subj[k], &rs);
          u8 n = thvm_match(ts[i], subj[k], &ns);
          if (r == n && memcmp(&rs, &ns, sizeof rs) == 0) ok++;
          hits += (u32)(c == 1u && k == 0u && n);
        }
      }
    }
    CHECK_EQ(ok, 6u * LIB_N);
    CHECK_EQ(hits, (u32)LIB_N);
  }

  TEST_BEGIN("atp_deep/pin-subst-apply-heap-layout");
  {
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < LIB_N; i++) {
      Term t = mk_random(seed);
      RewriteSubst sub;
      memset(&sub, 0, sizeof sub);
      for (u32 v = 0; v < 10u; v++) {
        if (rng32(seed) & 1u) sub.bindings[v] = mk_small(seed);
      }
      sub.bindings[63] = term_new_ctr(3u, NULL, 0u);
      u64 h0 = HEAP_NEXT;
      Term r = ref_thvm_subst_apply(t, &sub);
      u64 h1 = HEAP_NEXT;
      Term n = thvm_subst_apply(t, &sub);
      u64 h2 = HEAP_NEXT;
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) ok++;
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)LIB_N);
  }

  TEST_BEGIN("atp_deep/pin-rewrite-step-heap-layout");
  {
    u32 ok = 0, fired = 0;
    for (u32 i = 0; i < LIB_N; i++) {
      Term t = src[i];
      Term lhs[LIB_RULES], rhs[LIB_RULES];
      for (u32 k = 0; k < LIB_RULES; k++) {
        if (k == 0u) {
          // a permutation rule: fires to an equal term on f(a, a)
          Term xy[2] = { term_new_fvr(0u), term_new_fvr(1u) };
          Term yx[2] = { xy[1], xy[0] };
          u32 lab = rng32(seed) % 12u;
          lhs[k] = term_new_ctr(lab, xy, 2u);
          rhs[k] = term_new_ctr(lab, yx, 2u);
          continue;
        }
        Term u = pick_sub(seed, (k == 1u) ? t : src[(i + k) % LIB_N]);
        if (term_tag(u) != TAG_CTR) u = term_new_ctr(3u, NULL, 0u);
        lhs[k] = generalize(seed, u);
        if (term_tag(lhs[k]) != TAG_CTR) lhs[k] = u;
        rhs[k] = (rng32(seed) & 1u) ? term_new_fvr(rng32(seed) & 3u) : mk_small(seed);
      }
      u64 h0 = HEAP_NEXT;
      Term r = ref_thvm_rewrite_step(t, lhs, rhs, LIB_RULES);
      u64 h1 = HEAP_NEXT;
      Term n = thvm_rewrite_step(t, lhs, rhs, LIB_RULES);
      u64 h2 = HEAP_NEXT;
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) ok++;
      fired += (u32)(n != t);
    }
    CHECK_EQ(ok, (u32)LIB_N);
    CHECK(fired > LIB_N / 4u);
  }

  TEST_BEGIN("atp_deep/pin-unify");
  {
    u32 ok = 0, hits = 0;
    for (u32 i = 0; i < LIB_N; i++) {
      Term s = psrc[i];
      Term t;
      switch (i % 4u) {
        case 0: {   // an instance of s renamed apart: unifiable
          RewriteSubst inst;
          memset(&inst, 0, sizeof inst);
          for (u32 v = 0; v < 10u; v++) {
            inst.bindings[v] = (rng32(seed) & 1u) ? mk_ground(seed) : term_new_fvr(20u + v);
          }
          t = thvm_subst_apply(s, &inst);
          break;
        }
        case 1:     // another pattern over the same variables
          t = psrc[(i + 1u) % LIB_N];
          break;
        case 2: {   // occurs check: x0 against a term holding x0
          Term kid = s;
          t = term_new_ctr(7u, &kid, 1u);
          s = term_new_fvr(0u);
          break;
        }
        default:
          t = mk_mutant(seed, s);
          break;
      }
      RewriteSubst rs, ns;
      memset(&rs, 0, sizeof rs);
      ns = rs;
      u8 r = ref_thvm_unify(s, t, &rs);
      u8 n = thvm_unify(s, t, &ns);
      if (r == n && memcmp(&rs, &ns, sizeof rs) == 0) {
        // The unifier applies the same way, heap words and tallies included.
        u64 c0 = g_unify_apply_cells, x0 = g_unify_apply_taint;
        u64 h0 = HEAP_NEXT;
        Term ra = ref_thvm_unify_apply(s, &rs);
        u64 h1 = HEAP_NEXT;
        u64 c1 = g_unify_apply_cells, x1 = g_unify_apply_taint;
        Term na = thvm_unify_apply(s, &ns);
        u64 h2 = HEAP_NEXT;
        u64 c2 = g_unify_apply_cells, x2 = g_unify_apply_taint;
        u32 id = rng32(seed) % 12u;
        if (heap_regions_same(h0, h1, h1, h2) && reloc(ra, h0, h1, h1) == na &&
            c1 - c0 == c2 - c1 && x1 - x0 == x2 - x1 &&
            ref_unify_occurs(id, t, &rs) == unify_occurs(id, t, &ns)) {
          ok++;
        }
      }
      hits += n;
    }
    CHECK_EQ(ok, (u32)LIB_N);
    CHECK(hits > LIB_N / 5u);
  }

  TEST_BEGIN("atp_deep/pin-rename-normalize-heap-layout");
  {
    static const u32 offs[3] = { 0u, 1u, 70u };
    u32 ok = 0;
    for (u32 i = 0; i < LIB_N; i++) {
      Term t = src[i];
      u32 off = offs[i % 3u];
      u64 h0 = HEAP_NEXT;
      Term r = ref_thvm_rename_vars(t, off);
      u64 h1 = HEAP_NEXT;
      Term n = thvm_rename_vars(t, off);
      u64 h2 = HEAP_NEXT;
      u8 same = (u8)(heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n);
      Term rl = src[i], rr = src[(i + 3u) % LIB_N];
      Term nl = rl, nr = rr;
      u64 g0 = HEAP_NEXT;
      ref_thvm_normalize_vars(&rl, &rr);
      u64 g1 = HEAP_NEXT;
      thvm_normalize_vars(&nl, &nr);
      u64 g2 = HEAP_NEXT;
      if (same && heap_regions_same(g0, g1, g1, g2) &&
          reloc(rl, g0, g1, g1) == nl && reloc(rr, g0, g1, g1) == nr) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)LIB_N);
  }

  free(src);
  free(psrc);
}

static void deep_term_libs(void) {
  Term vals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term kid = term_new_ctr(9u, NULL, 0u);
    vals[v] = term_new_ctr(20u + v, &kid, v & 1u);
  }
  // Matching treats a NUM pattern leaf as a mismatch, so the patterns
  // here are plain (constants and variables); kbo_eq also sees NUMs.
  Term bottom = term_new_ctr(99u, NULL, 0u);
  Term pat  = mk_deep(DEEP, 1u, term_new_fvr(4u), NULL, 1u);
  Term subj = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term near = mk_deep(DEEP, 1u, vals[3], vals, 1u);   // differs at the bottom
  Term mark = mk_deep(DEEP, 1u, bottom, vals, 1u);

  TEST_BEGIN("atp_deep/deep-kbo-eq");
  {
    CHECK(kbo_eq(subj, mk_deep(DEEP, 1u, vals[4], vals, 1u)));
    CHECK(!kbo_eq(subj, near));
    Term num = mk_deep(DEEP, 1u, vals[4], vals, 0u);
    CHECK(kbo_eq(num, mk_deep(DEEP, 1u, vals[4], vals, 0u)));
    CHECK(!kbo_eq(num, mk_deep(DEEP, 1u, vals[3], vals, 0u)));
    CHECK(!kbo_eq(num, subj));
  }

  TEST_BEGIN("atp_deep/deep-match-subst-apply");
  {
    RewriteSubst sub;
    memset(&sub, 0, sizeof sub);
    CHECK(thvm_match(pat, subj, &sub));
    for (u32 v = 0; v < 5u; v++) CHECK(sub.bindings[v] == vals[v]);
    RewriteSubst miss;
    memset(&miss, 0, sizeof miss);
    CHECK(!thvm_match(pat, near, &miss));
    CHECK(term_eq_iter(thvm_subst_apply(pat, &sub), subj));
    CHECK(term_eq_iter(thvm_unify_apply(pat, &sub), subj));
  }

  TEST_BEGIN("atp_deep/deep-rewrite-step");
  {
    // The only redex is the bottom constant: the step rebuilds the spine.
    Term l = bottom;
    Term r = term_new_ctr(98u, NULL, 0u);
    Term out = thvm_rewrite_step(mark, &l, &r, 1u);
    CHECK(term_eq_iter(out, mk_deep(DEEP, 1u, r, vals, 1u)));
    Term none = term_new_ctr(97u, NULL, 0u);
    CHECK(thvm_rewrite_step(mark, &none, &r, 1u) == mark);
  }

  TEST_BEGIN("atp_deep/deep-unify-occurs");
  {
    RewriteSubst sub;
    memset(&sub, 0, sizeof sub);
    CHECK(thvm_unify(pat, subj, &sub));
    for (u32 v = 0; v < 5u; v++) CHECK(kbo_eq(sub.bindings[v], vals[v]));
    RewriteSubst occ;
    memset(&occ, 0, sizeof occ);
    CHECK(!thvm_unify(term_new_fvr(4u), pat, &occ));   // x4 sits at the bottom
    CHECK(unify_occurs(4u, pat, &occ));
    CHECK(!unify_occurs(9u, pat, &occ));
  }

  TEST_BEGIN("atp_deep/deep-rename-normalize");
  {
    Term vars10[5];
    for (u32 v = 0; v < 5u; v++) vars10[v] = term_new_fvr(10u + v);
    CHECK(term_eq_iter(thvm_rename_vars(pat, 10u),
                       mk_deep(DEEP, 1u, term_new_fvr(14u), vars10, 1u)));
    Term l = thvm_rename_vars(pat, 10u), r = subj;
    thvm_normalize_vars(&l, &r);
    CHECK(r == subj);                      // ground: untouched, shared
    Term l2 = l, r2 = r;
    thvm_normalize_vars(&l2, &r2);
    CHECK(l2 == l);                        // canonical: a fixpoint
    RewriteSubst back;
    memset(&back, 0, sizeof back);
    CHECK(thvm_match(l, pat, &back));      // still a variant of pat
  }
}

int main(void) {
  thvm_init();
  u32 seed = 0x6c8e9cf5u;
  pin_ri_walks(&seed);
  deep_ri_walks();
  pin_term_libs(&seed);
  deep_term_libs();
  thvm_free();
  TEST_REPORT();
}
