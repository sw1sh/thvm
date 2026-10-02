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
//
// Each deep block runs on a 1 MB thread stack (tests/deep_stack.h), so a
// walk that slips back to per-level recursion over 100,000 levels
// overflows it whatever `ulimit -s` the machine has.  The reference
// recursions those blocks compare against at 1,000-3,000 levels and
// that need more than a few hundred KB there (the AC, LPO and WM-mirror
// references) hop to a 512 MB stack.  The two 10,000-deep blocks (the ground-join
// rewrite and the CP subindex, quadratic in depth) are caught that way
// only for a recursion of more than 100 bytes a level.

#include "../src/thvm.c"

#include "test.h"
#include "deep_stack.h"

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
// src/atp/cp_store.c:166
static void ref_acp_pack_term(Term t, u32 *pos, u32 *nodes) {
  (*nodes)++;
  acp_scratch_ensure(*pos, 21u);
  u8 *p = g_acp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      *p++ = (u8)'C';
      acp_put_varint(&p, term_ext(t));
      acp_put_varint(&p, n);
      *pos = (u32)(p - g_acp_scratch);
      for (u32 i = 0; i < n; i++) ref_acp_pack_term(term_ctr_at(t, i), pos, nodes);
      return;
    }
    case TAG_NUM:
      *p++ = (u8)'N';
      acp_put_varint(&p, term_ext(t));
      acp_put_varint(&p, term_val(t));
      break;
    case TAG_FVR:
      *p++ = (u8)'V';
      acp_put_varint(&p, term_ext(t));
      break;
    default:
      *p++ = (u8)'E';
      break;
  }
  *pos = (u32)(p - g_acp_scratch);
}

// src/atp/cp_store.c:197
static Term ref_acp_unpack_term(const u8 **pp) {
  if (g_atp_phase_detail) { extern u64 g_acp_unterm_nodes; g_acp_unterm_nodes++; }
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n > REWRITE_MAX_ARITY) n = REWRITE_MAX_ARITY;
      Term kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) kids[i] = ref_acp_unpack_term(pp);
      return term_new_ctr(label, kids, n);
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      return term_new(0, TAG_NUM, dtype, val);
    }
    case 'V':
    default:
      return term_new_fvr((u32)acp_get_varint(pp));
  }
}

// src/atp/cp_store.c:227
static AtpFtCell *ref_acp_unpack_ft(AtpFt *a, const u8 **pp) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n > REWRITE_MAX_ARITY) n = REWRITE_MAX_ARITY;
      if (n == 0u) return ftnew_const(a, label, 0);
      AtpFtCell *kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) kids[i] = ref_acp_unpack_ft(a, pp);
      return ftnew_ctr(a, label, (u16)n, kids, 0);
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      AtpFtCell *kids[2] = {
        ftnew_const(a, dtype & ~WF_VAR_BIT, 0),
        ftnew_const(a, (u32)val & ~WF_VAR_BIT, 0),
      };
      return ftnew_ctr(a, ATPFT_NUM_MARKER, 2u, kids, 0);
    }
    case 'V':
    default:
      return ftnew_var(a, (u32)acp_get_varint(pp), 0);
  }
}

// src/atp/cp_store.c:288
static u32 ref_acp_count_term(Term t) {
  u32 n = 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 k = term_ctr_n(t);
    for (u32 i = 0; i < k; i++) n += ref_acp_count_term(term_ctr_at(t, i));
  }
  return n;
}

// src/atp/cp_store.c:307
static AtpFtCell *ref_acp_unpack_term_ft(AtpFt *a, const u8 **pp, int scratch) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n == 0u) return ftnew_const(a, label, scratch);
      enum { KIDS_STACK = 16 };
      AtpFtCell *kids_stack[KIDS_STACK];
      AtpFtCell **kids      = kids_stack;
      AtpFtCell **kids_heap = NULL;
      if (n > KIDS_STACK) {
        kids_heap = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)n);
        if (kids_heap == NULL) thvm_fatal("ref_acp_unpack_term_ft: kids OOM");
        kids = kids_heap;
      }
      for (u32 i = 0; i < n; i++) kids[i] = ref_acp_unpack_term_ft(a, pp, scratch);
      AtpFtCell *c = ftnew_ctr(a, label, (u16)n, kids, scratch);
      free(kids_heap);
      return c;
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      AtpFtCell *dt_cell  = ftnew_const(a, dtype & ~WF_VAR_BIT, scratch);
      AtpFtCell *raw_cell = ftnew_const(a, ((u32)val) & ~WF_VAR_BIT, scratch);
      AtpFtCell *kids[2]  = { dt_cell, raw_cell };
      return ftnew_ctr(a, ATPFT_NUM_MARKER, 2u, kids, scratch);
    }
    case 'V':
    default:
      return ftnew_var(a, (u32)acp_get_varint(pp), scratch);
  }
}

// src/atp/cp_store.c:462
static u32 ref_wald_term_sort_ft(const WaldSpec *spec, const AtpFtCell *c) {
  if (spec == NULL || spec->n_sorts == 0) return 0;
  if ((c->sym & WF_VAR_BIT) != 0u) {
    u32 id = c->sym & ~WF_VAR_BIT;
    for (u32 i = 0; i < spec->n_vars; i++) {
      if (spec->vars[i].var_id == id) return spec->vars[i].sort;
    }
    return WALD_MAX_SORTS;
  }
  u32 lab = c->sym;
  if (lab == ATPFT_NUM_MARKER) return WALD_MAX_SORTS;   // non-FOL tag
  u32 n = (u32)c->arity;
  const WaldSym *sym = NULL;
  for (u32 i = 0; i < spec->n_symbols; i++) {
    if (spec->symbols[i].label == lab) {
      sym = &spec->symbols[i];
      break;
    }
  }
  if (sym == NULL)     return WALD_MAX_SORTS;
  if (sym->arity != n) return WALD_MAX_SORTS;
  const AtpFtCell *kid = c->next;
  for (u32 i = 0; i < n; i++) {
    u32 child_sort = ref_wald_term_sort_ft(spec, kid);
    if (child_sort != sym->arg_sorts[i]) return WALD_MAX_SORTS;
    kid = kid->end->next;
  }
  return sym->result_sort;
}

// src/atp/cp_store.c:1216
static u8 ref_sjp_map_walk_term(Term t, AcpNvMap *m) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return 0;
      for (u32 i = 0; i < n; i++) {
        if (!ref_sjp_map_walk_term(term_ctr_at(t, i), m)) return 0;
      }
      return 1;
    }
    case TAG_FVR: {
      u32 old = term_ext(t);
      u32 d = 0;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;
        m->old_id[m->n++] = old;
      }
      return 1;
    }
    default:
      return 0;   // NUM/ERA atom: the sj compare never sees these
  }
}

// src/atp/cp_store.c:1246
static u8 ref_sjp_emit_term(Term t, AcpNvMap *m, u32 *pos) {
  sjp_ensure(*pos, 21u);
  u8 *p = g_sjp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return 0;
      *p++ = (u8)'C';
      acp_put_varint(&p, term_ext(t));
      acp_put_varint(&p, n);
      *pos = (u32)(p - g_sjp_scratch);
      for (u32 i = 0; i < n; i++) {
        if (!ref_sjp_emit_term(term_ctr_at(t, i), m, pos)) return 0;
      }
      return 1;
    }
    case TAG_FVR: {
      u32 old = term_ext(t);
      u32 d = 0;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;
        m->old_id[m->n++] = old;
      }
      *p++ = (u8)'V';
      acp_put_varint(&p, d);
      *pos = (u32)(p - g_sjp_scratch);
      return 1;
    }
    default:
      return 0;
  }
}

// src/atp/cp_emit.c:1452
static u32 ref_acp_term_nodes_skip(const u8 **pp) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      (void)acp_get_varint(pp);           // label
      u32 n = (u32)acp_get_varint(pp);    // arity
      if (n > REWRITE_MAX_ARITY) n = REWRITE_MAX_ARITY;
      u32 tot = 1u;
      for (u32 i = 0; i < n; i++) tot += ref_acp_term_nodes_skip(pp);
      return tot;
    }
    case 'N':
      (void)acp_get_varint(pp);           // dtype
      (void)acp_get_varint(pp);           // val
      return 1u;
    case 'V':
    default:
      (void)acp_get_varint(pp);           // ext
      return 1u;
  }
}

// src/atp/cp_emit.c:1498
static u8 ref_acp_match_term(const u8 **pp, Term subj, RewriteSubst *sub) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (term_tag(subj) != TAG_CTR) return 0;
      if (term_ext(subj) != label)   return 0;
      if (term_ctr_n(subj) != n)     return 0;
      for (u32 i = 0; i < n; i++) {
        if (!ref_acp_match_term(pp, term_ctr_at(subj, i), sub)) return 0;
      }
      return 1;
    }
    case 'V': {
      u32 id = (u32)acp_get_varint(pp);
      if (id >= REWRITE_MAX_VAR) return 0;
      if (sub->bindings[id] == 0) { sub->bindings[id] = subj; return 1; }
      return kbo_eq(sub->bindings[id], subj);
    }
    default:         // 'N' (NUM) / 'E' -- thvm_match's default: no match
      return 0;
  }
}

// src/wald/_.c:731
static u32 ref_wald_term_sort(const WaldSpec *spec, Term t) {
  if (spec == NULL || spec->n_sorts == 0) return 0;

  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      for (u32 i = 0; i < spec->n_vars; i++) {
        if (spec->vars[i].var_id == id) return spec->vars[i].sort;
      }
      return WALD_MAX_SORTS;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      u32 n   = term_ctr_n(t);
      const WaldSym *sym = NULL;
      for (u32 i = 0; i < spec->n_symbols; i++) {
        if (spec->symbols[i].label == lab) {
          sym = &spec->symbols[i];
          break;
        }
      }
      if (sym == NULL)         return WALD_MAX_SORTS;
      if (sym->arity != n)     return WALD_MAX_SORTS;
      for (u32 i = 0; i < n; i++) {
        u32 child_sort = ref_wald_term_sort(spec, term_ctr_at(t, i));
        if (child_sort != sym->arg_sorts[i]) return WALD_MAX_SORTS;
      }
      return sym->result_sort;
    }
    default:
      return WALD_MAX_SORTS;
  }
}

// src/atp/trace.c:16
static u32 ref_atp_pretty_term(Term t, char *buf, u32 cap);
static u32 ref_atp_pretty_ctr(Term t, char *buf, u32 cap) {
  if (cap <= 1) return 0;
  u32 lab = term_ext(t);
  u32 n   = term_ctr_n(t);
  int n_w = snprintf(buf, cap, "C%u", lab);
  if (n_w < 0) return 0;
  u32 w = (u32)n_w;
  if (w >= cap) return cap - 1;
  if (n == 0) return w;
  if (w + 1 >= cap) return w;
  w += (u32)snprintf(buf + w, cap - w, "(");
  for (u32 i = 0; i < n; i++) {
    if (w + 2 >= cap) break;
    if (i > 0) w += (u32)snprintf(buf + w, cap - w, ", ");
    if (w >= cap) return cap - 1;
    w += ref_atp_pretty_term(term_ctr_at(t, i), buf + w, cap - w);
    if (w >= cap) return cap - 1;
  }
  if (w + 1 < cap) w += (u32)snprintf(buf + w, cap - w, ")");
  return w;
}

// src/atp/trace.c:38
static u32 ref_atp_pretty_term(Term t, char *buf, u32 cap) {
  if (cap == 0) return 0;
  switch (term_tag(t)) {
    case TAG_FVR: return (u32)snprintf(buf, cap, "x_%u", term_ext(t));
    case TAG_NUM: return (u32)snprintf(buf, cap, "#%u", (u32)term_val(t));
    case TAG_ERA: return (u32)snprintf(buf, cap, "ERA");
    case TAG_CTR: return ref_atp_pretty_ctr(t, buf, cap);
    default:      return (u32)snprintf(buf, cap, "?T%u", term_tag(t));
  }
}

// src/atp/history.c:250
static u32 ref_atp_hx_feat_bytes(AtpHxFeat *f, const u8 **pp, u32 side, u8 top) {
  f->nodes[side]++;
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)atp_hx_varint(pp);
      u32 n     = (u32)atp_hx_varint(pp);
      atp_hx_feat_sym(f, label);
      if (top) f->top_sym[side] = label;
      u32 d = 0u;
      for (u32 i = 0; i < n; i++) {
        u32 cd = ref_atp_hx_feat_bytes(f, pp, side, 0u);
        if (cd > d) d = cd;
      }
      return d + 1u;
    }
    case 'N': {
      (void)atp_hx_varint(pp);
      (void)atp_hx_varint(pp);
      return 1u;
    }
    case 'V': {
      u32 id = (u32)atp_hx_varint(pp);
      atp_hx_feat_var(f, id, side);
      if (top) f->top_var[side] = 1u;
      return 1u;
    }
    default:
      return 1u;
  }
}

// src/atp/history.c:282
static u32 ref_atp_hx_feat_term(AtpHxFeat *f, Term t, u32 side, u8 top) {
  f->nodes[side]++;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 label = term_ext(t);
      u32 n     = term_ctr_n(t);
      atp_hx_feat_sym(f, label);
      if (top) f->top_sym[side] = label;
      u32 d = 0u;
      for (u32 i = 0; i < n; i++) {
        u32 cd = ref_atp_hx_feat_term(f, term_ctr_at(t, i), side, 0u);
        if (cd > d) d = cd;
      }
      return d + 1u;
    }
    case TAG_FVR: {
      atp_hx_feat_var(f, term_ext(t), side);
      if (top) f->top_var[side] = 1u;
      return 1u;
    }
    default:
      return 1u;
  }
}

// src/atp/history.c:350
static void ref_atp_hx_gsym_mark(Term t) {
  if (t == 0 || term_tag(t) != TAG_CTR) return;
  u32 label = term_ext(t);
  if (label < (1u << 20)) {
    if (label >= g_hx.gsym_cap) {
      u32 ncap = g_hx.gsym_cap ? g_hx.gsym_cap : 256u;
      while (ncap <= label) ncap *= 2u;
      u8 *ng = (u8 *)realloc(g_hx.gsym, ncap);
      if (ng == NULL) return;
      memset(ng + g_hx.gsym_cap, 0, ncap - g_hx.gsym_cap);
      g_hx.gsym = ng;
      g_hx.gsym_cap = ncap;
    }
    g_hx.gsym[label] = 1u;
  }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) ref_atp_hx_gsym_mark(term_ctr_at(t, i));
}

// src/atp/enigma_data.c:11
static void ref_atp_cp_var_stats_rec(Term t, u64 *seen, u32 *distinct,
                                 u32 *occ) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      (*occ)++;
      u32 bit = id & 63u;
      if (!((*seen >> bit) & 1u)) {
        *seen |= (1ull << bit);
        (*distinct)++;
      }
      return;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        ref_atp_cp_var_stats_rec(term_ctr_at(t, i), seen, distinct, occ);
      }
      return;
    }
    default: return;
  }
}

// src/atp/enigma_data.c:47
static int ref_atp_term_label_set(Term t, u64 *labels) {
  if (term_tag(t) == TAG_CTR) {
    u32 lab = term_ext(t);
    *labels |= (1ull << (lab & 63u));
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) ref_atp_term_label_set(term_ctr_at(t, i), labels);
  }
  return 0;
}
static Term ref_atp_ic_rewrite_step(Term t, const Term *lhs, const Term *rhs,
                            u32 n_rules);
static u64 ref_atp_term_struct_hash(Term t);
static u8 ref_atp_cp_perm_subsumed(Term lhs, Term rhs);
static u32 ref_atp_struct_hash(Term t);
static u32 ref_atp_conn_successors(AtpState *s, Term t, Term peak,
                               Term *out_buf, u32 *n, u32 cap);
static u8 ref_mnf_term_is_ground(Term t);
static u32 ref_mnf_hash(Term t);
static u32 ref_mnf_hash_span(const AtpFtCell *c);
static int ref_atp_term_has_var(Term t, u32 id);
static int ref_atp_vars_contained(Term a, Term b);
static Term ref_atp_ordered_rewrite_step(AtpState *s, Term t,
                                     const Term *lhs, const Term *rhs,
                                     u32 n_rules, u8 *fired);
static u32 ref_atp_symbol_count(Term t);
static void ref_atp_symbol_count_upto(Term t, u32 cap, u32 *acc);
static u8 ref_atp_twee_sd_eq(Term a, Term b);
static u8 ref_atp_twee_sd_contains(Term hay, Term needle);
static u32 ref_atp_term_depth(Term t);
static int ref_atp_var_occurs(Term t, u32 var_id);
static u32 ref_atp_unif_measure_rec(Term a, Term b, u32 d);
static u32 ref_atp_goal_match_cov(Term pat, Term subj, RewriteSubst base);
static u32 ref_atp_goal_doppel(Term cl, Term cr, Term sx, Term gy,
                           RewriteSubst base);
static void ref_atp_goal_sub_collect_rec(AtpState *s, Term t);
static u32 ref_atp_goal_subterm_overlap_rec(const AtpState *s, Term t, u32 best);
static u32 ref_atp_ladr_nest(Term t);
static u32 ref_atp_ladr_depth(Term t);
static u32 ref_atp_ladr_nest_ft(const AtpFtCell *t);
static u32 ref_atp_ladr_depth_ft(const AtpFtCell *t);
static Term ref_atp_grounded_instance(AtpState *s, Term t, Term lhs,
                                  Term min_const);
static AtpFtCell *ref_ft_ground_absent_vars(AtpFt *a, const AtpFtCell *t,
                                        const AtpFtCell *keep, u32 mc,
                                        u8 *changed);
static int ref_atp_lr_sortieren_rec(Term lhs, Term rhs);
static int ref_atp_lr_sortieren_rec_ft(const AtpFtCell *lhs,
                                   const AtpFtCell *rhs);
static u8 ref_atp_term_has_ctr(Term t, u32 ext);
static void ref_atp_sym_mask_collect(Term t, u32 mask[8]);
static int ref_atp_term_touches_goal(const AtpState *s, Term t);
static u64 ref_atp_term_sym_bitmap(Term t);
static u8 ref_atp_mgu_reducible(AtpState *s, Term face, Term peak);

// src/atp/prim.c:211
static Term ref_atp_ic_rewrite_step(Term t, const Term *lhs, const Term *rhs,
                            u32 n_rules) {
  u8 fired = 0;
  Term r = atp_ic_rewrite_try_top(t, lhs, rhs, n_rules, &fired);
  if (fired) return r;

  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) return t;
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      Term original = children[i];
      Term rewritten = ref_atp_ic_rewrite_step(original, lhs, rhs, n_rules);
      if (!kbo_eq(rewritten, original)) {
        children[i] = rewritten;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }

  return t;
}

// src/atp/prim.c:533
static u64 ref_atp_term_struct_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u64 h = 0xcbf29ce484222325ull ^ ((u64)term_ext(t) * 0x100000001b3ull);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h ^= ref_atp_term_struct_hash(term_ctr_at(t, i));
        h *= 0x100000001b3ull;
      }
      return h ^ ((u64)n * 0x9e3779b97f4a7c15ull);
    }
    case TAG_FVR:
      return 0xfacefacefaceull ^ ((u64)term_ext(t) * 0x100000001b3ull);
    default:
      return 0xdeadbeefcafebabeull ^ ((u64)term_tag(t) * 0x100000001b3ull);
  }
}

// src/atp/diag.c:370
static u8 ref_atp_cp_perm_subsumed(Term lhs, Term rhs) {
  if (lhs == rhs) return 0;                      // identical handled elsewhere
#ifdef THVM_ATP_AC
  // WM-faithful path: when the AC operators have been detected and
  // registered in the perm-subsume mask (thvm_atp_set_perm_subsume_mask
  // from thvm_atp_auto_ac), use the full flattened AC-dispensability
  // test (GZ_ACVerzichtbar).  This catches the n-ary associativity
  // permutations the binary swap below misses.  The mask is a LOCAL
  // AtpAcInfo, NOT the engine-global g_atp_ac_info -- so the engine
  // stays in standard (non-AC) completion exactly like Waldmeister.
  if (g_atp_perm_subsume_mask != 0ull) {
    AtpAcInfo ac = { .ac_mask = g_atp_perm_subsume_mask };
    return atp_cp_ac_dispensable(lhs, rhs, &ac);
  }
#endif
  if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0;
  if (term_ext(lhs) != term_ext(rhs)) return 0;
  // Per-symbol mask gate: when set, only fire perm_sub for symbols in
  // the mask.  Top-symbol bit outside the mask -> keep (return 0).
  if (g_atp_perm_subsume_mask != 0ull) {
    u32 lab = term_ext(lhs);
    if (lab >= 64u || ((g_atp_perm_subsume_mask >> lab) & 1ull) == 0ull) {
      return 0;
    }
  }
  u32 nl = term_ctr_n(lhs), nr = term_ctr_n(rhs);
  if (nl != nr || nl != 2u) return 0;
  Term l0 = term_ctr_at(lhs, 0), l1 = term_ctr_at(lhs, 1);
  Term r0 = term_ctr_at(rhs, 0), r1 = term_ctr_at(rhs, 1);
  // Two children are AC-same iff structurally equal OR they themselves
  // are an AC-permutation -- recurse.  This catches deep commutativity.
  u8 l0_r0 = kbo_eq(l0, r0) || ref_atp_cp_perm_subsumed(l0, r0);
  u8 l1_r1 = kbo_eq(l1, r1) || ref_atp_cp_perm_subsumed(l1, r1);
  if (l0_r0 && l1_r1) {
    // Identical (as multisets, same order):  caller (trivial-join /
    // duplicate-rule guard) handles this; we don't drop it because
    // the equation is `t = t` which should have been spotted upstream.
    // But pure structural equality (l0 == r0 && l1 == r1 with no
    // perm) is already caught earlier; this branch fires only when
    // the children are AC-equal under deeper permutation.  Treat it
    // as perm-subsumed: the equation says nothing new beyond what
    // its (already-derived) children-AC-equality said.
    return 1;
  }
  u8 l0_r1 = kbo_eq(l0, r1) || ref_atp_cp_perm_subsumed(l0, r1);
  u8 l1_r0 = kbo_eq(l1, r0) || ref_atp_cp_perm_subsumed(l1, r0);
  return l0_r1 && l1_r0;
}

// src/atp/diag.c:495
static u32 ref_atp_struct_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 h = 0x811c9dc5u ^ (term_ext(t) * 0x01000193u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h = (h ^ ref_atp_struct_hash(term_ctr_at(t, i))) * 0x01000193u;
      }
      return h ^ (n + 0x9e3779b9u);
    }
    case TAG_FVR:
      return (0x2545f491u ^ term_ext(t)) * 0x01000193u;
    default:
      return (0xdeadbeefu ^ (u32)term_tag(t)) * 0x01000193u;
  }
}

// src/atp/diag.c:516
static u32 ref_atp_conn_successors(AtpState *s, Term t, Term peak,
                               Term *out_buf, u32 *n, u32 cap) {
  u32 size = 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 m = term_ctr_n(t);
    if (m > REWRITE_MAX_ARITY) return size;
    for (u32 i = 0; i < m; i++) {
      u32 base = *n;
      (void)ref_atp_conn_successors(s, term_ctr_at(t, i), peak, out_buf, n, cap);
      for (u32 k = base; k < *n; k++) {
        Term ch[REWRITE_MAX_ARITY];
        for (u32 c = 0; c < m; c++) {
          ch[c] = (c == i) ? out_buf[k] : term_ctr_at(t, c);
        }
        out_buf[k] = term_new_ctr(term_ext(t), ch, m);
      }
    }
  }
  for (u32 j = 0; j < s->n_rules && *n < cap; j++) {
    RewriteSubst sub = {{0}};
    if (thvm_match(s->lhs[j], t, &sub)) {                 // l -> r
      Term repl = thvm_subst_apply(s->rhs[j], &sub);
      out_buf[(*n)++] = repl;
    }
    if (*n >= cap) break;
    if (ref_atp_vars_contained(s->lhs[j], s->rhs[j])) {       // r -> l (var-safe)
      RewriteSubst sb = {{0}};
      if (thvm_match(s->rhs[j], t, &sb)) {
        Term repl = thvm_subst_apply(s->lhs[j], &sb);
        out_buf[(*n)++] = repl;
      }
    }
  }
  // NOTE: no peak gate here.  Inner-position steps are wrapped back
  // into the parent by the recursion above, so a bare-reduct compare
  // gates the WRONG term -- the criterion constrains the FULL rewritten
  // term, which only the BFS caller sees (it filters every successor
  // against the peak before insertion).  The earlier per-reduct gate
  // admitted >=-peak intermediates and over-dropped: McCune
  // Associativity falsely SATURATED under THVM_ATP_CONNECT=1.
  return size;
}

// src/atp/mnf.c:155
static u8 ref_mnf_term_is_ground(Term t) {
  if (term_tag(t) == TAG_FVR) return 0u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!ref_mnf_term_is_ground(term_ctr_at(t, i))) return 0u;
    }
  }
  return 1u;
}

// src/atp/mnf.c:167
static u32 ref_mnf_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 h = 0x811c9dc5u ^ (term_ext(t) * 0x01000193u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h = (h ^ ref_mnf_hash(term_ctr_at(t, i))) * 0x01000193u;
      }
      return h ^ (n + 0x9e3779b9u);
    }
    case TAG_FVR:
      return (0x2545f491u ^ term_ext(t)) * 0x01000193u;
    default:
      return (0xdeadbeefu ^ (u32)term_tag(t)) * 0x01000193u;
  }
}

// src/atp/mnf.c:202
static u32 ref_mnf_hash_span(const AtpFtCell *c) {
  if ((c->sym & WF_VAR_BIT) != 0u)
    return (0x2545f491u ^ (c->sym & ~WF_VAR_BIT)) * 0x01000193u;
  u32 h = 0x811c9dc5u ^ (ft_ctr_sym(c) * 0x01000193u);
  u32 n = (u32)c->arity;
  const AtpFtCell *k = c->next;
  for (u32 i = 0; i < n; i++) {
    h = (h ^ ref_mnf_hash_span(k)) * 0x01000193u;
    k = k->end->next;
  }
  return h ^ (n + 0x9e3779b9u);
}

// src/atp/ordered_rewrite.c:7
static int ref_atp_term_has_var(Term t, u32 id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (ref_atp_term_has_var(term_ctr_at(t, i), id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// src/atp/ordered_rewrite.c:23
static int ref_atp_vars_contained(Term a, Term b) {
  switch (term_tag(a)) {
    case TAG_FVR: return ref_atp_term_has_var(b, term_ext(a));
    case TAG_CTR: {
      u32 n = term_ctr_n(a);
      for (u32 i = 0; i < n; i++) {
        if (!ref_atp_vars_contained(term_ctr_at(a, i), b)) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

// src/atp/ordered_rewrite.c:281
static Term ref_atp_ordered_rewrite_step(AtpState *s, Term t,
                                     const Term *lhs, const Term *rhs,
                                     u32 n_rules, u8 *fired) {
  // DIAG (THVM_ATP_NORMLOG): trip when a normalize subject grows large, to
  // locate the rule-250 lazy-normalize blowup (grow-per-step vs many-steps).
  {
    static int dl = -1; static u32 mx = 0u;
    if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
    if (dl) { u32 sz = ref_atp_symbol_count(t);
      if (sz > mx && sz > 5000u) { mx = sz;
        fprintf(stderr, "NORMLOG t_size=%u\n", sz); } }
  }
  Term top = atp_ordered_try_top(s, t, lhs, rhs, n_rules, fired);
  if (*fired) return top;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    for (u32 i = 0; i < n; i++) {
      u8 cf = 0;
      Term nch = ref_atp_ordered_rewrite_step(s, term_ctr_at(t, i),
                                          lhs, rhs, n_rules, &cf);
      if (cf) {
        Term children[REWRITE_MAX_ARITY];
        for (u32 j = 0; j < n; j++) {
          children[j] = (j == i) ? nch : term_ctr_at(t, j);
        }
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  *fired = 0;
  return t;
}

// src/atp/ordered_rewrite.c:515
static u32 ref_atp_symbol_count(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      u32 c = 1;
      for (u32 i = 0; i < n; i++) {
        c += ref_atp_symbol_count(term_ctr_at(t, i));
      }
      return c;
    }
    default: return 1;   // FVR / atoms / NUM / etc.
  }
}

// src/atp/ordered_rewrite.c:534
static void ref_atp_symbol_count_upto(Term t, u32 cap, u32 *acc) {
  if (*acc >= cap) return;
  (*acc)++;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n && *acc < cap; i++) {
      ref_atp_symbol_count_upto(term_ctr_at(t, i), cap, acc);
    }
  }
}

// src/atp/ordered_rewrite.c:644
static u8 ref_atp_twee_sd_eq(Term a, Term b) {
  if (a == b) return 1u;
  u32 ta = term_tag(a), tb = term_tag(b);
  if (ta != tb) return 0u;
  if (ta == TAG_FVR || ta == TAG_NUM) return term_ext(a) == term_ext(b) &&
                                             term_val(a) == term_val(b);
  if (ta != TAG_CTR) return 0u;
  if (term_ext(a) != term_ext(b)) return 0u;
  u32 n = term_ctr_n(a);
  if (n != term_ctr_n(b)) return 0u;
  for (u32 i = 0; i < n; i++)
    if (!ref_atp_twee_sd_eq(term_ctr_at(a, i), term_ctr_at(b, i))) return 0u;
  return 1u;
}

// src/atp/ordered_rewrite.c:658
static u8 ref_atp_twee_sd_contains(Term hay, Term needle) {
  if (ref_atp_twee_sd_eq(hay, needle)) return 1u;
  if (term_tag(hay) != TAG_CTR) return 0u;
  u32 n = term_ctr_n(hay);
  for (u32 i = 0; i < n; i++)
    if (ref_atp_twee_sd_contains(term_ctr_at(hay, i), needle)) return 1u;
  return 0u;
}

// src/atp/cp_weight.c:9
static u32 ref_atp_term_depth(Term t) {
  if (term_tag(t) != TAG_CTR) return 0;
  u32 n = term_ctr_n(t);
  u32 d = 0;
  for (u32 i = 0; i < n; i++) {
    u32 cd = ref_atp_term_depth(term_ctr_at(t, i));
    if (cd > d) d = cd;
  }
  return n == 0 ? 0 : d + 1;
}

// src/atp/cp_weight.c:22
static int ref_atp_var_occurs(Term t, u32 var_id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == var_id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (ref_atp_var_occurs(term_ctr_at(t, i), var_id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// src/atp/cp_weight.c:45
static u32 ref_atp_unif_measure_rec(Term a, Term b, u32 d) {
  u8 ta = term_tag(a), tb = term_tag(b);
  if (ta == TAG_CTR && tb == TAG_CTR
      && term_ext(a) == term_ext(b)
      && term_ctr_n(a) == term_ctr_n(b)) {
    u32 nd = d > 1 ? d - 1 : 1;
    u32 n = term_ctr_n(a);
    u32 mass = 0;
    for (u32 i = 0; i < n; i++) {
      mass += ref_atp_unif_measure_rec(term_ctr_at(a, i),
                                   term_ctr_at(b, i), nd);
    }
    return mass;
  }
  if (ta == TAG_FVR) {
    return ref_atp_var_occurs(b, term_ext(a)) ? 2u * d : 0u;
  }
  if (tb == TAG_FVR) {
    return ref_atp_var_occurs(a, term_ext(b)) ? 2u * d : 0u;
  }
  // Function-symbol (or non-CTR) clash.
  return d;
}

// src/atp/cp_weight.c:387
static u32 ref_atp_goal_match_cov(Term pat, Term subj, RewriteSubst base) {
  if (term_tag(pat) != TAG_CTR) return 0u;
  u32 best = 0u;
  RewriteSubst t = base;
  if (thvm_match(pat, subj, &t)) {
    u32 c = ref_atp_symbol_count(subj);
    if (c > best) best = c;
  }
  if (term_tag(subj) == TAG_CTR) {
    u32 n = term_ctr_n(subj);
    for (u32 i = 0; i < n; i++) {
      u32 c = ref_atp_goal_match_cov(pat, term_ctr_at(subj, i), base);
      if (c > best) best = c;
    }
  }
  return best;
}

// src/atp/cp_weight.c:409
static u32 ref_atp_goal_doppel(Term cl, Term cr, Term sx, Term gy,
                           RewriteSubst base) {
  u32 best = 0u;
  if (term_tag(cl) == TAG_CTR) {
    RewriteSubst t1 = base;
    if (thvm_match(cl, sx, &t1)) {
      u32 cov_a = ref_atp_symbol_count(sx);
      u32 cov_b = ref_atp_goal_match_cov(cr, gy, t1);
      if (cov_b && cov_a + cov_b > best) best = cov_a + cov_b;
    }
  }
  if (term_tag(sx) == TAG_CTR) {
    u32 n = term_ctr_n(sx);
    for (u32 i = 0; i < n; i++) {
      u32 c = ref_atp_goal_doppel(cl, cr, term_ctr_at(sx, i), gy, base);
      if (c > best) best = c;
    }
  }
  return best;
}

// src/atp/cp_weight.c:444
static void ref_atp_goal_sub_collect_rec(AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return;
  if (atp_term_is_ground(t)) {
    u32 sz = ref_atp_symbol_count(t);
    if (sz >= 2u && s->goal_sub_n < 128u) {   // skip bare constants
      u64 h = ref_atp_term_struct_hash(t);
      u8 dup = 0u;
      for (u32 i = 0; i < s->goal_sub_n; i++)
        if (s->goal_sub_hash[i] == h) { dup = 1u; break; }
      if (!dup) {
        s->goal_sub_hash[s->goal_sub_n] = h;
        s->goal_sub_size[s->goal_sub_n] = (u16)(sz > 0xffffu ? 0xffffu : sz);
        s->goal_sub_n++;
      }
    }
  }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) ref_atp_goal_sub_collect_rec(s, term_ctr_at(t, i));
}

// src/atp/cp_weight.c:470
static u32 ref_atp_goal_subterm_overlap_rec(const AtpState *s, Term t, u32 best) {
  if (term_tag(t) != TAG_CTR) return best;
  if (atp_term_is_ground(t)) {
    u32 sz = ref_atp_symbol_count(t);
    if (sz > best) {
      u64 h = ref_atp_term_struct_hash(t);
      for (u32 i = 0; i < s->goal_sub_n; i++)
        if (s->goal_sub_hash[i] == h) { best = sz; break; }
    }
  }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    best = ref_atp_goal_subterm_overlap_rec(s, term_ctr_at(t, i), best);
  return best;
}

// src/atp/cp_weight.c:688
static u32 ref_atp_ladr_nest(Term t) {
  if (term_tag(t) != TAG_CTR) return 0u;
  u32 n = term_ctr_n(t), c = 0u;
  for (u32 i = 0; i < n; i++) {
    Term ch = term_ctr_at(t, i);
    if (n <= 2u && term_tag(ch) == TAG_CTR && term_ext(ch) == term_ext(t)) c++;
    c += ref_atp_ladr_nest(ch);
  }
  return c;
}

// src/atp/cp_weight.c:698
static u32 ref_atp_ladr_depth(Term t) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) == 0u) return 0u;
  u32 n = term_ctr_n(t), d = 0u;
  for (u32 i = 0; i < n; i++) {
    u32 k = ref_atp_ladr_depth(term_ctr_at(t, i));
    if (k > d) d = k;
  }
  return d + 1u;
}

// src/atp/cp_weight.c:715
static u32 ref_atp_ladr_nest_ft(const AtpFtCell *t) {
  if (atp_ladr_leaf_ft(t)) return 0u;
  u32 c = 0u;
  const AtpFtCell *ch = t->next;
  for (u32 i = 0; i < t->arity; i++) {
    if (t->arity <= 2u && (ch->sym & WF_VAR_BIT) == 0u && ch->sym == t->sym) c++;
    c += ref_atp_ladr_nest_ft(ch);
    ch = ch->end->next;
  }
  return c;
}

// src/atp/cp_weight.c:726
static u32 ref_atp_ladr_depth_ft(const AtpFtCell *t) {
  if (atp_ladr_leaf_ft(t)) return 0u;
  u32 d = 0u;
  const AtpFtCell *ch = t->next;
  for (u32 i = 0; i < t->arity; i++) {
    u32 k = ref_atp_ladr_depth_ft(ch);
    if (k > d) d = k;
    ch = ch->end->next;
  }
  return d + 1u;
}

// src/atp/step.c:1882
static Term ref_atp_grounded_instance(AtpState *s, Term t, Term lhs,
                                  Term min_const) {
  switch (term_tag(t)) {
    case TAG_FVR:
      // Variable: ground it iff it does not occur on the LHS.  The
      // not-on-LHS guard mirrors PCL_FreieVarInstanz's "frei in r"
      // condition -- a bound variable is left alone, so the resulting
      // rule still unifies with redexes that previously matched lhs.
      return ref_atp_term_has_var(lhs, term_ext(t)) ? t : min_const;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n == 0) return t;
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term ch = term_ctr_at(t, i);
        Term sub = ref_atp_grounded_instance(s, ch, lhs, min_const);
        children[i] = sub;
        if (sub != ch) changed = 1;
      }
      if (!changed) return t;
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// src/atp/step.c:1923
static AtpFtCell *ref_ft_ground_absent_vars(AtpFt *a, const AtpFtCell *t,
                                        const AtpFtCell *keep, u32 mc,
                                        u8 *changed) {
  if ((t->sym & WF_VAR_BIT) != 0u) {
    u32 id = t->sym & ~WF_VAR_BIT;
    if (!ft_span_has_var(keep, id)) {
      *changed = 1u;
      return ftnew_const(a, mc, 0);
    }
    return ftnew_var(a, id, 0);
  }
  u32 n = (u32)t->arity;
  if (n == 0u) return ftnew_const(a, ft_ctr_sym(t), 0);
  // Term sibling returns the subtree UNCHANGED above REWRITE_MAX_ARITY
  // (ref_atp_grounded_instance's stack-children cap): clone verbatim, no
  // grounding below it, no changed mark.
  if (n > REWRITE_MAX_ARITY) return ft_clone_span(a, t, 0);
  AtpFtCell *kids[REWRITE_MAX_ARITY];
  const AtpFtCell *kid = t->next;
  for (u32 i = 0; i < n; i++) {
    kids[i] = ref_ft_ground_absent_vars(a, kid, keep, mc, changed);
    kid = kid->end->next;
  }
  return ftnew_ctr(a, ft_ctr_sym(t), (u16)n, kids, 0);
}

// src/atp/step.c:2044
static int ref_atp_lr_sortieren_rec(Term lhs, Term rhs) {
  u8 lv = (term_tag(lhs) == TAG_FVR);
  u8 rv = (term_tag(rhs) == TAG_FVR);
  if (lv && rv) return 0;          // both variables -> Gleich
  if (lv) return -1;               // variable < non-variable -> Kleiner
  if (rv) return 1;                // non-variable > variable -> Groesser
  // Both constructors.  Single-operator Sheffer signature: SymbolVergleich
  // is Gleich for equal functors; differing functors fall back to the
  // label code (a stable total order mirroring the precedence index).
  u32 lf = term_ext(lhs), rf = term_ext(rhs);
  if (lf != rf) return (lf < rf) ? -1 : 1;
  u32 ln = term_ctr_n(lhs), rn = term_ctr_n(rhs);
  if (ln != rn) return (ln < rn) ? -1 : 1;
  for (u32 i = 0; i < ln; i++) {
    int c = ref_atp_lr_sortieren_rec(term_ctr_at(lhs, i), term_ctr_at(rhs, i));
    if (c != 0) return c;
  }
  return 0;
}

// src/atp/step.c:2067
static int ref_atp_lr_sortieren_rec_ft(const AtpFtCell *lhs,
                                   const AtpFtCell *rhs) {
  u8 lv = (u8)((lhs->sym & WF_VAR_BIT) != 0u);
  u8 rv = (u8)((rhs->sym & WF_VAR_BIT) != 0u);
  if (lv && rv) return 0;
  if (lv) return -1;
  if (rv) return 1;
  u32 lf = lhs->sym, rf = rhs->sym;
  if (lf != rf) return (lf < rf) ? -1 : 1;
  u32 ln = (u32)lhs->arity, rn = (u32)rhs->arity;
  if (ln != rn) return (ln < rn) ? -1 : 1;
  const AtpFtCell *lk = lhs->next, *rk = rhs->next;
  for (u32 i = 0; i < ln; i++) {
    int c = ref_atp_lr_sortieren_rec_ft(lk, rk);
    if (c != 0) return c;
    lk = lk->end->next;
    rk = rk->end->next;
  }
  return 0;
}

// src/atp/step.c:2101
static u8 ref_atp_term_has_ctr(Term t, u32 ext) {
  if (term_tag(t) != TAG_CTR) return 0u;
  if (term_ext(t) == ext) return 1u;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    if (ref_atp_term_has_ctr(term_ctr_at(t, i), ext)) return 1u;
  return 0u;
}

// src/atp/rerank.c:986
static void ref_atp_sym_mask_collect(Term t, u32 mask[8]) {
  if (term_tag(t) == TAG_CTR) {
    u32 lbl = term_ext(t);
    mask[(lbl >> 5) & 7u] |= 1u << (lbl & 31u);
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) return;
    for (u32 i = 0; i < n; i++) {
      ref_atp_sym_mask_collect(term_ctr_at(t, i), mask);
    }
  }
}

// src/atp/rerank.c:998
static int ref_atp_term_touches_goal(const AtpState *s, Term t) {
  if (s == NULL || !s->use_sos) return 0;
  if (term_tag(t) == TAG_CTR) {
    u32 lbl = term_ext(t);
    if (s->goal_sym_mask[(lbl >> 5) & 7u] & (1u << (lbl & 31u))) return 1;
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) return 0;
    for (u32 i = 0; i < n; i++) {
      if (ref_atp_term_touches_goal(s, term_ctr_at(t, i))) return 1;
    }
  }
  return 0;
}

// src/atp/cp_interreduce.c:85
static u64 ref_atp_term_sym_bitmap(Term t) {
  if (t == 0u) return 0u;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 lab = term_ext(t);
      u64 m = (lab < 63u) ? ((u64)1 << lab) : ((u64)1 << 63u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) m |= ref_atp_term_sym_bitmap(term_ctr_at(t, i));
      return m;
    }
    default: return 0u;
  }
}

// src/atp/cp_gen.c:160
static u8 ref_atp_mgu_reducible(AtpState *s, Term face, Term peak) {
  if (face == 0 || peak == 0) return 0u;
  if (term_tag(face) == TAG_FVR) {
    if (term_tag(peak) != TAG_CTR) return 0u;   // sigma(x) a bare var: irred.
    return (u8)(!kbo_eq(atp_rules_only_normalize(s, peak, 4096u), peak));
  }
  if (term_tag(face) == TAG_CTR && term_tag(peak) == TAG_CTR) {
    u32 n = term_ctr_n(face);
    if (term_ctr_n(peak) != n) return 0u;
    for (u32 c = 0; c < n; c++)
      if (ref_atp_mgu_reducible(s, term_ctr_at(face, c), term_ctr_at(peak, c)))
        return 1u;
  }
  return 0u;
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
    if (*budget > 0u) {
      (*budget)--;
    }
    return mk_leaf(seed);
  }
  (*budget)--;
  u32 n = 1u + ((r >> 9) % 3u);
  if (depth == 1u && ((r >> 3) & 31u) == 0u) {
    n = g_wide - 3u + ((r >> 12) & 3u);
  }
  Term kids[20];
  for (u32 i = 0; i < n; i++) {
    kids[i] = mk_term(seed, depth - 1u, budget);
  }
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
    if (leaf == t) {
      leaf = term_new_ctr(11u, NULL, 0u);
    }
    return leaf;
  }
  Term kids[20];
  for (u32 i = 0; i < n; i++) {
    kids[i] = term_ctr_at(t, i);
  }
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
  if (a1 - a0 != b1 - b0) {
    return 0;
  }
  for (u64 k = 0; k < a1 - a0; k++) {
    if (reloc(heap_read(a0 + k), a0, a1, b0) != heap_read(b0 + k)) {
      return 0;
    }
  }
  return 1;
}

static long cell_off(const AtpFtCell *c, const AtpFtCell *base) {
  return c == NULL ? -1L : (long)((const char *)c - (const char *)base);
}

// Same preorder cells (sym / arity / flags) at the same offsets from
// each arena's first cell, with the same next / end links: the two
// walks built the same span AND allocated its cells in the same order.
static int span_layout_same(const AtpFtCell *x, const AtpFtCell *xb,
                            const AtpFtCell *y, const AtpFtCell *yb) {
  const AtpFtCell *px = x;
  const AtpFtCell *py = y;
  for (;;) {
    if (px->sym != py->sym || px->arity != py->arity || px->flags != py->flags) {
      return 0;
    }
    if (cell_off(px, xb) != cell_off(py, yb)) {
      return 0;
    }
    if (cell_off(px->end, xb) != cell_off(py->end, yb)) {
      return 0;
    }
    if (cell_off(px->next, xb) != cell_off(py->next, yb)) {
      return 0;
    }
    if (px == x->end) {
      return py == y->end;
    }
    if (py == y->end) {
      return 0;
    }
    px = px->next;
    py = py->next;
  }
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
      if (term_val(x) != term_val(y) && term_tag(x) != TAG_FVR) {
        eq = 0;
      }
      continue;
    }
    u32 k = term_ctr_n(x);
    if (k != term_ctr_n(y)) { eq = 0; break; }
    if (n + 2u * k > 2u * cap) {
      while (n + 2u * k > 2u * cap) {
        cap *= 2u;
      }
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
    if (term_tag(x) != TAG_CTR) {
      continue;
    }
    u32 k = term_ctr_n(x);
    if (n + k > cap) {
      while (n + k > cap) {
        cap *= 2u;
      }
      st = (Term *)realloc(st, cap * sizeof(Term));
    }
    for (u32 i = 0; i < k; i++) {
      st[n++] = term_ctr_at(x, i);
    }
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
        if (r == n && rp == np && rf == nf && memcmp(ra, na, sizeof *ra) == 0) {
          ok++;
        }
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
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) {
        ok++;
      }
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
      for (u32 v = 0; v < ATP_DTREE_MAXVARS; v++) {
        star[v] = ATP_DTREE_NIL;
      }
      u32 nbound = (i % 3u == 0u) ? 7u : 10u;
      for (u32 v = 0; v < nbound; v++) {
        star[v] = rng32(seed) % slen;
      }
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
        if (r == n && rp == np && rf == nf && memcmp(ra, na, sizeof *ra) == 0) {
          ok++;
        }
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
      if (same && r == n && memcmp(&rv, &nv, sizeof rv) == 0) {
        ok++;
      }
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
    for (u32 v = 0; v < 5u; v++) {
      vars10[v] = term_new_fvr(10u + v);
    }
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
    for (u32 v = 0; v < 5u; v++) {
      holder_kids[v] = vals[v];
    }
    Term holder = term_new_ctr(30u, holder_kids, 5u);
    u8  fd = 0u;
    u32 hlen = 0u;
    CHECK(atp_ri_flatten(holder, flat, subsz, flatsym, &fd, BIG, &hlen));
    u32 star[ATP_DTREE_MAXVARS];
    for (u32 v = 0; v < ATP_DTREE_MAXVARS; v++) {
      star[v] = ATP_DTREE_NIL;
    }
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
  if (term_tag(t) != TAG_CTR) {
    return t;
  }
  u32  n = term_ctr_n(t);
  Term kids[20];
  for (u32 i = 0; i < n; i++) {
    kids[i] = copy_term(term_ctr_at(t, i));
  }
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
  for (u32 i = 0; i < n; i++) {
    kids[i] = generalize(seed, term_ctr_at(t, i));
  }
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
  for (u32 i = 0; i < LIB_N; i++) {
    src[i] = mk_random(seed);
  }
  g_plain = 1u;
  for (u32 i = 0; i < LIB_N; i++) {
    psrc[i] = mk_random(seed);
  }
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
        if (r == n) {
          ok++;
        }
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
        for (u32 v = 0; v < 10u; v++) {
          inst.bindings[v] = mk_small(seed);
        }
        Term s_inst = thvm_subst_apply(ts[i], &inst);
        Term subj[3] = { s_inst, ts[(i + 7u) % LIB_N], mk_mutant(seed, s_inst) };
        for (u32 k = 0; k < 3u; k++) {
          RewriteSubst rs, ns;
          memset(&rs, 0, sizeof rs);
          if (k == 0u && (i & 3u) == 0u) {
            rs.bindings[(i >> 2) % 10u] = inst.bindings[(i >> 2) % 10u];
          }
          ns = rs;
          u8 r = ref_thvm_match(ts[i], subj[k], &rs);
          u8 n = thvm_match(ts[i], subj[k], &ns);
          if (r == n && memcmp(&rs, &ns, sizeof rs) == 0) {
            ok++;
          }
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
        if (rng32(seed) & 1u) {
          sub.bindings[v] = mk_small(seed);
        }
      }
      sub.bindings[63] = term_new_ctr(3u, NULL, 0u);
      u64 h0 = HEAP_NEXT;
      Term r = ref_thvm_subst_apply(t, &sub);
      u64 h1 = HEAP_NEXT;
      Term n = thvm_subst_apply(t, &sub);
      u64 h2 = HEAP_NEXT;
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) {
        ok++;
      }
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
        if (term_tag(u) != TAG_CTR) {
          u = term_new_ctr(3u, NULL, 0u);
        }
        lhs[k] = generalize(seed, u);
        if (term_tag(lhs[k]) != TAG_CTR) {
          lhs[k] = u;
        }
        rhs[k] = (rng32(seed) & 1u) ? term_new_fvr(rng32(seed) & 3u) : mk_small(seed);
      }
      u64 h0 = HEAP_NEXT;
      Term r = ref_thvm_rewrite_step(t, lhs, rhs, LIB_RULES);
      u64 h1 = HEAP_NEXT;
      Term n = thvm_rewrite_step(t, lhs, rhs, LIB_RULES);
      u64 h2 = HEAP_NEXT;
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) {
        ok++;
      }
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
    for (u32 v = 0; v < 5u; v++) {
      CHECK(sub.bindings[v] == vals[v]);
    }
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
    for (u32 v = 0; v < 5u; v++) {
      CHECK(kbo_eq(sub.bindings[v], vals[v]));
    }
    RewriteSubst occ;
    memset(&occ, 0, sizeof occ);
    CHECK(!thvm_unify(term_new_fvr(4u), pat, &occ));   // x4 sits at the bottom
    CHECK(unify_occurs(4u, pat, &occ));
    CHECK(!unify_occurs(9u, pat, &occ));
  }

  TEST_BEGIN("atp_deep/deep-rename-normalize");
  {
    Term vars10[5];
    for (u32 v = 0; v < 5u; v++) {
      vars10[v] = term_new_fvr(10u + v);
    }
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

// ---- cp_store.c / cp_emit.c / trace.c / history.c / enigma_data.c / wald ---

// Pack `t` with `pack` at scratch offset `start`; copy the bytes out.
static u32 pack_copy(void (*pack)(Term, u32 *, u32 *), Term t, u32 start,
                     u8 *out, u32 *nodes) {
  u32 pos = start;
  *nodes = 0u;
  acp_scratch_ensure(start, 1u);
  memset(g_acp_scratch, 0x5a, start);
  pack(t, &pos, nodes);
  memcpy(out, g_acp_scratch, pos);
  return pos;
}

// Labels 0..7: arity (lab >> 1) % 3, result sort lab & 1, argument i of
// sort (lab >> (i + 1)) & 1; variable x_v has sort v & 1.
static void sort_spec_init(WaldSpec *ws) {
  memset(ws, 0, sizeof *ws);
  ws->n_sorts = 2u;
  for (u32 lab = 0; lab < 8u; lab++) {
    WaldSym *sy = &ws->symbols[ws->n_symbols++];
    sy->label = lab;
    sy->arity = (lab >> 1) % 3u;
    sy->result_sort = lab & 1u;
    for (u32 i = 0; i < sy->arity; i++) {
      sy->arg_sorts[i] = (lab >> (i + 1u)) & 1u;
    }
  }
  for (u32 v = 0; v < 10u; v++) {
    ws->vars[ws->n_vars].var_id = v;
    ws->vars[ws->n_vars].sort   = v & 1u;
    ws->n_vars++;
  }
}

// A term of sort `sort` over sort_spec_init's signature, now and then
// broken by a wrong-sort argument.
static Term mk_sorted(u32 *seed, u32 sort, u32 depth) {
  u32 r = rng32(seed);
  if ((r & 63u) == 0u) {
    sort ^= 1u;
  }
  if (depth == 0u || (r & 3u) == 0u) {
    if (r & 4u) {
      return term_new_fvr(((r >> 3) % 5u) * 2u + sort);
    }
    return term_new_ctr((r & 8u) ? 6u + sort : sort, NULL, 0u);
  }
  u32  lab = ((r >> 4) & 1u) ? 4u + sort : 2u + sort;
  u32  n   = (lab >> 1) % 3u;
  Term kids[2];
  for (u32 i = 0; i < n; i++) {
    kids[i] = mk_sorted(seed, (lab >> (i + 1u)) & 1u, depth - 1u);
  }
  return term_new_ctr(lab, kids, n);
}

enum { SER_N = 3000, SER_BUF = 1u << 16 };

static void pin_serialize(u32 *seed) {
  u8 *rb = (u8 *)malloc(SER_BUF);
  u8 *nb = (u8 *)malloc(SER_BUF);
  extern u64 g_acp_unterm_nodes;

  TEST_BEGIN("atp_deep/pin-acp-pack-count");
  {
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < SER_N; i++) {
      Term t = mk_random(seed);
      u32 start = (i & 1u) ? 7u : 0u;
      u32 rn, nn;
      u32 rl = pack_copy(ref_acp_pack_term, t, start, rb, &rn);
      u32 nl = pack_copy(acp_pack_term, t, start, nb, &nn);
      if (rl == nl && rn == nn && memcmp(rb, nb, rl) == 0 &&
          ref_acp_count_term(t) == acp_count_term(t)) {
        ok++;
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)SER_N);
  }

  TEST_BEGIN("atp_deep/pin-acp-unpack");
  {
    u32 ok = 0;
    g_atp_phase_detail = 1u;
    for (u32 i = 0; i < SER_N; i++) {
      g_wide = (i & 1u) ? 20u : 16u;
      Term t = mk_random(seed);
      g_wide = 16u;
      u32 nodes;
      u32 len = pack_copy(acp_pack_term, t, 0u, nb, &nodes);
      u8 ok_i = 1u;
      if ((i & 1u) == 0u) {
        // Clamped decoders: fed only terms within REWRITE_MAX_ARITY.
        const u8 *rp = nb, *np = nb;
        u64 c0 = g_acp_unterm_nodes;
        u64 h0 = HEAP_NEXT;
        Term r = ref_acp_unpack_term(&rp);
        u64 h1 = HEAP_NEXT;
        u64 c1 = g_acp_unterm_nodes;
        Term n = acp_unpack_term(&np);
        u64 h2 = HEAP_NEXT;
        u64 c2 = g_acp_unterm_nodes;
        ok_i &= (u8)(rp == np && np == nb + len && c1 - c0 == c2 - c1 &&
                     heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n);
        rp = nb;
        np = nb;
        ok_i &= (u8)(ref_acp_term_nodes_skip(&rp) == acp_term_nodes_skip(&np) && rp == np);
        AtpFt ra, na;
        ft_init(&ra);
        ft_init(&na);
        AtpFtCell *rb0 = ft_alloc_persistent(&ra);
        AtpFtCell *nb0 = ft_alloc_persistent(&na);
        rp = nb;
        np = nb;
        AtpFtCell *rx = ref_acp_unpack_ft(&ra, &rp);
        AtpFtCell *nx = acp_unpack_ft(&na, &np);
        ok_i &= (u8)(rp == np && span_layout_same(rx, rb0, nx, nb0));
        ft_destroy(&ra);
        ft_destroy(&na);
      }
      for (int scratch = 0; scratch < 2; scratch++) {
        AtpFt ra, na;
        ft_init(&ra);
        ft_init(&na);
        AtpFtCell *rb0 = scratch ? ft_alloc_scratch(&ra) : ft_alloc_persistent(&ra);
        AtpFtCell *nb0 = scratch ? ft_alloc_scratch(&na) : ft_alloc_persistent(&na);
        const u8 *rp = nb, *np = nb;
        AtpFtCell *rx = ref_acp_unpack_term_ft(&ra, &rp, scratch);
        AtpFtCell *nx = acp_unpack_term_ft(&na, &np, scratch);
        ok_i &= (u8)(rp == np && np == nb + len && span_layout_same(rx, rb0, nx, nb0));
        ft_destroy(&ra);
        ft_destroy(&na);
      }
      ok += ok_i;
    }
    g_atp_phase_detail = 0u;
    CHECK_EQ(ok, (u32)SER_N);
  }

  TEST_BEGIN("atp_deep/pin-acp-match-term");
  {
    u32 ok = 0, hits = 0;
    for (u32 i = 0; i < SER_N; i++) {
      g_plain = (u8)(i & 1u);
      Term p = mk_random(seed);
      g_plain = 0u;
      RewriteSubst inst;
      memset(&inst, 0, sizeof inst);
      for (u32 v = 0; v < 10u; v++) {
        inst.bindings[v] = mk_small(seed);
      }
      Term s = thvm_subst_apply(p, &inst);
      Term subj[3] = { s, mk_mutant(seed, s), mk_random(seed) };
      u32 nodes;
      (void)pack_copy(acp_pack_term, p, 0u, nb, &nodes);
      for (u32 k = 0; k < 3u; k++) {
        RewriteSubst rs, ns;
        memset(&rs, 0, sizeof rs);
        ns = rs;
        const u8 *rp = nb, *np = nb;
        u8 r = ref_acp_match_term(&rp, subj[k], &rs);
        u8 n = acp_match_term(&np, subj[k], &ns);
        if (r == n && rp == np && memcmp(&rs, &ns, sizeof rs) == 0) {
          ok++;
        }
        hits += (u32)((i & 1u) && k == 0u && n);
      }
    }
    CHECK_EQ(ok, 3u * SER_N);
    CHECK_EQ(hits, (u32)SER_N / 2u);
  }

  TEST_BEGIN("atp_deep/pin-sjp-map-emit");
  {
    u32 ok = 0, hits = 0;
    for (u32 i = 0; i < SER_N; i++) {
      g_plain = (u8)((i & 3u) != 0u);
      g_wide = (i % 7u == 0u) ? 20u : 16u;
      Term t = mk_random(seed);
      g_plain = 0u;
      g_wide = 16u;
      AcpNvMap rm, nm;
      memset(&rm, 0, sizeof rm);
      memset(&nm, 0, sizeof nm);
      if (i & 8u) { rm.old_id[rm.n++] = 3u; nm = rm; }
      u8 r = ref_sjp_map_walk_term(t, &rm);
      u8 n = sjp_map_walk_term(t, &nm);
      u8 same = (u8)(r == n && memcmp(&rm, &nm, sizeof rm) == 0);
      memset(&rm, 0, sizeof rm);
      memset(&nm, 0, sizeof nm);
      u32 rpos = 5u, npos = 5u;
      sjp_ensure(0u, 64u);
      memset(g_sjp_scratch, 0x5a, 5u);
      r = ref_sjp_emit_term(t, &rm, &rpos);
      memcpy(rb, g_sjp_scratch, rpos);
      n = sjp_emit_term(t, &nm, &npos);
      if (same && r == n && rpos == npos && memcmp(&rm, &nm, sizeof rm) == 0 &&
          memcmp(rb, g_sjp_scratch, npos) == 0) {
        ok++;
      }
      hits += n;
    }
    CHECK_EQ(ok, (u32)SER_N);
    CHECK(hits > SER_N / 3u);
  }

  TEST_BEGIN("atp_deep/pin-wald-term-sort");
  {
    WaldSpec *ws = (WaldSpec *)malloc(sizeof *ws);
    sort_spec_init(ws);
    AtpFt a;
    ft_init(&a);
    u32 ok = 0, sorted = 0;
    for (u32 i = 0; i < SER_N; i++) {
      Term t = (i % 5u == 4u) ? mk_random(seed) : mk_sorted(seed, i & 1u, 6u);
      AtpFtCell *x = ft_from_term(&a, t, 0);
      u32 r = ref_wald_term_sort(ws, t);
      u32 n = wald_term_sort(ws, t);
      if (r == n && ref_wald_term_sort_ft(ws, x) == wald_term_sort_ft(ws, x) &&
          wald_term_sort(NULL, t) == 0u) {
        ok++;
      }
      sorted += (u32)(n != WALD_MAX_SORTS);
    }
    CHECK_EQ(ok, (u32)SER_N);
    CHECK(sorted > SER_N / 3u);
    ft_destroy(&a);
    free(ws);
  }

  TEST_BEGIN("atp_deep/pin-pretty-term");
  {
    static const u32 caps[14] = { 0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 13u, 16u,
                                  33u, 128u, 1024u, 65536u };
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < SER_N; i++) {
      Term t = mk_random(seed);
      if (i % 9u == 0u) {
        Term kids[2] = { t, term_new(0, TAG_ERA, 0, 0) };
        t = term_new_ctr(4u, kids, 2u);
      }
      for (u32 k = 0; k < 14u; k++) {
        memset(rb, 0x5a, (size_t)caps[k] + 8u);
        memset(nb, 0x5a, (size_t)caps[k] + 8u);
        u32 r = ref_atp_pretty_term(t, (char *)rb, caps[k]);
        u32 n = atp_pretty_term(t, (char *)nb, caps[k]);
        if (r == n && memcmp(rb, nb, (size_t)caps[k] + 8u) == 0) {
          ok++;
        }
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, 14u * SER_N);
  }

  TEST_BEGIN("atp_deep/pin-history-enigma-features");
  {
    u32 ok = 0;
    u8 *saved_gsym = g_hx.gsym;
    u32 saved_cap  = g_hx.gsym_cap;
    for (u32 i = 0; i < SER_N; i++) {
      Term l = mk_random(seed), r = mk_random(seed);
      // goal-symbol marks: the same table for both feature walks
      g_hx.gsym = NULL;
      g_hx.gsym_cap = 0u;
      ref_atp_hx_gsym_mark(l);
      u8 *rg = g_hx.gsym;
      u32 rc = g_hx.gsym_cap;
      g_hx.gsym = NULL;
      g_hx.gsym_cap = 0u;
      atp_hx_gsym_mark(l);
      u8 same = (u8)(rc == g_hx.gsym_cap && (rc == 0u || memcmp(rg, g_hx.gsym, rc) == 0));
      free(rg);
      u32 nodes = 0u, pos = 0u;
      acp_pack_term(l, &pos, &nodes);
      acp_pack_term(r, &pos, &nodes);
      memcpy(nb, g_acp_scratch, pos);
      AtpHxFeat rf, nf;
      memset(&rf, 0, sizeof rf);
      memset(&nf, 0, sizeof nf);
      const u8 *rp = nb, *np = nb;
      rf.depth[0] = ref_atp_hx_feat_bytes(&rf, &rp, 0u, 1u);
      rf.depth[1] = ref_atp_hx_feat_bytes(&rf, &rp, 1u, 1u);
      nf.depth[0] = atp_hx_feat_bytes(&nf, &np, 0u, 1u);
      nf.depth[1] = atp_hx_feat_bytes(&nf, &np, 1u, 1u);
      same &= (u8)(rp == np && memcmp(&rf, &nf, sizeof rf) == 0);
      memset(&rf, 0, sizeof rf);
      memset(&nf, 0, sizeof nf);
      rf.depth[0] = ref_atp_hx_feat_term(&rf, l, 0u, 1u);
      rf.depth[1] = ref_atp_hx_feat_term(&rf, r, 1u, 1u);
      nf.depth[0] = atp_hx_feat_term(&nf, l, 0u, 1u);
      nf.depth[1] = atp_hx_feat_term(&nf, r, 1u, 1u);
      same &= (u8)(memcmp(&rf, &nf, sizeof rf) == 0);
      free(g_hx.gsym);
      g_hx.gsym = NULL;
      g_hx.gsym_cap = 0u;
      u64 rs = 0u, ns = 0u, rlb = 0u, nlb = 0u;
      u32 rd = 0u, nd = 0u, ro = 0u, no = 0u;
      ref_atp_cp_var_stats_rec(l, &rs, &rd, &ro);
      atp_cp_var_stats_rec(l, &ns, &nd, &no);
      (void)ref_atp_term_label_set(r, &rlb);
      (void)atp_term_label_set(r, &nlb);
      if (same && rs == ns && rd == nd && ro == no && rlb == nlb) {
        ok++;
      }
    }
    g_hx.gsym = saved_gsym;
    g_hx.gsym_cap = saved_cap;
    CHECK_EQ(ok, (u32)SER_N);
  }

  free(rb);
  free(nb);
}

static void deep_serialize(void) {
  Term vals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term kid = term_new_ctr(9u, NULL, 0u);
    vals[v] = term_new_ctr(20u + v, &kid, v & 1u);
  }
  Term pat  = mk_deep(DEEP, 1u, term_new_fvr(4u), NULL, 1u);
  Term subj = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term num  = mk_deep(DEEP, 1u, vals[4], vals, 0u);
  u32  size = term_size_iter(num);
  u32  cap  = 24u * size;
  u8  *buf  = (u8 *)malloc(cap);

  TEST_BEGIN("atp_deep/deep-acp-pack-unpack");
  {
    u32 nodes = 0u, pos = 0u;
    acp_pack_term(num, &pos, &nodes);
    CHECK_EQ(nodes, size);
    CHECK_EQ(acp_count_term(num), size);
    memcpy(buf, g_acp_scratch, pos);
    const u8 *p = buf;
    CHECK(term_eq_iter(acp_unpack_term(&p), num));
    CHECK(p == buf + pos);
    p = buf;
    CHECK_EQ(acp_term_nodes_skip(&p), size);
    AtpFt a;
    ft_init(&a);
    p = buf;
    CHECK(ft_eq_term(acp_unpack_term_ft(&a, &p, 0), num));
    p = buf;
    CHECK(ft_eq_term(acp_unpack_ft(&a, &p), num));
    ft_destroy(&a);
    pos = 0u;
    acp_pack_term(pat, &pos, &nodes);
    memcpy(buf, g_acp_scratch, pos);
    RewriteSubst sub;
    memset(&sub, 0, sizeof sub);
    p = buf;
    CHECK(acp_match_term(&p, subj, &sub));
    CHECK(p == buf + pos);
    for (u32 v = 0; v < 5u; v++) {
      CHECK(sub.bindings[v] == vals[v]);
    }
  }

  TEST_BEGIN("atp_deep/deep-sjp-emit");
  {
    AcpNvMap m;
    memset(&m, 0, sizeof m);
    CHECK(sjp_map_walk_term(pat, &m));
    CHECK_EQ(m.n, 5u);
    memset(&m, 0, sizeof m);
    u32 epos = 0u;
    CHECK(sjp_emit_term(subj, &m, &epos));
    u32 nodes = 0u, pos = 0u;
    acp_pack_term(subj, &pos, &nodes);
    CHECK_EQ(epos, pos);
    CHECK(memcmp(g_sjp_scratch, g_acp_scratch, pos) == 0);   // ground: same records
    memset(&m, 0, sizeof m);
    epos = 0u;
    CHECK(!sjp_emit_term(num, &m, &epos));                     // a NUM leaf bails
  }

  TEST_BEGIN("atp_deep/deep-wald-term-sort");
  {
    WaldSpec *ws = (WaldSpec *)malloc(sizeof *ws);
    sort_spec_init(ws);
    // 3(3(...3(c1)...)) is sort 1 (label 3: arity 1, argument sort 1).
    Term chain = term_new_ctr(1u, NULL, 0u);
    Term bad   = term_new_ctr(0u, NULL, 0u);
    for (u32 k = 0; k < DEEP; k++) {
      chain = term_new_ctr(3u, &chain, 1u);
      bad   = term_new_ctr(3u, &bad, 1u);
    }
    CHECK_EQ(wald_term_sort(ws, chain), 1u);
    CHECK_EQ(wald_term_sort(ws, bad), (u32)WALD_MAX_SORTS);
    AtpFt a;
    ft_init(&a);
    CHECK_EQ(wald_term_sort_ft(ws, ft_from_term(&a, chain, 0)), 1u);
    CHECK_EQ(wald_term_sort_ft(ws, ft_from_term(&a, bad, 0)), (u32)WALD_MAX_SORTS);
    ft_destroy(&a);
    free(ws);
  }

  TEST_BEGIN("atp_deep/deep-pretty-term");
  {
    u32 pcap = 16u * DEEP + 64u;
    char *pt = (char *)malloc(pcap);
    char *pf = (char *)malloc(pcap);
    AtpFt a;
    ft_init(&a);
    u32 nt = atp_pretty_term(num, pt, pcap);
    u32 nf = atp_pretty_ft(ft_from_term(&a, num, 0), pf, pcap);
    CHECK(nt + 1u < pcap);
    CHECK_EQ(nt, nf);
    CHECK(memcmp(pt, pf, (size_t)nt + 1u) == 0);
    ft_destroy(&a);
    free(pt);
    free(pf);
  }

  TEST_BEGIN("atp_deep/deep-history-enigma-features");
  {
    u32 nodes = 0u, pos = 0u;
    acp_pack_term(pat, &pos, &nodes);
    memcpy(buf, g_acp_scratch, pos);
    AtpHxFeat fb, ft;
    memset(&fb, 0, sizeof fb);
    memset(&ft, 0, sizeof ft);
    const u8 *p = buf;
    u32 db = atp_hx_feat_bytes(&fb, &p, 0u, 1u);
    u32 dt = atp_hx_feat_term(&ft, pat, 0u, 1u);
    CHECK_EQ(db, (u32)DEEP + 1u);
    CHECK_EQ(dt, db);
    CHECK(memcmp(&fb, &ft, sizeof fb) == 0);
    u64 seen = 0u, labels = 0u;
    u32 distinct = 0u, occ = 0u;
    atp_cp_var_stats_rec(pat, &seen, &distinct, &occ);
    CHECK_EQ(distinct, 5u);
    CHECK_EQ(occ, (u32)DEEP / 3u + 2u);
    (void)atp_term_label_set(subj, &labels);
    CHECK_EQ(labels, (1ull << 1) | (1ull << 2) | (1ull << 5) | (1ull << 6) |
                     (1ull << 9) | (0x1full << 20));
    u8 *saved_gsym = g_hx.gsym;
    u32 saved_cap  = g_hx.gsym_cap;
    g_hx.gsym = NULL;
    g_hx.gsym_cap = 0u;
    atp_hx_gsym_mark(subj);
    CHECK(g_hx.gsym != NULL && g_hx.gsym[1] && g_hx.gsym[2] && g_hx.gsym[24] && !g_hx.gsym[3]);
    free(g_hx.gsym);
    g_hx.gsym = saved_gsym;
    g_hx.gsym_cap = saved_cap;
  }

  free(buf);
}

// ---- prim / ordered_rewrite / cp_weight / step / rerank / diag / mnf ------

static u32 g_wo_weights[32];
static u32 g_wo_prec[32];
static KboConfig g_wo_cfg;

// An engine over labels 0..31 (unit weights, precedence = label) with a
// few rules lifted from random terms: generalized subterms (oriented or
// not) and a commutativity rule.
static AtpState *wo_state(u32 *seed) {
  for (u32 i = 0; i < 32u; i++) { g_wo_weights[i] = 1u; g_wo_prec[i] = i; }
  g_wo_cfg.weights    = g_wo_weights;
  g_wo_cfg.precedence = g_wo_prec;
  g_wo_cfg.n_labels   = 32u;
  g_wo_cfg.var_weight = 1u;
  AtpState *s = thvm_atp_init(&g_wo_cfg, 100u);
  Term xy[2] = { term_new_fvr(0u), term_new_fvr(1u) };
  Term yx[2] = { xy[1], xy[0] };
  (void)atp_push_rule(s, term_new_ctr(3u, xy, 2u), term_new_ctr(3u, yx, 2u));
  for (u32 k = 0; k < 10u; k++) {
    Term u = pick_sub(seed, mk_random(seed));
    if (term_tag(u) != TAG_CTR || term_ctr_n(u) == 0u) {
      continue;
    }
    Term l = generalize(seed, u);
    if (term_tag(l) != TAG_CTR) {
      continue;
    }
    Term r = (rng32(seed) & 1u) ? term_new_fvr(rng32(seed) & 3u) : mk_small(seed);
    (void)atp_push_rule(s, l, r);
  }
  return s;
}

enum { WO_N = 2000 };

static void pin_weight_order(u32 *seed) {
  AtpState *s = wo_state(seed);
  AtpFt a;
  ft_init(&a);

  TEST_BEGIN("atp_deep/pin-hashes-ground");
  {
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < WO_N; i++) {
      Term t = mk_random(seed);
      if (i % 7u == 0u) {
        Term kids[2] = { t, term_new(0, TAG_ERA, 0, 0) };
        t = term_new_ctr(4u, kids, 2u);
      }
      AtpFtCell *x = ft_from_term(&a, t, 0);
      if (ref_atp_term_struct_hash(t) == atp_term_struct_hash(t) &&
          ref_atp_struct_hash(t) == atp_struct_hash(t) &&
          ref_mnf_hash(t) == mnf_hash(t) &&
          ref_mnf_hash_span(x) == mnf_hash_span(x) &&
          ref_mnf_term_is_ground(t) == mnf_term_is_ground(t)) {
        ok++;
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)WO_N);
  }

  TEST_BEGIN("atp_deep/pin-term-predicates-counts");
  {
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < WO_N; i++) {
      Term t = mk_random(seed);
      Term u = (i & 1u) ? copy_term(t) : mk_random(seed);
      Term sub = pick_sub(seed, t);
      u32 id = rng32(seed) % 12u;
      u32 cap = 1u + rng32(seed) % 64u;
      u32 ra = 0u, na = 0u;
      if (i & 2u) { ra = 3u; na = 3u; }
      ref_atp_symbol_count_upto(t, cap, &ra);
      atp_symbol_count_upto(t, cap, &na);
      u32 lab = rng32(seed) % 12u;
      if (ref_atp_term_has_var(t, id) == atp_term_has_var(t, id) &&
          ref_atp_vars_contained(t, u) == atp_vars_contained(t, u) &&
          ref_atp_symbol_count(t) == atp_symbol_count(t) && ra == na &&
          ref_atp_twee_sd_eq(t, u) == atp_twee_sd_eq(t, u) &&
          ref_atp_twee_sd_contains(t, sub) == atp_twee_sd_contains(t, sub) &&
          ref_atp_twee_sd_contains(u, sub) == atp_twee_sd_contains(u, sub) &&
          ref_atp_term_depth(t) == atp_term_depth(t) &&
          ref_atp_var_occurs(t, id) == atp_var_occurs(t, id) &&
          ref_atp_ladr_nest(t) == atp_ladr_nest(t) &&
          ref_atp_ladr_depth(t) == atp_ladr_depth(t) &&
          ref_atp_term_has_ctr(t, lab) == atp_term_has_ctr(t, lab) &&
          ref_atp_term_sym_bitmap(t) == atp_term_sym_bitmap(t) &&
          ref_atp_lr_sortieren_rec(t, u) == atp_lr_sortieren_rec(t, u)) {
        ok++;
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)WO_N);
  }

  TEST_BEGIN("atp_deep/pin-span-ladr-lr");
  {
    u32 ok = 0;
    for (u32 i = 0; i < WO_N; i++) {
      Term t = mk_random(seed);
      Term u = (i & 1u) ? copy_term(t) : mk_mutant(seed, t);
      AtpFtCell *x = ft_from_term(&a, t, 0);
      AtpFtCell *y = ft_from_term(&a, u, 0);
      if (ref_atp_ladr_nest_ft(x) == atp_ladr_nest_ft(x) &&
          ref_atp_ladr_depth_ft(x) == atp_ladr_depth_ft(x) &&
          ref_atp_lr_sortieren_rec_ft(x, y) == atp_lr_sortieren_rec_ft(x, y)) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)WO_N);
  }

  TEST_BEGIN("atp_deep/pin-unif-goal-measures");
  {
    u32 ok = 0;
    for (u32 i = 0; i < WO_N; i++) {
      Term t = mk_random(seed);
      Term u = (i & 1u) ? mk_mutant(seed, t) : mk_random(seed);
      u32 d = rng32(seed) % 9u;
      g_plain = 1u;
      Term pat = generalize(seed, pick_sub(seed, t));
      Term pr  = mk_small(seed);
      g_plain = 0u;
      RewriteSubst base;
      memset(&base, 0, sizeof base);
      if (i & 4u) {
        base.bindings[1] = mk_small(seed);
      }
      if (ref_atp_unif_measure_rec(t, u, d) == atp_unif_measure_rec(t, u, d) &&
          ref_atp_goal_match_cov(pat, t, base) == atp_goal_match_cov(pat, t, base) &&
          ref_atp_goal_doppel(pat, pr, t, u, base) == atp_goal_doppel(pat, pr, t, u, base)) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)WO_N);
  }

  TEST_BEGIN("atp_deep/pin-goal-subterms");
  {
    u32 ok = 0;
    for (u32 i = 0; i < WO_N / 4u; i++) {
      Term g = mk_random(seed);
      s->goal_sub_n = 0u;
      ref_atp_goal_sub_collect_rec(s, g);
      u32 rn = s->goal_sub_n;
      u64 rh[128];
      u16 rz[128];
      memcpy(rh, s->goal_sub_hash, rn * sizeof(u64));
      memcpy(rz, s->goal_sub_size, rn * sizeof(u16));
      s->goal_sub_n = 0u;
      atp_goal_sub_collect_rec(s, g);
      u8 same = (u8)(rn == s->goal_sub_n &&
                     memcmp(rh, s->goal_sub_hash, rn * sizeof(u64)) == 0 &&
                     memcmp(rz, s->goal_sub_size, rn * sizeof(u16)) == 0);
      for (u32 k = 0; k < 4u; k++) {
        Term t = (k == 0u) ? g : mk_random(seed);
        u32 b0 = rng32(seed) % 4u;
        same &= (u8)(ref_atp_goal_subterm_overlap_rec(s, t, b0) ==
                     atp_goal_subterm_overlap_rec(s, t, b0));
      }
      ok += same;
    }
    s->goal_sub_n = 0u;
    CHECK_EQ(ok, (u32)WO_N / 4u);
  }

  TEST_BEGIN("atp_deep/pin-masks-touch-goal");
  {
    u32 ok = 0;
    u8 sos = s->use_sos;
    s->use_sos = 1u;
    for (u32 i = 0; i < WO_N; i++) {
      g_wide = (i & 1u) ? 20u : 16u;
      Term t = mk_random(seed);
      g_wide = 16u;
      u32 rm[8] = {0}, nm[8] = {0};
      ref_atp_sym_mask_collect(t, rm);
      atp_sym_mask_collect(t, nm);
      for (u32 w = 0; w < 8u; w++) {
        s->goal_sym_mask[w] = rng32(seed) & rng32(seed) & rng32(seed);
      }
      if (memcmp(rm, nm, sizeof rm) == 0 &&
          ref_atp_term_touches_goal(s, t) == atp_term_touches_goal(s, t)) {
        ok++;
      }
    }
    s->use_sos = sos;
    memset(s->goal_sym_mask, 0, sizeof s->goal_sym_mask);
    CHECK_EQ(ok, (u32)WO_N);
  }

  TEST_BEGIN("atp_deep/pin-grounded-instance-heap-layout");
  {
    u32 ok = 0;
    Term minc = term_new_ctr(2u, NULL, 0u);
    for (u32 i = 0; i < WO_N; i++) {
      Term t = mk_random(seed);
      Term l = mk_random(seed);
      u64 h0 = HEAP_NEXT;
      Term r = ref_atp_grounded_instance(s, t, l, minc);
      u64 h1 = HEAP_NEXT;
      Term n = atp_grounded_instance(s, t, l, minc);
      u64 h2 = HEAP_NEXT;
      AtpFt ra, na;
      ft_init(&ra);
      ft_init(&na);
      AtpFtCell *rb = ft_alloc_persistent(&ra);
      AtpFtCell *nb = ft_alloc_persistent(&na);
      AtpFtCell *x  = ft_from_term(&a, t, 0);
      AtpFtCell *k  = ft_from_term(&a, l, 0);
      u8 rc = 0u, nc = 0u;
      AtpFtCell *rx = ref_ft_ground_absent_vars(&ra, x, k, 2u, &rc);
      AtpFtCell *nx = ft_ground_absent_vars(&na, x, k, 2u, &nc);
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n &&
          rc == nc && span_layout_same(rx, rb, nx, nb)) {
        ok++;
      }
      ft_destroy(&ra);
      ft_destroy(&na);
    }
    CHECK_EQ(ok, (u32)WO_N);
  }

  TEST_BEGIN("atp_deep/pin-ordered-rewrite-step-heap-layout");
  {
    // Every other term is an instance of a rule's lhs (from a seed of its
    // own, so the later pins draw the same terms): a random term alone
    // meets a rule in under a tenth of the steps.
    u32 ok = 0, fired = 0;
    u32 iseed = 0x2545f491u;
    for (u32 i = 0; i < WO_N; i++) {
      Term t = mk_random(seed);
      if ((i & 1u) != 0u && s->n_rules > 0u) {
        RewriteSubst sub;
        memset(&sub, 0, sizeof sub);
        for (u32 v = 0; v < 10u; v++) {
          sub.bindings[v] = mk_small(&iseed);
        }
        t = ref_thvm_subst_apply(s->lhs[(i >> 1) % s->n_rules], &sub);
      }
      u8 rf = 7u, nf = 9u;
      u64 h0 = HEAP_NEXT;
      Term r = ref_atp_ordered_rewrite_step(s, t, s->lhs, s->rhs, s->n_rules, &rf);
      u64 h1 = HEAP_NEXT;
      Term n = atp_ordered_rewrite_step(s, t, s->lhs, s->rhs, s->n_rules, &nf);
      u64 h2 = HEAP_NEXT;
      if (rf == nf && heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) {
        ok++;
      }
      fired += nf;
    }
    CHECK_EQ(ok, (u32)WO_N);
    CHECK(fired > WO_N / 10u);
  }

  TEST_BEGIN("atp_deep/pin-mgu-reducible-conn-successors");
  {
    u32 ok = 0;
    for (u32 i = 0; i < WO_N / 2u; i++) {
      Term peak = mk_random(seed);
      Term face = generalize(seed, peak);
      u64 h0 = HEAP_NEXT;
      u8 r = ref_atp_mgu_reducible(s, face, peak);
      u64 h1 = HEAP_NEXT;
      u8 n = atp_mgu_reducible(s, face, peak);
      u64 h2 = HEAP_NEXT;
      u8 same = (u8)(r == n && h1 - h0 == h2 - h1);
      Term rbuf[64], nbuf[64];
      u32 rn = 0u, nn = 0u;
      if (i & 1u) { rn = nn = 60u; for (u32 k = 0; k < 60u; k++) rbuf[k] = nbuf[k] = peak; }
      u64 g0 = HEAP_NEXT;
      (void)ref_atp_conn_successors(s, peak, peak, rbuf, &rn, 64u);
      u64 g1 = HEAP_NEXT;
      (void)atp_conn_successors(s, peak, peak, nbuf, &nn, 64u);
      u64 g2 = HEAP_NEXT;
      same &= (u8)(rn == nn && heap_regions_same(g0, g1, g1, g2));
      for (u32 k = 0; k < nn && same; k++) {
        same &= (u8)(reloc(rbuf[k], g0, g1, g1) == nbuf[k]);
      }
      ok += same;
    }
    CHECK_EQ(ok, (u32)WO_N / 2u);
  }

  TEST_BEGIN("atp_deep/pin-perm-subsumed");
  {
    u32 ok = 0, hits = 0;
    for (u32 i = 0; i < WO_N; i++) {
      // binary label-3 trees, some of them permutations of each other
      Term t = mk_random(seed);
      Term kids[2] = { t, mk_random(seed) };
      Term l = term_new_ctr(3u, kids, 2u);
      Term sw[2] = { kids[1], kids[0] };
      Term r = (i & 1u) ? term_new_ctr(3u, sw, 2u) : mk_mutant(seed, l);
      if (i % 5u == 0u) {
        Term inner[2] = { l, r };
        Term outer[2] = { r, l };
        l = term_new_ctr(3u, inner, 2u);
        r = term_new_ctr(3u, outer, 2u);
      }
      u8 rv = ref_atp_cp_perm_subsumed(l, r);
      u8 nv = atp_cp_perm_subsumed(l, r);
      if (rv == nv) {
        ok++;
      }
      hits += nv;
    }
    CHECK_EQ(ok, (u32)WO_N);
    CHECK(hits > WO_N / 10u);
  }

  TEST_BEGIN("atp_deep/pin-ic-rewrite-step");
  {
    atp_register_primitives();
    u32 ok = 0;
    for (u32 i = 0; i < WO_N / 4u; i++) {
      Term t = mk_random(seed);
      u64 h0 = HEAP_NEXT;
      Term r = ref_atp_ic_rewrite_step(t, s->lhs, s->rhs, s->n_rules);
      u64 h1 = HEAP_NEXT;
      Term n = atp_ic_rewrite_step(t, s->lhs, s->rhs, s->n_rules);
      u64 h2 = HEAP_NEXT;
      if (h1 - h0 == h2 - h1 && kbo_eq(r, n) && ((r == t) == (n == t))) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)WO_N / 4u);
  }

  ft_destroy(&a);
  thvm_atp_free(s);
}

static void deep_weight_order(u32 *seed) {
  AtpState *s = wo_state(seed);
  Term vals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term kid = term_new_ctr(9u, NULL, 0u);
    vals[v] = term_new_ctr(20u + v, &kid, v & 1u);
  }
  Term pat  = mk_deep(DEEP, 1u, term_new_fvr(4u), NULL, 1u);
  Term subj = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term near = mk_deep(DEEP, 1u, vals[3], vals, 1u);
  u32  size = term_size_iter(subj);
  AtpFt a;
  ft_init(&a);

  TEST_BEGIN("atp_deep/deep-weight-order-walks");
  {
    CHECK_EQ(atp_term_struct_hash(subj), atp_term_struct_hash(mk_deep(DEEP, 1u, vals[4], vals, 1u)));
    CHECK(atp_term_struct_hash(subj) != atp_term_struct_hash(near));
    CHECK(atp_struct_hash(subj) != atp_struct_hash(near));
    AtpFtCell *xs = ft_from_term(&a, subj, 0);
    CHECK_EQ(mnf_hash_span(xs), mnf_hash(subj));
    CHECK(mnf_term_is_ground(subj));
    CHECK(!mnf_term_is_ground(pat));
    CHECK(atp_term_has_var(pat, 4u));
    CHECK(!atp_term_has_var(pat, 7u));
    CHECK(atp_vars_contained(pat, pat));
    CHECK(!atp_vars_contained(pat, subj));
    CHECK_EQ(atp_symbol_count(subj), size);
    u32 acc = 0u;
    atp_symbol_count_upto(subj, size - 5u, &acc);
    CHECK_EQ(acc, size - 5u);
    CHECK(atp_twee_sd_eq(subj, mk_deep(DEEP, 1u, vals[4], vals, 1u)));
    CHECK(!atp_twee_sd_eq(subj, near));
    CHECK(atp_twee_sd_contains(subj, vals[4]));
    CHECK(!atp_twee_sd_contains(near, mk_deep(3u, 1u, vals[4], vals, 1u)));
    CHECK_EQ(atp_term_depth(subj), (u32)DEEP);       // leaves are depth 0
    CHECK_EQ(atp_ladr_depth(subj), (u32)DEEP);
    CHECK_EQ(atp_ladr_depth_ft(xs), (u32)DEEP);
    CHECK_EQ(atp_ladr_nest(subj), atp_ladr_nest_ft(xs));
    CHECK(atp_term_has_ctr(subj, 24u));
    CHECK(!atp_term_has_ctr(subj, 30u));
    CHECK_EQ(atp_term_sym_bitmap(subj),
             (1ull << 1) | (1ull << 2) | (1ull << 5) | (1ull << 6) | (1ull << 9) |
             (0x1full << 20));
    CHECK_EQ(atp_lr_sortieren_rec(subj, near), 1);
    CHECK_EQ(atp_lr_sortieren_rec_ft(xs, ft_from_term(&a, near, 0)), 1);
    CHECK_EQ(atp_unif_measure_rec(pat, subj, 3u), 0u);
    CHECK(atp_unif_measure_rec(subj, near, 3u) > 0u);
  }

  TEST_BEGIN("atp_deep/deep-goal-and-grounding");
  {
    RewriteSubst base;
    memset(&base, 0, sizeof base);
    CHECK_EQ(atp_goal_match_cov(pat, subj, base), size);
    CHECK_EQ(atp_goal_doppel(pat, vals[4], subj, vals[4], base), size + 1u);
    u32 mask[8] = {0};
    atp_sym_mask_collect(subj, mask);
    CHECK_EQ(mask[0], (1u << 1) | (1u << 2) | (1u << 5) | (1u << 6) | (1u << 9) |
                      (0x1fu << 20));
    s->use_sos = 1u;
    memset(s->goal_sym_mask, 0, sizeof s->goal_sym_mask);
    s->goal_sym_mask[0] = 1u << 24;
    CHECK(atp_term_touches_goal(s, subj));
    s->goal_sym_mask[0] = 1u << 30;
    CHECK(!atp_term_touches_goal(s, subj));
    s->use_sos = 0u;
    memset(s->goal_sym_mask, 0, sizeof s->goal_sym_mask);
    // The overlap walk re-tests groundness and size at every subterm,
    // quadratic in depth, so it runs on a 3,000-deep spine.
    Term mid = mk_deep(3000u, 1u, vals[4], vals, 1u);
    s->goal_sub_n = 0u;
    atp_goal_sub_collect_rec(s, vals[1]);
    CHECK_EQ(s->goal_sub_n, 1u);
    CHECK_EQ(atp_goal_subterm_overlap_rec(s, mid, 0u), 2u);
    s->goal_sub_n = 0u;
    Term minc = term_new_ctr(2u, NULL, 0u);
    Term vars10[5];
    for (u32 v = 0; v < 5u; v++) {
      vars10[v] = (v == 2u) ? term_new_fvr(2u) : minc;
    }
    Term keep = term_new_fvr(2u);
    CHECK(term_eq_iter(atp_grounded_instance(s, pat, keep, minc),
                       mk_deep(DEEP, 1u, minc, vars10, 1u)));
    u8 changed = 0u;
    AtpFtCell *g = ft_ground_absent_vars(&a, ft_from_term(&a, pat, 0),
                                         ft_from_term(&a, keep, 0), 2u, &changed);
    CHECK(changed);
    CHECK(ft_eq_term(g, mk_deep(DEEP, 1u, minc, vars10, 1u)));
  }

  TEST_BEGIN("atp_deep/deep-perm-conn-step");
  {
    // f(x, y) = f(y, x) applied at depth: the two sides are permutations.
    Term l = subj, r = subj;
    for (u32 k = 0; k < DEEP / 4u; k++) {
      Term c = term_new_ctr(5u, NULL, 0u);
      Term lk[2] = { l, c }, rk[2] = { c, r };
      l = term_new_ctr(3u, lk, 2u);
      r = term_new_ctr(3u, rk, 2u);
    }
    CHECK(atp_cp_perm_subsumed(l, r));
    CHECK(!atp_cp_perm_subsumed(l, near));
    Term buf[64];
    u32 nb = 0u;
    (void)atp_conn_successors(s, l, l, buf, &nb, 64u);
    CHECK(nb >= 1u);
    u8 fired = 0u;
    Term st = atp_ordered_rewrite_step(s, l, s->lhs, s->rhs, s->n_rules, &fired);
    CHECK(fired);
    CHECK(!kbo_eq(st, l));
    u8 mr = atp_mgu_reducible(s, pat, subj);
    CHECK(mr <= 1u);
  }

  ft_destroy(&a);
  thvm_atp_free(s);
}

// ---- gc.c / ground_join.c: ifeq, tgt, fold and ground-join walks -----------

// src/atp/gc.c:1410
static u8 ref_atp_ifeq_term_eq(Term a, Term b) {
  u8 ta = term_tag(a), tb = term_tag(b);
  if (ta != tb) return 0;
  if (ta == TAG_FVR) return term_ext(a) == term_ext(b);
  if (ta != TAG_CTR) return a == b;
  if (term_ext(a) != term_ext(b)) return 0;
  u32 n = term_ctr_n(a);
  if (n != term_ctr_n(b)) return 0;
  for (u32 i = 0; i < n; i++)
    if (!ref_atp_ifeq_term_eq(term_ctr_at(a, i), term_ctr_at(b, i))) return 0;
  return 1;
}

// src/atp/gc.c:1426
static u32 ref_atp_ifeq_guard_count(const AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return 0u;
  u32 n = term_ctr_n(t);
  u32 c = 0u;
  if ((n == 3u || n == 4u) && atp_ifeq_sym_is(s, term_ext(t)) &&
      !ref_atp_ifeq_term_eq(term_ctr_at(t, 0), term_ctr_at(t, 1))) c = 1u;
  for (u32 i = 0; i < n; i++)
    c += ref_atp_ifeq_guard_count(s, term_ctr_at(t, i));
  return c;
}

// src/atp/gc.c:1467
static void ref_atp_ifeq_var_mask(Term t, u64 *mask) {
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) { *mask |= 1ull << (term_ext(t) & 63u); return; }
  if (tg != TAG_CTR) return;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) ref_atp_ifeq_var_mask(term_ctr_at(t, i), mask);
}

// src/atp/gc.c:1722
static u64 ref_atp_ifeq_guard_hash_ft_rec(const AtpFtCell *t, AtpIfeqVarMap *vm) {
  if (ft_is_var(t)) {
    u32 cid = atp_ifeq_canon_var(vm, ft_var_id(t));
    return 0xfacefacefaceull ^ ((u64)cid * 0x100000001b3ull);
  }
  u32 n = t->arity;
  u64 h = 0xcbf29ce484222325ull ^ ((u64)ft_ctr_sym(t) * 0x100000001b3ull);
  const AtpFtCell *k = t->next;
  for (u32 i = 0; i < n; i++) {
    h ^= ref_atp_ifeq_guard_hash_ft_rec(k, vm);
    h *= 0x100000001b3ull;
    k = k->end->next;
  }
  return h ^ ((u64)n * 0x9e3779b97f4a7c15ull);
}

// src/atp/gc.c:2060
static void ref_atp_tgt_mask_collect(AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return;
  u32 lbl = term_ext(t);
  s->tgt_sym_mask[(lbl >> 6) & (ATP_TGT_MASK_WORDS - 1u)] |=
      1ull << (lbl & 63u);
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return;
  for (u32 i = 0; i < n; i++) ref_atp_tgt_mask_collect(s, term_ctr_at(t, i));
}

// src/atp/gc.c:2122
static void ref_atp_tgt_count_rec(const AtpState *s, Term t,
                                  u32 *gc, u32 *oc, u32 *vc) {
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) { (*vc)++; return; }
  if (tg != TAG_CTR) return;
  if (atp_tgt_sym_hit(s, term_ext(t))) (*gc)++; else (*oc)++;
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return;
  for (u32 i = 0; i < n; i++)
    ref_atp_tgt_count_rec(s, term_ctr_at(t, i), gc, oc, vc);
}

// src/atp/gc.c:2302
static u8 ref_atp_term_has_fold(const AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return 0u;
  u32 lbl = term_ext(t);
  if (lbl >= s->fold_lo && lbl < s->fold_hi) return 1u;
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return 0u;
  for (u32 i = 0; i < n; i++)
    if (ref_atp_term_has_fold(s, term_ctr_at(t, i))) return 1u;
  return 0u;
}

// src/atp/gc.c:3198
static int ref_atp_term_is_ground(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: return 0;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (!ref_atp_term_is_ground(term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

// src/atp/ground_join.c:57
static u8 ref_atp_gj_collect_vars(Term t, u32 *ids, u32 *n, u32 cap) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      for (u32 i = 0; i < *n; i++) {
        if (ids[i] == id) return 1;       // already seen
      }
      if (*n >= cap) return 0;            // too many distinct vars
      ids[*n] = id;
      (*n)++;
      return 1;
    }
    case TAG_CTR: {
      u32 k = term_ctr_n(t);
      for (u32 i = 0; i < k; i++) {
        if (!ref_atp_gj_collect_vars(term_ctr_at(t, i), ids, n, cap)) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

// src/atp/ground_join.c:156
static void ref_gj_form_addto(Term t, int sign, const KboConfig *cfg,
                              long long *c0, long long *var_coeff) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < KBO_MAX_VAR) var_coeff[id] += sign;
      return;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      *c0 += (long long)sign *
             (long long)((lab < cfg->n_labels) ? cfg->weights[lab] : 0);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        ref_gj_form_addto(term_ctr_at(t, i), sign, cfg, c0, var_coeff);
      return;
    }
    default: return;
  }
}

// src/atp/ground_join.c:651
static int ref_gj_has_var(Term t, u32 id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) if (ref_gj_has_var(term_ctr_at(t, i), id)) return 1;
      return 0;
    }
    default: return 0;
  }
}

// src/atp/ground_join.c:665
static u8 ref_gj_vars_subset(Term sub, Term sup) {
  switch (term_tag(sub)) {
    case TAG_FVR: return ref_gj_has_var(sup, term_ext(sub)) ? 1 : 0;
    case TAG_CTR: {
      u32 n = term_ctr_n(sub);
      for (u32 i = 0; i < n; i++)
        if (!ref_gj_vars_subset(term_ctr_at(sub, i), sup)) return 0;
      return 1;
    }
    default: return 1;
  }
}

// src/atp/ground_join.c:740
static u8 ref_gj_has_big_var(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) >= KBO_MAX_VAR ? 1u : 0u;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        if (ref_gj_has_big_var(term_ctr_at(t, i))) return 1u;
      return 0u;
    }
    default: return 0u;
  }
}

// src/atp/ground_join.c:870, with the top-position rule scan (unchanged)
// factored out as gj_rewrite_top.
static Term ref_gj_rewrite_step(AtpState *s, const GjModel *m, Term t,
                                const u32 *cp_vars, u32 n_cp_vars, u8 *fired,
                                Term protect) {
  if (protect == 0 && gj_nf_hit(t)) { *fired = 0; return t; }
  u8 top = 0u;
  Term red = gj_rewrite_top(s, m, t, cp_vars, n_cp_vars, protect, &top);
  if (top) { *fired = 1; return red; }
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) {
      if (protect == 0) gj_nf_add(t);
      *fired = 0;
      return t;
    }
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      u8 sub_fired = 0;
      Term rc = ref_gj_rewrite_step(s, m, children[i], cp_vars, n_cp_vars,
                                    &sub_fired, 0);
      if (sub_fired) {
        children[i] = rc;
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  if (protect == 0) gj_nf_add(t);
  *fired = 0;
  return t;
}

// src/atp/ground_join.c:1006, with the top-position scan (unchanged)
// factored out as gj_prenorm_top.
static Term ref_gj_prenorm_step(AtpState *s, Term t, u8 *fired) {
  u8 top = 0u;
  Term red = gj_prenorm_top(s, t, &top);
  if (top) { *fired = 1; return red; }
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      u8 sub_fired = 0;
      Term rc = ref_gj_prenorm_step(s, children[i], &sub_fired);
      if (sub_fired) {
        children[i] = rc;
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  *fired = 0;
  return t;
}

// src/atp/ground_join.c:1097
static u8 ref_gj_collect_consts(Term t, u32 *labs, u32 *n, u32 cap) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 k = term_ctr_n(t);
      if (k == 0) {
        u32 lab = term_ext(t);
        for (u32 i = 0; i < *n; i++) if (labs[i] == lab) return 1;
        if (*n >= cap) return 0;
        labs[(*n)++] = lab;
        return 1;
      }
      for (u32 i = 0; i < k; i++)
        if (!ref_gj_collect_consts(term_ctr_at(t, i), labs, n, cap)) return 0;
      return 1;
    }
    default: return 1;
  }
}

// src/atp/ground_join.c:1432
static int ref_gj_has_const(Term t, u32 lab) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n == 0) return term_ext(t) == lab;
      for (u32 i = 0; i < n; i++)
        if (ref_gj_has_const(term_ctr_at(t, i), lab)) return 1;
      return 0;
    }
    default: return 0;
  }
}

// A random term, often under a dispatcher node ifeq(a, b, then[, else])
// over label 3 or 7 whose guard pair is a fresh copy (resolved) half the
// time, and sometimes one level below the root.
static Term mk_ifeq_term(u32 *seed) {
  Term t = mk_random(seed);
  u32  r = rng32(seed);
  if ((r & 3u) == 0u) {
    return t;
  }
  Term a       = mk_small(seed);
  Term kids[4] = { a, (r & 4u) ? copy_term(a) : mk_small(seed), t,
                   mk_small(seed) };
  Term g       = term_new_ctr((r & 8u) ? 3u : 7u, kids, (r & 16u) ? 4u : 3u);
  if ((r & 32u) == 0u) {
    return g;
  }
  Term outer[2] = { mk_small(seed), g };
  return term_new_ctr(2u, outer, 2u);
}

// A fresh normal-form memo generation, as gj_normalize opens one.
static void gj_nf_fresh(void) {
  if (++g_gj_nf_cur == 0u) {
    memset(g_gj_nf_gen, 0, sizeof g_gj_nf_gen);
    g_gj_nf_cur = 1u;
  }
  g_gj_nf_n = 0u;
}

// The memo's live entries (slot, key) in slot order; the count.
static u32 gj_nf_snapshot(u32 *slots, Term *keys) {
  u32 n = 0u;
  for (u32 i = 0; i < GJ_NF_SIZE; i++) {
    if (g_gj_nf_gen[i] == g_gj_nf_cur) {
      slots[n] = i;
      keys[n]  = g_gj_nf_key[i];
      n++;
    }
  }
  return n;
}

static void sort_u32(u32 *a, u32 n) {
  for (u32 i = 1u; i < n; i++) {
    u32 v = a[i], j = i;
    while (j > 0u && a[j - 1u] > v) {
      a[j] = a[j - 1u];
      j--;
    }
    a[j] = v;
  }
}

enum { GC_N = 3000 };

static void pin_gc_gj(u32 *seed) {
  AtpState *s = wo_state(seed);
  s->ifeq_syms[0] = 3u;
  s->ifeq_syms[1] = 7u;
  s->n_ifeq_syms  = 2u;
  AtpFt a;
  ft_init(&a);

  TEST_BEGIN("atp_deep/pin-ifeq-tgt-fold-ground");
  {
    u32 ok = 0, guards = 0;
    g_wide = 20u;
    for (u32 i = 0; i < GC_N; i++) {
      Term t = mk_ifeq_term(seed);
      Term u = (i % 3u == 0u) ? copy_term(t)
             : (i % 3u == 1u) ? mk_mutant(seed, t) : mk_ifeq_term(seed);
      u64 rm = 0ull, nm = 0ull;
      ref_atp_ifeq_var_mask(t, &rm);
      atp_ifeq_var_mask(t, &nm);
      AtpFtCell *x = ft_from_term(&a, t, 0);
      AtpIfeqVarMap rv, nv;
      rv.n = 0u;
      nv.n = 0u;
      u64 rh = ref_atp_ifeq_guard_hash_ft_rec(x, &rv);
      u64 nh = atp_ifeq_guard_hash_ft_rec(x, &nv);
      u64 rmask[ATP_TGT_MASK_WORDS];
      memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
      ref_atp_tgt_mask_collect(s, t);
      memcpy(rmask, s->tgt_sym_mask, sizeof rmask);
      memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
      atp_tgt_mask_collect(s, t);
      int mask_same = memcmp(rmask, s->tgt_sym_mask, sizeof rmask) == 0;
      memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
      s->tgt_sym_mask[0] = ((u64)rng32(seed) << 32) | rng32(seed);
      u32 rg = 0u, ro = 0u, rvc = 0u, ng = 0u, no = 0u, nvc = 0u;
      ref_atp_tgt_count_rec(s, t, &rg, &ro, &rvc);
      atp_tgt_count_rec(s, t, &ng, &no, &nvc);
      s->fold_lo = rng32(seed) % 12u;
      s->fold_hi = s->fold_lo + rng32(seed) % 4u;
      u32 rgc = ref_atp_ifeq_guard_count(s, t);
      if (ref_atp_ifeq_term_eq(t, u) == atp_ifeq_term_eq(t, u) &&
          rgc == atp_ifeq_guard_count(s, t) &&
          rm == nm && rh == nh && rv.n == nv.n &&
          memcmp(rv.raw, nv.raw, sizeof rv.raw[0] * rv.n) == 0 &&
          mask_same && rg == ng && ro == no && rvc == nvc &&
          ref_atp_term_has_fold(s, t) == atp_term_has_fold(s, t) &&
          ref_atp_term_is_ground(t) == atp_term_is_ground(t)) {
        ok++;
      }
      guards += (rgc != 0u);
    }
    g_wide = 16u;
    s->fold_lo = 0u;
    s->fold_hi = 0u;
    memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
    CHECK_EQ(ok, (u32)GC_N);
    CHECK(guards > 0u);
  }

  TEST_BEGIN("atp_deep/pin-gj-collect-form-vars-consts");
  {
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < GC_N; i++) {
      Term t = mk_random(seed);
      Term u = (i & 1u) ? mk_mutant(seed, t) : generalize(seed, t);
      u32 rids[16], nids[16], rn = 0u, nn = 0u;
      if (i & 2u) {
        rids[0] = nids[0] = 3u;
        rn = nn = 1u;
      }
      u32 cap = rng32(seed) % 9u;
      u8 rc = ref_atp_gj_collect_vars(t, rids, &rn, cap);
      u8 nc = atp_gj_collect_vars(t, nids, &nn, cap);
      long long rc0 = 5, nc0 = 5;
      long long rvc[KBO_MAX_VAR], nvc[KBO_MAX_VAR];
      memset(rvc, 0, sizeof rvc);
      memset(nvc, 0, sizeof nvc);
      int sign = (i & 4u) ? -1 : 1;
      ref_gj_form_addto(t, sign, &g_wo_cfg, &rc0, rvc);
      gj_form_addto(t, sign, &g_wo_cfg, &nc0, nvc);
      u32 rl[20], nl[20], rk = 0u, nk = 0u;
      u32 lcap = rng32(seed) % 17u;
      u8 rcc = ref_gj_collect_consts(t, rl, &rk, lcap);
      u8 ncc = gj_collect_consts(t, nl, &nk, lcap);
      u32 id  = (rng32(seed) & 1u) ? rng32(seed) % 12u : 60u + rng32(seed) % 8u;
      u32 lab = rng32(seed) % 12u;
      if (rc == nc && rn == nn && memcmp(rids, nids, rn * sizeof(u32)) == 0 &&
          rc0 == nc0 && memcmp(rvc, nvc, sizeof rvc) == 0 &&
          rcc == ncc && rk == nk && memcmp(rl, nl, rk * sizeof(u32)) == 0 &&
          ref_gj_has_var(t, id) == gj_has_var(t, id) &&
          ref_gj_vars_subset(t, u) == gj_vars_subset(t, u) &&
          ref_gj_vars_subset(u, t) == gj_vars_subset(u, t) &&
          ref_gj_has_big_var(t) == gj_has_big_var(t) &&
          ref_gj_has_const(t, lab) == gj_has_const(t, lab)) {
        ok++;
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)GC_N);
  }

  // The rewrite and pre-normalization steps on an engine whose rules
  // include a surely-firing oriented one, f3(x0, x1) -> x0.
  AtpState *g = wo_state(seed);
  {
    Term xy[2] = { term_new_fvr(0u), term_new_fvr(1u) };
    (void)atp_push_rule(g, term_new_ctr(3u, xy, 2u), xy[0]);
  }
  GjModel m3;
  memset(&m3, 0, sizeof m3);
  m3.atom[0].is_const = 0u; m3.atom[0].id = 0u; m3.major[0] = 0u;
  m3.atom[1].is_const = 0u; m3.atom[1].id = 1u; m3.major[1] = 1u;
  m3.atom[2].is_const = 1u; m3.atom[2].id = 5u; m3.major[2] = 1u;
  m3.n = 3u;
  GjModel m0;
  memset(&m0, 0, sizeof m0);
  const u32 cpv[3] = { 0u, 1u, 2u };
  {
    // Warm the lazily built rule flags and top-scan index.
    u8 f = 0u;
    gj_sess_arm(&m0, g->kbo, cpv, 3u);
    gj_nf_fresh();
    (void)gj_rewrite_step(g, &m0, mk_random(seed), cpv, 3u, &f, 0);
    gj_sess_disarm();
  }

  TEST_BEGIN("atp_deep/pin-gj-rewrite-step-heap-layout");
  {
    static u32  rsl[GJ_NF_SIZE], nsl[GJ_NF_SIZE];
    static Term rky[GJ_NF_SIZE], nky[GJ_NF_SIZE];
    u32 ok = 0, fires = 0;
    for (u32 i = 0; i < GC_N; i++) {
      Term t = mk_random(seed);
      Term protect = (i % 5u == 0u) ? t : 0;
      const GjModel *mm = (i & 2u) ? &m3 : &m0;
      g_gj_orient_free = (u8)(i & 1u);
      gj_sess_arm(mm, g->kbo, cpv, 3u);
      gj_nf_fresh();
      u64 q0 = g_atp_gjp_top_q, p0 = g_atp_gjp_gate_pass;
      u64 h0 = HEAP_NEXT;
      u8   rf = 0u;
      Term r  = ref_gj_rewrite_step(g, mm, t, cpv, 3u, &rf, protect);
      u64 h1 = HEAP_NEXT;
      u64 q1 = g_atp_gjp_top_q, p1 = g_atp_gjp_gate_pass;
      u32 rk = gj_nf_snapshot(rsl, rky);
      gj_nf_fresh();
      u8   nf = 0u;
      Term n  = gj_rewrite_step(g, mm, t, cpv, 3u, &nf, protect);
      u64 h2 = HEAP_NEXT;
      u64 q2 = g_atp_gjp_top_q, p2 = g_atp_gjp_gate_pass;
      u32 nk = gj_nf_snapshot(nsl, nky);
      gj_sess_disarm();
      if (rf == nf && heap_regions_same(h0, h1, h1, h2) &&
          reloc(r, h0, h1, h1) == n && rk == nk &&
          memcmp(rsl, nsl, rk * sizeof(u32)) == 0 &&
          memcmp(rky, nky, rk * sizeof(Term)) == 0 &&
          q1 - q0 == q2 - q1 && p1 - p0 == p2 - p1) {
        ok++;
      }
      fires += nf;
    }
    g_gj_orient_free = 0u;
    CHECK_EQ(ok, (u32)GC_N);
    CHECK(fires > 0u);
  }

  TEST_BEGIN("atp_deep/pin-gj-prenorm-step-heap-layout");
  {
    u32 ok = 0, fires = 0;
    for (u32 i = 0; i < GC_N; i++) {
      Term t = mk_random(seed);
      u64  h0 = HEAP_NEXT;
      u8   rf = 0u;
      Term r  = ref_gj_prenorm_step(g, t, &rf);
      u64  h1 = HEAP_NEXT;
      u8   nf = 0u;
      Term n  = gj_prenorm_step(g, t, &nf);
      u64  h2 = HEAP_NEXT;
      if (rf == nf && heap_regions_same(h0, h1, h1, h2) &&
          reloc(r, h0, h1, h1) == n) {
        ok++;
      }
      fires += nf;
    }
    CHECK_EQ(ok, (u32)GC_N);
    CHECK(fires > 0u);
  }

  ft_destroy(&a);
  thvm_atp_free(g);
  thvm_atp_free(s);
}

static void deep_gc_gj(u32 *seed) {
  AtpState *s = wo_state(seed);
  s->ifeq_syms[0] = 25u;
  s->n_ifeq_syms  = 1u;
  Term vals[5], vvar[5];
  for (u32 v = 0; v < 5u; v++) {
    Term kid = term_new_ctr(9u, NULL, 0u);
    vals[v] = term_new_ctr(20u + v, &kid, v & 1u);
    vvar[v] = term_new_fvr(10u + v);
  }
  Term pat  = mk_deep(DEEP, 1u, term_new_fvr(4u), NULL, 1u);
  Term ren  = mk_deep(DEEP, 1u, vvar[4], vvar, 1u);   // pat, vars renamed
  Term subj = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term same = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term near = mk_deep(DEEP, 1u, vals[3], vals, 1u);
  u32  size = term_size_iter(subj);
  AtpFt a;
  ft_init(&a);

  TEST_BEGIN("atp_deep/deep-ifeq-tgt-fold-ground");
  {
    CHECK(atp_ifeq_term_eq(subj, same));
    CHECK(!atp_ifeq_term_eq(subj, near));
    // DEEP nested guards ifeq(c5, d, below), resolved (d == c5) at every
    // third level.
    Term chain = term_new_ctr(9u, NULL, 0u);
    u32  open  = 0u;
    for (u32 k = 0; k < DEEP; k++) {
      Term kids[3] = { term_new_ctr(5u, NULL, 0u),
                       term_new_ctr((k % 3u == 0u) ? 5u : 6u, NULL, 0u),
                       chain };
      chain = term_new_ctr(25u, kids, 3u);
      open += (k % 3u != 0u);
    }
    CHECK_EQ(atp_ifeq_guard_count(s, chain), open);
    u64 vm = 0ull;
    atp_ifeq_var_mask(pat, &vm);
    CHECK_EQ(vm, 0x1full);
    AtpIfeqVarMap v1, v2, v3, v4;
    v1.n = v2.n = v3.n = v4.n = 0u;
    AtpFtCell *xs = ft_from_term(&a, subj, 0);
    AtpFtCell *zs = ft_from_term(&a, near, 0);
    u64 hs = atp_ifeq_guard_hash_ft_rec(xs, &v1);
    CHECK_EQ(hs, atp_ifeq_guard_hash_ft_rec(ft_from_term(&a, same, 0), &v1));
    CHECK(hs != atp_ifeq_guard_hash_ft_rec(zs, &v1));
    // Canonical variable numbering: a renamed copy hashes alike.
    CHECK_EQ(atp_ifeq_guard_hash_ft_rec(ft_from_term(&a, pat, 0), &v2),
             atp_ifeq_guard_hash_ft_rec(ft_from_term(&a, ren, 0), &v3));
    CHECK_EQ(v2.n, 5u);
    CHECK_EQ(v3.n, 5u);
    // A 3,000-deep spine stays within the reference recursion's reach.
    Term mid = mk_deep(3000u, 1u, term_new_fvr(4u), NULL, 1u);
    AtpFtCell *ms = ft_from_term(&a, mid, 0);
    AtpIfeqVarMap r5;
    r5.n = 0u;
    CHECK_EQ(atp_ifeq_guard_hash_ft_rec(ms, &v4),
             ref_atp_ifeq_guard_hash_ft_rec(ms, &r5));
    memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
    atp_tgt_mask_collect(s, subj);
    CHECK_EQ(s->tgt_sym_mask[0],
             (1ull << 1) | (1ull << 2) | (1ull << 5) | (1ull << 6) |
             (1ull << 9) | (0x1full << 20));
    memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
    s->tgt_sym_mask[0] = (1ull << 1) | (1ull << 2);   // the spine labels
    u32 gc = 0u, oc = 0u, vc = 0u;
    atp_tgt_count_rec(s, subj, &gc, &oc, &vc);
    CHECK_EQ(gc, (u32)DEEP);
    CHECK_EQ(gc + oc, size);
    CHECK_EQ(vc, 0u);
    memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
    s->fold_lo = 24u;
    s->fold_hi = 25u;
    CHECK(atp_term_has_fold(s, subj));
    s->fold_lo = 30u;
    s->fold_hi = 32u;
    CHECK(!atp_term_has_fold(s, subj));
    s->fold_lo = 0u;
    s->fold_hi = 0u;
    CHECK(atp_term_is_ground(subj));
    CHECK(!atp_term_is_ground(pat));
  }

  TEST_BEGIN("atp_deep/deep-gj-walks");
  {
    u32 ids[8], n = 0u;
    CHECK(atp_gj_collect_vars(pat, ids, &n, 8u));
    CHECK_EQ(n, 5u);
    sort_u32(ids, n);
    for (u32 v = 0; v < 5u; v++) {
      CHECK_EQ(ids[v], v);
    }
    n = 0u;
    CHECK(!atp_gj_collect_vars(pat, ids, &n, 3u));
    CHECK_EQ(n, 3u);
    u32 labs[16], nl = 0u;
    CHECK(gj_collect_consts(subj, labs, &nl, 16u));
    CHECK_EQ(nl, 6u);
    sort_u32(labs, nl);
    static const u32 want[6] = { 5u, 6u, 9u, 20u, 22u, 24u };
    for (u32 k = 0; k < 6u; k++) {
      CHECK_EQ(labs[k], want[k]);
    }
    nl = 0u;
    CHECK(!gj_collect_consts(subj, labs, &nl, 2u));
    CHECK(gj_has_var(pat, 4u));
    CHECK(!gj_has_var(pat, 7u));
    CHECK(gj_vars_subset(pat, ren) == 0u);
    CHECK(gj_vars_subset(pat, pat));
    CHECK(gj_vars_subset(subj, pat));
    CHECK(!gj_vars_subset(pat, subj));
    CHECK(!gj_has_big_var(pat));
    CHECK(gj_has_big_var(mk_deep(DEEP, 1u, term_new_fvr(70u), NULL, 1u)));
    CHECK(gj_has_const(subj, 24u));
    CHECK(!gj_has_const(subj, 30u));
    long long c0 = 0;
    long long vc[KBO_MAX_VAR];
    memset(vc, 0, sizeof vc);
    gj_form_addto(subj, +1, &g_wo_cfg, &c0, vc);
    CHECK_EQ(c0, (long long)size);                    // unit weights
    c0 = 0;
    gj_form_addto(pat, -1, &g_wo_cfg, &c0, vc);
    long long vsum = 0;
    for (u32 v = 0; v < KBO_MAX_VAR; v++) {
      vsum += vc[v];
    }
    CHECK_EQ(c0 + vsum, -(long long)term_size_iter(pat));
  }

  TEST_BEGIN("atp_deep/deep-gj-rewrite-prenorm");
  {
    // One oriented rule c31 -> c30 (precedence = label) whose only
    // redex is the bottom of a deep spine: the leftmost-outermost step
    // walks the whole spine and rebuilds it.  The rewrite step's indexed
    // top scan flattens every subterm it visits (quadratic in depth), so
    // that walk runs on a 10,000-deep spine; the pre-normalization step
    // scans rules linearly and runs on DEEP.
    AtpState *g = thvm_atp_init(&g_wo_cfg, 100u);
    (void)atp_push_rule(g, term_new_ctr(31u, NULL, 0u),
                        term_new_ctr(30u, NULL, 0u));
    CHECK(g->r_orient[0]);
    enum { GJ_DEEP = 10000 };
    Term c31   = term_new_ctr(31u, NULL, 0u);
    Term c30   = term_new_ctr(30u, NULL, 0u);
    Term chain = mk_deep(GJ_DEEP, 1u, c31, vals, 1u);
    Term want  = mk_deep(GJ_DEEP, 1u, c30, vals, 1u);
    GjModel em;
    memset(&em, 0, sizeof em);
    const u32 cpv[1] = { 0u };
    g_gj_orient_free = 1u;
    gj_sess_arm(&em, g->kbo, cpv, 0u);
    gj_nf_fresh();
    u8   fired = 0u;
    Term out   = gj_rewrite_step(g, &em, chain, cpv, 0u, &fired, 0);
    gj_sess_disarm();
    g_gj_orient_free = 0u;
    CHECK(fired);
    CHECK(term_eq_iter(out, want));
    Term dchain = mk_deep(DEEP, 1u, c31, vals, 1u);
    fired = 0u;
    out = gj_prenorm_step(g, dchain, &fired);
    CHECK(fired);
    CHECK(term_eq_iter(out, mk_deep(DEEP, 1u, c30, vals, 1u)));
    fired = 1u;
    out = gj_prenorm_step(g, subj, &fired);
    CHECK(!fired);
    CHECK(out == subj);
    thvm_atp_free(g);
  }

  ft_destroy(&a);
  thvm_atp_free(s);
}

// ---- overlap_index.c / wm_order.c / narrow.c: indexes and goal queues ------

// src/atp/overlap_index.c:101
static void ref_atp_cp_index_collect_subtree(u32 node) {
  AtpRuleIndex *ix = g_atp_cp_ix;
  atp_cp_index_leaf(node);
  for (u32 c = ix->nodes[node].child; c != ATP_DTREE_NIL;
       c = ix->nodes[c].sibling) {
    if (g_atp_cp_overflow) return;
    ref_atp_cp_index_collect_subtree(c);
  }
}

// src/atp/overlap_index.c:114
static void ref_atp_cp_index_descend(u32 node, u32 pos, u32 qend) {
  AtpRuleIndex *ix = g_atp_cp_ix;
  if (pos == qend) { atp_cp_index_leaf(node); return; }
  u32 qsym = g_atp_cp_qflatsym[pos];
  u8  q_is_star = (qsym >= ATP_DTREE_STAR_BASE && qsym < ATP_DTREE_CTR_BASE);
  u32 sz   = g_atp_cp_qsubsz[pos];
  for (u32 c = ix->nodes[node].child; c != ATP_DTREE_NIL;
       c = ix->nodes[c].sibling) {
    if (g_atp_cp_overflow) return;
    u32 csym = ix->nodes[c].sym;
    if (csym >= ATP_DTREE_STAR_BASE && csym < ATP_DTREE_CTR_BASE) {
      ref_atp_cp_index_descend(c, pos + sz, qend);
    } else if (csym == qsym) {
      ref_atp_cp_index_descend(c, pos + 1u, qend);
    }
  }
  if (q_is_star) {
    ref_atp_cp_index_collect_subtree(node);
  }
}

// src/atp/overlap_index.c:274
static void ref_atp_cp_subindex_insert(AtpRuleIndex *ix, Term t, u32 rule,
                                       AtpDTreeVarMap *vm) {
  if (term_tag(t) == TAG_CTR) {
    u32 node = atp_ri_insert_term(ix, ix->root, t, vm);
    u32 rec  = atp_ri_rec_new(ix);
    ix->recs[rec].rule       = rule;
    ix->recs[rec].next       = ix->nodes[node].rec_head;
    ix->nodes[node].rec_head = rec;
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      ref_atp_cp_subindex_insert(ix, term_ctr_at(t, i), rule, vm);
    }
  }
}

// src/atp/wm_order.c:266
static u32 ref_wmo_term_depth(Term t) {
  if (term_tag(t) != TAG_CTR) return 1;
  u32 n = term_ctr_n(t), d = 0;
  for (u32 i = 0; i < n; i++) {
    u32 cd = ref_wmo_term_depth(term_ctr_at(t, i));
    if (cd > d) d = cd;
  }
  return d + 1;
}

// src/atp/wm_order.c:360
static u32 ref_wmo_term_depth_ft(const struct AtpFtCell *c) {
  if ((c->sym & WF_VAR_BIT) != 0u || c->arity == 0u) return 1;
  u32 d = 0;
  const struct AtpFtCell *kid = c->next;
  for (u32 i = 0; i < (u32)c->arity; i++) {
    u32 cd = ref_wmo_term_depth_ft(kid);
    if (cd > d) d = cd;
    kid = kid->end->next;
  }
  return d + 1;
}

// src/atp/wm_order.c:2116
static u8 ref_wmo_term_is_ground(Term t) {
  if (t == 0) return 1u;
  if (term_tag(t) == TAG_FVR) return 0u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!ref_wmo_term_is_ground(term_ctr_at(t, i))) return 0u;
    }
  }
  return 1u;
}

// src/atp/wm_order.c:2929
static void ref_wmo_var_counts(Term t, u16 *cnt) {
  if (term_tag(t) == TAG_FVR) {
    u32 v = term_ext(t);
    if (v < WMO_VAR_CNT_CAP && cnt[v] < 0xffffu) cnt[v]++;
    return;
  }
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) ref_wmo_var_counts(term_ctr_at(t, i), cnt);
  }
}

// src/atp/wm_order.c:1712
static void ref_wmo_dfs(WmoDfs *d, void *n_raw, u8 is_leaf, u32 qi, u32 spos,
                        const WmoUnif *u, u32 depth_guard) {
  if (depth_guard == 0u) return;
  if (is_leaf) {
    WmoLeaf *leaf = (WmoLeaf *)n_raw;
    WmoUnif u2;
    wmo_unif_copy(&u2, u);
    u32 si = (spos != 0xffffffffu) ? spos : leaf->hang + 1u;
    if (wmo_unify_rest(d, leaf, si, qi, &u2)) wmo_dfs_emit(d, leaf);
    return;
  }
  WmoNode *n = (WmoNode *)n_raw;
  if (qi >= d->q_len) return;
  const WmoCell *qc = &d->q[qi];
  if (!qc->is_var) {
    for (u32 k = 0; k < n->n_kids; k++) {
      if (!n->kids[k].sym.is_var && n->kids[k].sym.sym == qc->sym) {
        ref_wmo_dfs(d, n->kids[k].child, n->kids[k].is_leaf, qi + 1u,
                    0xffffffffu, u, depth_guard - 1u);
      }
    }
    WmoKid vkids[32];
    u32 nv = 0;
    for (u32 k = 0; k < n->n_kids && nv < 32u; k++) {
      if (n->kids[k].sym.is_var) vkids[nv++] = n->kids[k];
    }
    if (nv > 1u) wmo_sort_vkids_vardesc(vkids, nv);
    u32 qe = wmo_sub_end(d->q, qi);
    for (u32 k = 0; k < nv; k++) {
      WmoUnif u2;
      wmo_unif_copy(&u2, u);
      if (wmo_unify_var(1u, vkids[k].sym.sym, d->q + qi, qe - qi, 0u,
                        &u2, 64u)) {
        ref_wmo_dfs(d, vkids[k].child, vkids[k].is_leaf, qe, 0xffffffffu,
                    &u2, depth_guard - 1u);
      }
    }
  } else {
    WmoKid vkids[32];
    u32 nv = 0;
    for (u32 k = 0; k < n->n_kids && nv < 32u; k++) {
      if (n->kids[k].sym.is_var) vkids[nv++] = n->kids[k];
    }
    if (nv > 1u) wmo_sort_vkids_vardesc(vkids, nv);
    for (u32 k = 0; k < nv; k++) {
      WmoUnif u2;
      wmo_unif_copy(&u2, u);
      WmoCell vcell = vkids[k].sym;
      if (wmo_unify_cells(&d->q[qi], 1u, 0u, &vcell, 1u, 1u, &u2, 64u)) {
        ref_wmo_dfs(d, vkids[k].child, vkids[k].is_leaf, qi + 1u,
                    0xffffffffu, &u2, depth_guard - 1u);
      }
    }
    for (WmoEntry *e = n->exits; e != NULL; e = e->next) {
      if (wmo_jump_dedup_on()) {
        u8 shadowed = 0u;
        for (WmoEntry *e2 = e->next; e2 != NULL; e2 = e2->next) {
          if (e2->ziel == e->ziel && e2->ziel_leaf == e->ziel_leaf &&
              e2->sub_len == e->sub_len &&
              wmo_cells_eq(e2->sub, e->sub, e->sub_len)) {
            shadowed = 1u;
            break;
          }
        }
        if (shadowed) continue;
      }
      WmoUnif u2;
      wmo_unif_copy(&u2, u);
      if (wmo_unify_var(0u, qc->sym, e->sub, e->sub_len, 1u, &u2,
                        64u)) {
        u32 land = wmo_node_depth(e->start) + e->sub_len;
        ref_wmo_dfs(d, e->ziel, e->ziel_leaf, qi + 1u,
                    e->ziel_leaf ? land : 0xffffffffu, &u2, depth_guard - 1u);
      }
    }
  }
}

// src/atp/narrow.c:617
static u32 ref_cgq_term_size(Term t) {
  if (term_tag(t) != TAG_CTR) return 1u;
  u32 n = term_ctr_n(t);
  u32 sz = 1u;
  for (u32 i = 0; i < n; i++) sz += ref_cgq_term_size(term_ctr_at(t, i));
  return sz;
}

// src/atp/narrow.c:625
static u32 ref_cgq_term_max_var(Term t) {
  if (term_tag(t) == TAG_FVR) return term_ext(t) + 1u;
  if (term_tag(t) != TAG_CTR) return 0u;
  u32 n = term_ctr_n(t);
  u32 m = 0u;
  for (u32 i = 0; i < n; i++) {
    u32 v = ref_cgq_term_max_var(term_ctr_at(t, i));
    if (v > m) m = v;
  }
  return m;
}

// src/atp/narrow.c:637
static u64 ref_cgq_term_hash(Term t, u64 h) {
  h = (h ^ ((u64)term_tag(t) << 32 ^ (u64)term_ext(t)))
      * 0x100000001B3ull;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    h = (h ^ n) * 0x100000001B3ull;
    for (u32 i = 0; i < n; i++) h = ref_cgq_term_hash(term_ctr_at(t, i), h);
  }
  return h;
}

// An empty overlap index, as atp_cp_subindex_rebuild starts one.
static AtpRuleIndex *cp_index_new(void) {
  AtpRuleIndex *ix = atp_ri_new();
  ix->n_nodes = 0u;
  ix->n_recs  = 0u;
  ix->root    = atp_ri_node_new(ix, ATP_DTREE_NIL);
  return ix;
}

// The candidates the overlap index yields for query face `q`: as
// atp_cp_subindex_collect_face (`whole`, one descent from the root over
// the whole face) or atp_cp_index_collect_face (one descent per non-var
// position).  `ref` picks the recursive descent.  Returns the count (or
// 0xffffffff when the face does not flatten) and the overflow flag.
static u32 cp_index_query(AtpRuleIndex *ix, Term q, u8 whole, u8 ref,
                          u32 *out, u8 *ovf) {
  static Term qflat[ATP_RI_FLAT_CAP];
  static u32  qsubsz[ATP_RI_FLAT_CAP];
  static u32  qflatsym[ATP_RI_FLAT_CAP];
  u8  folded = 0u;
  u32 pos    = 0u;
  if (!atp_ri_flatten(q, qflat, qsubsz, qflatsym, &folded, ATP_RI_FLAT_CAP,
                      &pos)) {
    return 0xffffffffu;
  }
  atp_cp_collect_begin();
  g_atp_cp_ix       = ix;
  g_atp_cp_qflat    = qflat;
  g_atp_cp_qsubsz   = qsubsz;
  g_atp_cp_qflatsym = qflatsym;
  if (whole) {
    if (ref) {
      ref_atp_cp_index_descend(ix->root, 0u, pos);
    } else {
      atp_cp_index_descend(ix->root, 0u, pos);
    }
  } else {
    for (u32 p = 0; p < pos && !g_atp_cp_overflow; p++) {
      u32 qsym = qflatsym[p];
      if (qsym >= ATP_DTREE_STAR_BASE && qsym < ATP_DTREE_CTR_BASE) {
        continue;
      }
      if (ref) {
        ref_atp_cp_index_descend(ix->root, p, p + qsubsz[p]);
      } else {
        atp_cp_index_descend(ix->root, p, p + qsubsz[p]);
      }
    }
  }
  memcpy(out, g_atp_cp_cand, (size_t)g_atp_cp_ncand * sizeof(u32));
  *ovf = g_atp_cp_overflow;
  return g_atp_cp_ncand;
}

// A unary chain s^depth(bottom) over label `s`.
static Term mk_chain(u32 depth, u32 s, Term bottom) {
  Term t = bottom;
  for (u32 k = 0; k < depth; k++) {
    t = term_new_ctr(s, &t, 1u);
  }
  return t;
}

// A random term of at most `budget` nodes.
static Term mk_sized(u32 *seed, u32 budget) {
  return mk_term(seed, 6u, &budget);
}

enum { IX_N = 3000, IX_FACES = 300, WMO_FACES = 400, WMO_Q = 2000 };

static void pin_index_queue(u32 *seed) {
  AtpState *s = wo_state(seed);
  atp_cp_seen_ensure(s);

  TEST_BEGIN("atp_deep/pin-cp-subindex-insert");
  AtpRuleIndex *rx = cp_index_new();
  AtpRuleIndex *nx = cp_index_new();
  {
    u32 ok = 0;
    for (u32 i = 0; i < IX_FACES; i++) {
      Term f = (i % 3u == 0u) ? mk_small(seed)
                              : generalize(seed, pick_sub(seed, mk_random(seed)));
      AtpDTreeVarMap rv, nv;
      vm_reset(&rv);
      vm_reset(&nv);
      ref_atp_cp_subindex_insert(rx, f, i, &rv);
      atp_cp_subindex_insert(nx, f, i, &nv);
      if (memcmp(&rv, &nv, sizeof rv) == 0) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)IX_FACES);
    CHECK_EQ(rx->n_nodes, nx->n_nodes);
    CHECK(memcmp(rx->nodes, nx->nodes, (size_t)rx->n_nodes * sizeof(AtpDTreeNode)) == 0);
    CHECK_EQ(rx->n_recs, nx->n_recs);
    CHECK(memcmp(rx->recs, nx->recs, (size_t)rx->n_recs * sizeof(AtpRiRec)) == 0);
  }

  TEST_BEGIN("atp_deep/pin-cp-index-descend");
  {
    static u32 rc[ATP_CP_MAXCAND], nc[ATP_CP_MAXCAND];
    u32 ok = 0, hits = 0;
    for (u32 i = 0; i < IX_N; i++) {
      Term q = (i & 1u) ? mk_random(seed) : generalize(seed, mk_random(seed));
      u8   whole = (u8)((i >> 1) & 1u);
      u8   ro = 0u, no = 0u;
      u32  rn = cp_index_query(nx, q, whole, 1u, rc, &ro);
      u32  nn = cp_index_query(nx, q, whole, 0u, nc, &no);
      if (rn == nn && ro == no &&
          (rn == 0xffffffffu || memcmp(rc, nc, (size_t)rn * sizeof(u32)) == 0)) {
        ok++;
      }
      hits += (rn != 0xffffffffu && rn > 0u);
    }
    CHECK_EQ(ok, (u32)IX_N);
    CHECK(hits > 0u);
  }
  atp_ri_free(rx);
  atp_ri_free(nx);

  TEST_BEGIN("atp_deep/pin-wmo-term-walks");
  {
    AtpFt a;
    ft_init(&a);
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < IX_N; i++) {
      Term t = mk_random(seed);
      AtpFtCell *x = ft_from_term(&a, t, 0);
      u16 rcnt[WMO_VAR_CNT_CAP], ncnt[WMO_VAR_CNT_CAP];
      memset(rcnt, 0, sizeof rcnt);
      memset(ncnt, 0, sizeof ncnt);
      ref_wmo_var_counts(t, rcnt);
      wmo_var_counts(t, ncnt);
      if (ref_wmo_term_depth(t) == wmo_term_depth(t) &&
          ref_wmo_term_depth_ft(x) == wmo_term_depth_ft(x) &&
          ref_wmo_term_is_ground(t) == wmo_term_is_ground(t) &&
          memcmp(rcnt, ncnt, sizeof rcnt) == 0) {
        ok++;
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)IX_N);
    CHECK_EQ(ref_wmo_term_is_ground(0), wmo_term_is_ground(0));
    ft_destroy(&a);
  }

  TEST_BEGIN("atp_deep/pin-wmo-dfs");
  {
    AtpWmOrder *w = atp_wmo_new();
    WmoCell cells[WMO_MAX_CELLS];
    for (u32 k = 0; k < WMO_FACES; k++) {
      Term f = (k % 3u == 0u) ? mk_small(seed)
                              : generalize(seed, pick_sub(seed, mk_sized(seed, 200u)));
      u32 n = wmo_face_cells(f, cells, WMO_MAX_CELLS);
      if (n == 0u) {
        continue;
      }
      wmo_register(w, k + 1u, (u8)(k & 1u), (u8)((k >> 1) & 1u), cells, n,
                   wmo_term_depth(f), 0u);
    }
    static WmoLeaf *ro[WMO_TOPS_ARR_CAP], *no[WMO_TOPS_ARR_CAP];
    u32 ok = 0, runs = 0, hits = 0;
    for (u32 i = 0; i < WMO_Q; i++) {
      Term q = (i & 1u) ? mk_sized(seed, 200u)
                        : generalize(seed, mk_sized(seed, 200u));
      WmoCell qc[WMO_MAX_CELLS];
      u32 qn = wmo_face_cells(q, qc, WMO_MAX_CELLS);
      for (u32 tree = 0; tree < 2u && qn != 0u; tree++) {
        if (w->tree[tree].root == NULL) {
          continue;
        }
        WmoUnif u;
        memset(&u, 0, sizeof u);
        WmoDfs rd = { qc, qn, ro, 0u, WMO_TOPS_ARR_CAP };
        WmoDfs nd = { qc, qn, no, 0u, WMO_TOPS_ARR_CAP };
        ref_wmo_dfs(&rd, w->tree[tree].root, 0u, 0u, 0xffffffffu, &u, 4000u);
        wmo_dfs(&nd, w->tree[tree].root, 0u, 0u, 0xffffffffu, &u, 4000u);
        runs++;
        if (rd.n_out == nd.n_out &&
            memcmp(ro, no, (size_t)rd.n_out * sizeof(WmoLeaf *)) == 0) {
          ok++;
        }
        hits += (nd.n_out > 0u);
      }
    }
    CHECK_EQ(ok, runs);
    CHECK(runs > 0u);
    CHECK(hits > 0u);
  }

  TEST_BEGIN("atp_deep/pin-cgq-term-walks");
  {
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < IX_N; i++) {
      Term t  = mk_random(seed);
      u64  h0 = ((u64)rng32(seed) << 32) | rng32(seed);
      if (ref_cgq_term_size(t) == cgq_term_size(t) &&
          ref_cgq_term_max_var(t) == cgq_term_max_var(t) &&
          ref_cgq_term_hash(t, h0) == cgq_term_hash(t, h0)) {
        ok++;
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)IX_N);
  }

  thvm_atp_free(s);
}

// ref_wmo_dfs (about 4 KB of C stack a level) against wmo_dfs's
// result, run on the reference stack from inside a deep block.
typedef struct {
  const WmoCell *qc;
  u32            qn;
  void          *root;
  const WmoUnif *u;
  WmoLeaf      **out;
  u32            n_out;
} WmoDfsVsRef;

static void wmo_dfs_vs_ref(void *p) {
  WmoDfsVsRef     *c = (WmoDfsVsRef *)p;
  static WmoLeaf *rout[WMO_TOPS_ARR_CAP];
  WmoDfs rd = { c->qc, c->qn, rout, 0u, WMO_TOPS_ARR_CAP };
  ref_wmo_dfs(&rd, c->root, 0u, 0u, 0xffffffffu, c->u, 4000u);
  CHECK_EQ(rd.n_out, c->n_out);
  CHECK(memcmp(rout, c->out, (size_t)c->n_out * sizeof(WmoLeaf *)) == 0);
}

static void deep_index_queue(u32 *seed) {
  AtpState *s = wo_state(seed);
  atp_cp_seen_ensure(s);
  Term vals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term kid = term_new_ctr(9u, NULL, 0u);
    vals[v] = term_new_ctr(20u + v, &kid, v & 1u);
  }
  Term pat  = mk_deep(DEEP, 1u, term_new_fvr(4u), NULL, 1u);
  Term subj = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term same = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term near = mk_deep(DEEP, 1u, vals[3], vals, 1u);
  AtpFt a;
  ft_init(&a);

  TEST_BEGIN("atp_deep/deep-cp-subindex");
  {
    // Every CTR subterm of a face is its own tree path, so the insert is
    // quadratic in depth: a 10,000-deep chain.
    enum { IX_DEEP = 10000 };
    Term c5    = term_new_ctr(5u, NULL, 0u);
    Term chain = mk_chain(IX_DEEP, 3u, c5);
    Term vchn  = mk_chain(IX_DEEP, 3u, term_new_fvr(0u));
    AtpRuleIndex *ix = cp_index_new();
    AtpDTreeVarMap vm;
    vm_reset(&vm);
    atp_cp_subindex_insert(ix, chain, 7u, &vm);
    vm_reset(&vm);
    atp_cp_subindex_insert(ix, vchn, 8u, &vm);
    CHECK_EQ(ix->n_recs, 2u * IX_DEEP + 1u);
    static u32 out[ATP_CP_MAXCAND];
    u8 ovf = 1u;
    u32 n = cp_index_query(ix, chain, 1u, 0u, out, &ovf);
    CHECK_EQ(n, 2u);
    CHECK(!ovf);
    sort_u32(out, n);
    CHECK_EQ(out[0], 7u);
    CHECK_EQ(out[1], 8u);
    // A variable bottom harvests the stored subtree under it.
    n = cp_index_query(ix, mk_chain(IX_DEEP, 3u, term_new_fvr(5u)), 1u, 0u,
                       out, &ovf);
    sort_u32(out, n);
    CHECK_EQ(n, 2u);
    CHECK_EQ(out[0], 7u);
    CHECK_EQ(out[1], 8u);
    // One descent per position: every suffix of the chain meets a path.
    n = cp_index_query(ix, chain, 0u, 0u, out, &ovf);
    sort_u32(out, n);
    CHECK_EQ(n, 2u);
    // A 1,000-deep instance agrees with the recursive descent.
    static u32 rout[ATP_CP_MAXCAND];
    Term mid = mk_chain(1000u, 3u, c5);
    u8 rovf = 1u;
    u32 rn = cp_index_query(ix, mid, 0u, 1u, rout, &rovf);
    n = cp_index_query(ix, mid, 0u, 0u, out, &ovf);
    CHECK_EQ(rn, n);
    CHECK(memcmp(rout, out, (size_t)n * sizeof(u32)) == 0);
    atp_ri_free(ix);
  }

  TEST_BEGIN("atp_deep/deep-wmo-term-walks");
  {
    CHECK_EQ(wmo_term_depth(subj), (u32)DEEP + 1u);
    CHECK_EQ(wmo_term_depth(pat), (u32)DEEP + 1u);
    AtpFtCell *xs = ft_from_term(&a, subj, 0);
    CHECK_EQ(wmo_term_depth_ft(xs), (u32)DEEP + 1u);
    CHECK(wmo_term_is_ground(subj));
    CHECK(!wmo_term_is_ground(pat));
    u16 cnt[WMO_VAR_CNT_CAP];
    memset(cnt, 0, sizeof cnt);
    wmo_var_counts(pat, cnt);
    u32 total = 0u;
    for (u32 v = 0; v < WMO_VAR_CNT_CAP; v++) {
      total += cnt[v];
    }
    CHECK_EQ(total, (u32)((DEEP + 2u) / 3u) + 1u);
  }

  TEST_BEGIN("atp_deep/deep-wmo-dfs");
  {
    // Faces s^k(x) for k = 1..40, 700, 759 against chain queries up to
    // the 768-cell face cap: the descent opens one frame per query cell.
    AtpWmOrder *w = atp_wmo_new();
    WmoCell cells[WMO_MAX_CELLS];
    static const u32 lens[3] = { 700u, 759u, 0u };
    u32 nf = 0u;
    for (u32 k = 1u; k <= 42u; k++) {
      u32 len = (k <= 40u) ? k : lens[k - 41u];
      Term f = mk_chain(len, 3u, term_new_fvr(0u));
      u32  n = wmo_face_cells(f, cells, WMO_MAX_CELLS);
      CHECK_EQ(n, len + 1u);
      wmo_register(w, k, 0u, 0u, cells, n, wmo_term_depth(f), 0u);
      nf++;
    }
    static WmoLeaf *out[WMO_TOPS_ARR_CAP];
    WmoCell qc[WMO_MAX_CELLS];
    WmoUnif u;
    memset(&u, 0, sizeof u);
    static const u32 qlen[3]  = { 759u, 700u, 200u };
    static const u32 qwant[3] = { 42u, 41u, 40u };
    for (u32 j = 0; j < 3u; j++) {
      Term q  = mk_chain(qlen[j], 3u, term_new_ctr(5u, NULL, 0u));
      u32  qn = wmo_face_cells(q, qc, WMO_MAX_CELLS);
      WmoDfs d = { qc, qn, out, 0u, WMO_TOPS_ARR_CAP };
      wmo_dfs(&d, w->tree[0].root, 0u, 0u, 0xffffffffu, &u, 4000u);
      CHECK_EQ(d.n_out, qwant[j]);
      if (j == 2u) {
        WmoDfsVsRef rc = { qc, qn, w->tree[0].root, &u, out, d.n_out };
        deep_stack_run(REF_STACK_BYTES, wmo_dfs_vs_ref, &rc);
      }
    }
    CHECK_EQ(nf, 42u);
  }

  TEST_BEGIN("atp_deep/deep-cgq-term-walks");
  {
    CHECK_EQ(cgq_term_size(subj), term_size_iter(subj));
    CHECK_EQ(cgq_term_max_var(pat), 5u);
    CHECK_EQ(cgq_term_max_var(subj), 0u);
    CHECK_EQ(cgq_term_hash(subj, 7u), cgq_term_hash(same, 7u));
    CHECK(cgq_term_hash(subj, 7u) != cgq_term_hash(near, 7u));
    Term mid = mk_deep(3000u, 1u, vals[4], vals, 1u);
    CHECK_EQ(cgq_term_hash(mid, 7u), ref_cgq_term_hash(mid, 7u));
    CHECK_EQ(cgq_term_size(mid), ref_cgq_term_size(mid));
  }

  ft_destroy(&a);
  thvm_atp_free(s);
}

// ---- ac.c / ft_ac_match.c / ft_ac_eq.c: AC flattening, canon, equality -----

// src/atp/ac.c:94
static u8 ref_atp_ac_flatten_under(Term t, u32 top_label,
                                   Term *out, u32 *n, u32 cap) {
  if (term_tag(t) == TAG_CTR && term_ext(t) == top_label) {
    u32 n_kids = term_ctr_n(t);
    for (u32 i = 0; i < n_kids; i++) {
      if (!ref_atp_ac_flatten_under(term_ctr_at(t, i), top_label,
                                    out, n, cap)) {
        return 0;
      }
    }
    return 1;
  }
  if (*n >= cap) return 0;
  out[*n] = t;
  *n += 1u;
  return 1;
}

// src/atp/ac.c:182.  An AC-labelled constant has no leaves: the
// recursion chained leaves[-1] there; it is left as-is, as the explicit
// stack leaves it.
static Term ref_atp_ac_canon_rec(Term t, const AtpAcInfo *ac) {
  if (term_tag(t) != TAG_CTR) return t;
  u32 label = term_ext(t);
  if (atp_ac_is_ac_label(ac, label)) {
    Term leaves[ATP_AC_FLATTEN_CAP];
    u32  n = 0u;
    if (!ref_atp_ac_flatten_under(t, label, leaves, &n, ATP_AC_FLATTEN_CAP)) {
      return t;        // overflow: leave node as-is
    }
    if (n == 0u) {
      return t;
    }
    for (u32 i = 0; i < n; i++) {
      leaves[i] = ref_atp_ac_canon_rec(leaves[i], ac);
    }
    atp_ac_sort_leaves(leaves, n);
    return atp_ac_build_chain(label, leaves, n);
  }
  u32 n_kids = term_ctr_n(t);
  if (n_kids == 0u) return t;
  if (n_kids > REWRITE_MAX_ARITY) return t;
  Term kids[REWRITE_MAX_ARITY];
  u8 changed = 0u;
  for (u32 i = 0; i < n_kids; i++) {
    Term orig = term_ctr_at(t, i);
    kids[i] = ref_atp_ac_canon_rec(orig, ac);
    if (kids[i] != orig) changed = 1u;
  }
  if (!changed) return t;
  return term_new_ctr(label, kids, n_kids);
}

// src/atp/ft_ac_match.c:90
static u8 ref_ft_ac_flatten_under(const AtpFtCell *t, u32 top_label,
                                  const AtpFtCell **out, u32 *n, u32 cap) {
  if (t == NULL) return 0;
  u8 is_ctr = (u8)((t->sym & WF_VAR_BIT) == 0u);
  if (is_ctr && t->sym == top_label && t->arity > 0u) {
    const AtpFtCell *c = t->next;
    for (u16 i = 0; i < t->arity; i++) {
      if (!ref_ft_ac_flatten_under(c, top_label, out, n, cap)) return 0;
      c = c->end->next;
    }
    return 1;
  }
  if (*n >= cap) return 0;
  out[*n] = t;
  *n += 1u;
  return 1;
}

// src/atp/ft_ac_eq.c:48
static u8 ref_ft_ac_eq(const AtpFtCell *a, const AtpFtCell *b,
                       const AtpAcInfo *ac) {
  if (a == NULL && b == NULL) return 1u;
  if (a == NULL || b == NULL) return 0u;
  u8 a_var = (u8)((a->sym & WF_VAR_BIT) != 0u);
  u8 b_var = (u8)((b->sym & WF_VAR_BIT) != 0u);
  if (a_var && b_var) return (u8)(ft_eq(a, b) ? 1u : 0u);
  if (a_var != b_var) return 0u;
  if (a->sym != b->sym) return 0u;
  if (a->arity != b->arity) return 0u;
  // The recursion recursed on an AC-labelled constant forever (it
  // flattens to itself); the engine compares it as a constant.
  if (ft_ac_eq_is_ac_label(ac, a->sym) && a->arity > 0u) {
    const AtpFtCell *A[ATPFT_AC_FLATTEN_CAP];
    const AtpFtCell *B[ATPFT_AC_FLATTEN_CAP];
    u32 na = 0u, nb = 0u;
    if (!ref_ft_ac_flatten_under(a, a->sym, A, &na, ATPFT_AC_FLATTEN_CAP)) {
      return (u8)(ft_eq(a, b) ? 1u : 0u);
    }
    if (!ref_ft_ac_flatten_under(b, b->sym, B, &nb, ATPFT_AC_FLATTEN_CAP)) {
      return (u8)(ft_eq(a, b) ? 1u : 0u);
    }
    if (na != nb) return 0u;
    u8 used[ATPFT_AC_FLATTEN_CAP] = {0};
    for (u32 i = 0; i < na; i++) {
      u8 matched = 0u;
      for (u32 j = 0; j < nb; j++) {
        if (used[j]) continue;
        if (ref_ft_ac_eq(A[i], B[j], ac)) {
          used[j] = 1u;
          matched = 1u;
          break;
        }
      }
      if (!matched) return 0u;
    }
    return 1u;
  }
  u16 arity = a->arity;
  const AtpFtCell *ca = a->next;
  const AtpFtCell *cb = b->next;
  for (u16 i = 0; i < arity; i++) {
    if (!ref_ft_ac_eq(ca, cb, ac)) return 0u;
    ca = ca->end->next;
    cb = cb->end->next;
  }
  return 1u;
}

// An AC-equal variant of a shallow term: the children of every binary
// node over a label in `mask` swapped at random, and f(f(a, b), c)
// sometimes re-associated to f(a, f(b, c)).
static Term ac_shuffle(u32 *seed, Term t, u64 mask) {
  if (term_tag(t) != TAG_CTR) {
    return t;
  }
  u32  n = term_ctr_n(t);
  u32  lab = term_ext(t);
  Term kids[20];
  for (u32 i = 0; i < n; i++) {
    kids[i] = ac_shuffle(seed, term_ctr_at(t, i), mask);
  }
  if (n == 2u && lab < 64u && ((mask >> lab) & 1ull) != 0ull) {
    u32 r = rng32(seed);
    if (r & 1u) {
      Term x  = kids[0];
      kids[0] = kids[1];
      kids[1] = x;
    }
    if ((r & 2u) && term_tag(kids[0]) == TAG_CTR && term_ext(kids[0]) == lab &&
        term_ctr_n(kids[0]) == 2u) {
      Term bc[2] = { term_ctr_at(kids[0], 1), kids[1] };
      kids[0] = term_ctr_at(kids[0], 0);
      kids[1] = term_new_ctr(lab, bc, 2u);
    }
  }
  return term_new_ctr(lab, kids, n);
}

enum { AC_N = 3000 };
#define AC_TEST_MASK ((1ull << 3) | (1ull << 7))

static void pin_ac(u32 *seed) {
  AtpAcInfo ac;
  ac.ac_mask = AC_TEST_MASK;
  AtpFt a;
  ft_init(&a);

  TEST_BEGIN("atp_deep/pin-ac-flatten");
  {
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < AC_N; i++) {
      Term t = mk_random(seed);
      u32 lab = (i & 1u) ? 3u : 7u;
      if (i % 3u == 0u && term_tag(t) == TAG_CTR) {
        lab = term_ext(t);
      }
      u32 cap = rng32(seed) % 70u;
      Term ro[80], no[80];
      u32  rn = (i & 4u) ? 2u : 0u, nn = rn;
      ro[0] = no[0] = ro[1] = no[1] = 0;
      u8 rr = ref_atp_ac_flatten_under(t, lab, ro, &rn, cap);
      u8 nr = atp_ac_flatten_under(t, lab, no, &nn, cap);
      AtpFtCell *x = ft_from_term(&a, t, 0);
      const AtpFtCell *rf[80], *nf[80];
      u32 rfn = 0u, nfn = 0u;
      u8 rfr = ref_ft_ac_flatten_under(x, lab, rf, &rfn, cap);
      u8 nfr = ft_ac_flatten_under(x, lab, nf, &nfn, cap);
      if (rr == nr && rn == nn && memcmp(ro, no, rn * sizeof(Term)) == 0 &&
          rfr == nfr && rfn == nfn &&
          memcmp(rf, nf, rfn * sizeof(const AtpFtCell *)) == 0) {
        ok++;
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)AC_N);
    const AtpFtCell *one[1];
    u32 z = 0u;
    CHECK_EQ(ft_ac_flatten_under(NULL, 3u, one, &z, 1u),
             ref_ft_ac_flatten_under(NULL, 3u, one, &z, 1u));
  }

  TEST_BEGIN("atp_deep/pin-ac-canon-heap-layout");
  {
    u32 ok = 0, changed = 0;
    for (u32 i = 0; i < AC_N; i++) {
      Term t = (i & 1u) ? mk_random(seed)
                        : ac_shuffle(seed, mk_random(seed), AC_TEST_MASK);
      u64  h0 = HEAP_NEXT;
      Term r  = ref_atp_ac_canon_rec(t, &ac);
      u64  h1 = HEAP_NEXT;
      Term n  = atp_ac_canon_rec(t, &ac);
      u64  h2 = HEAP_NEXT;
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) {
        ok++;
      }
      changed += (n != t);
    }
    CHECK_EQ(ok, (u32)AC_N);
    CHECK(changed > 0u);
  }

  TEST_BEGIN("atp_deep/pin-ft-ac-eq");
  {
    u32 ok = 0, hits = 0;
    for (u32 i = 0; i < AC_N; i++) {
      Term t = mk_random(seed);
      Term u = (i % 3u == 0u) ? ac_shuffle(seed, t, AC_TEST_MASK)
             : (i % 3u == 1u) ? mk_mutant(seed, ac_shuffle(seed, t, AC_TEST_MASK))
                              : mk_random(seed);
      AtpFtCell *x = ft_from_term(&a, t, 0);
      AtpFtCell *y = ft_from_term(&a, u, 0);
      u8 r = ref_ft_ac_eq(x, y, &ac);
      u8 n = ft_ac_eq(x, y, &ac);
      if (r == n) {
        ok++;
      }
      hits += n;
    }
    CHECK_EQ(ok, (u32)AC_N);
    CHECK(hits > 0u);
    CHECK_EQ(ft_ac_eq(NULL, NULL, &ac), ref_ft_ac_eq(NULL, NULL, &ac));
  }

  ft_destroy(&a);
}

// The reference recursions of the AC walks (about 1 KB of C stack a
// level) at 3,000 levels, run on the reference stack from inside the
// deep blocks.
typedef struct {
  AtpAcInfo  *ac;
  AtpFt      *a;
  const Term *vals;
  const Term *svals;
  const Term *pbot;
  Term        c9;
} AcVsRef;

static void ac_canon_eq_vs_ref(void *p) {
  AcVsRef *m  = (AcVsRef *)p;
  Term     mt = mk_deep(3000u, 1u, m->vals[4], m->vals, 1u);
  Term     ms = mk_deep(3000u, 1u, m->svals[4], m->svals, 1u);
  u64      h0 = HEAP_NEXT;
  Term     r  = ref_atp_ac_canon_rec(ms, m->ac);
  u64      h1 = HEAP_NEXT;
  Term     n  = atp_ac_canon_rec(ms, m->ac);
  u64      h2 = HEAP_NEXT;
  CHECK(heap_regions_same(h0, h1, h1, h2));
  CHECK(reloc(r, h0, h1, h1) == n);
  AtpFtCell *mxt = ft_from_term(m->a, mt, 0);
  AtpFtCell *mxs = ft_from_term(m->a, ms, 0);
  CHECK_EQ(ft_ac_eq(mxt, mxs, m->ac), ref_ft_ac_eq(mxt, mxs, m->ac));
}

static void deep_ac(void) {
  AtpAcInfo ac;
  ac.ac_mask = AC_TEST_MASK;
  AtpFt a;
  ft_init(&a);
  // AC leaves f3(c_{20+v}, c9) along a non-AC spine, and the same spine
  // with every leaf's children swapped.
  Term c9 = term_new_ctr(9u, NULL, 0u);
  Term vals[5], svals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term cv = term_new_ctr(20u + v, NULL, 0u);
    Term k1[2] = { cv, c9 }, k2[2] = { c9, cv };
    vals[v]  = term_new_ctr(3u, k1, 2u);
    svals[v] = term_new_ctr(3u, k2, 2u);
  }
  Term t    = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term sw   = mk_deep(DEEP, 1u, svals[4], svals, 1u);
  Term near = mk_deep(DEEP, 1u, vals[3], vals, 1u);

  TEST_BEGIN("atp_deep/deep-ac-canon-eq");
  {
    CHECK(atp_ac_eq(t, sw, &ac));
    CHECK(!atp_ac_eq(t, near, &ac));
    CHECK(term_eq_iter(atp_ac_canon(t, &ac), atp_ac_canon(sw, &ac)));
    AtpFtCell *xt = ft_from_term(&a, t, 0);
    AtpFtCell *xs = ft_from_term(&a, sw, 0);
    AtpFtCell *xn = ft_from_term(&a, near, 0);
    CHECK(ft_ac_eq(xt, xs, &ac));
    CHECK(!ft_ac_eq(xt, xn, &ac));
    // A 3,000-deep spine against the reference recursions, on their stack.
    AcVsRef m = { &ac, &a, vals, svals, NULL, 0 };
    deep_stack_run(REF_STACK_BYTES, ac_canon_eq_vs_ref, &m);
  }

  TEST_BEGIN("atp_deep/deep-ac-chain");
  {
    // A left-nested AC chain f3(f3(... f3(c5, c6) ..., c5), c5).
    Term c5 = term_new_ctr(5u, NULL, 0u);
    Term c6 = term_new_ctr(6u, NULL, 0u);
    Term chain = c6;
    for (u32 k = 0; k < DEEP; k++) {
      Term kids[2] = { chain, c5 };
      chain = term_new_ctr(3u, kids, 2u);
    }
    Term *out = (Term *)malloc((DEEP + 1u) * sizeof(Term));
    u32 n = 0u;
    CHECK(atp_ac_flatten_under(chain, 3u, out, &n, DEEP + 1u));
    CHECK_EQ(n, (u32)DEEP + 1u);
    CHECK(out[0] == c6);
    CHECK(out[DEEP] == c5);
    n = 0u;
    CHECK(!atp_ac_flatten_under(chain, 3u, out, &n, ATP_AC_FLATTEN_CAP));
    CHECK_EQ(n, (u32)ATP_AC_FLATTEN_CAP);
    CHECK(atp_ac_canon(chain, &ac) == chain);     // over the leaf cap
    AtpFtCell *xc = ft_from_term(&a, chain, 0);
    const AtpFtCell **fout =
        (const AtpFtCell **)malloc((DEEP + 1u) * sizeof(const AtpFtCell *));
    n = 0u;
    CHECK(ft_ac_flatten_under(xc, 3u, fout, &n, DEEP + 1u));
    CHECK_EQ(n, (u32)DEEP + 1u);
    CHECK_EQ(fout[0]->sym, 6u);
    n = 0u;
    CHECK(!ft_ac_flatten_under(xc, 3u, fout, &n, ATPFT_AC_FLATTEN_CAP));
    CHECK_EQ(n, (u32)ATPFT_AC_FLATTEN_CAP);
    // Over the cap the comparison falls back to syntactic equality.
    CHECK(ft_ac_eq(xc, ft_clone_span(&a, xc, 0), &ac));
    free(fout);
    free(out);
  }

  ft_destroy(&a);
}

// ---- ac.c / ft_ac_match.c: AC matching -----------------------------------

// src/atp/ac.c:661
static u8 ref_atp_match_ac(Term pattern, Term subject,
                           const AtpAcInfo *ac, RewriteSubst *subst) {
  if (term_tag(pattern) == TAG_FVR) {
    u32 id = term_ext(pattern);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (subst->bindings[id] == 0) {
      subst->bindings[id] = subject;
      return 1;
    }
    return kbo_eq(subst->bindings[id], subject);
  }
  if (term_tag(pattern) != TAG_CTR) {
    return kbo_eq(pattern, subject) ? 1u : 0u;
  }
  if (term_tag(subject) != TAG_CTR) return 0;
  if (term_ext(pattern) != term_ext(subject)) return 0;
  u32 lab = term_ext(pattern);
  if (atp_ac_is_ac_label(ac, lab)) {
    Term P[ATP_AC_FLATTEN_CAP];
    Term S[ATP_AC_FLATTEN_CAP];
    u32  np = 0u, ns = 0u;
    if (!atp_ac_flatten_under(pattern, lab, P, &np, ATP_AC_FLATTEN_CAP))
      return 0;
    if (!atp_ac_flatten_under(subject, lab, S, &ns, ATP_AC_FLATTEN_CAP))
      return 0;
    return atp_match_ac_flat(lab, P, np, S, ns, ac, subst);
  }
  u32 np = term_ctr_n(pattern);
  u32 ns = term_ctr_n(subject);
  if (np != ns) return 0;
  for (u32 i = 0; i < np; i++) {
    if (!ref_atp_match_ac(term_ctr_at(pattern, i),
                          term_ctr_at(subject, i),
                          ac, subst)) {
      return 0;
    }
  }
  return 1;
}

// src/atp/ft_ac_match.c:458
static u8 ref_ft_match_ac(AtpFt *a, const AtpFtCell *pat, const AtpFtCell *subj,
                          const AtpAcInfo *ac, AtpFtSubst *subst) {
  if (pat == NULL || subj == NULL) return 0;
  u32 wm_save = subst->wm;
  if ((pat->sym & WF_VAR_BIT) != 0u) {
    u32 id = pat->sym & ~WF_VAR_BIT;
    if (id >= ATPFT_MAX_VARS) return 0;
    AtpFtCell *prev = subst->bind[id];
    if (prev == NULL) {
      subst->bind[id]              = (AtpFtCell *)subj;
      subst->bound_ids[subst->wm]  = id;
      subst->wm                   += 1u;
      return 1u;
    }
    return ft_eq(prev, subj) ? 1u : 0u;
  }
  if ((subj->sym & WF_VAR_BIT) != 0u) return 0;
  if (pat->sym != subj->sym) return 0;
  if (ft_ac_is_ac_label(ac, pat->sym)) {
    const AtpFtCell *P[ATPFT_AC_FLATTEN_CAP];
    const AtpFtCell *S[ATPFT_AC_FLATTEN_CAP];
    u32 np = 0u, ns = 0u;
    if (!ft_ac_flatten_under(pat,  pat->sym, P, &np, ATPFT_AC_FLATTEN_CAP)) {
      ft_subst_rewind(subst, wm_save);
      return 0;
    }
    if (!ft_ac_flatten_under(subj, subj->sym, S, &ns, ATPFT_AC_FLATTEN_CAP)) {
      ft_subst_rewind(subst, wm_save);
      return 0;
    }
    return ft_match_ac_flat(a, pat->sym, P, np, S, ns, ac, subst);
  }
  if (pat->arity != subj->arity) return 0;
  u16 arity = pat->arity;
  const AtpFtCell *pc = pat->next;
  const AtpFtCell *sc = subj->next;
  for (u16 i = 0; i < arity; i++) {
    if (!ref_ft_match_ac(a, pc, sc, ac, subst)) {
      ft_subst_rewind(subst, wm_save);
      return 0;
    }
    pc = pc->end->next;
    sc = sc->end->next;
  }
  return 1u;
}

// The watermark, the binding order and every binding slot.
static int ft_subst_same(const AtpFtSubst *x, const AtpFtSubst *y) {
  if (x->wm != y->wm) {
    return 0;
  }
  if (memcmp(x->bound_ids, y->bound_ids, x->wm * sizeof x->bound_ids[0]) != 0) {
    return 0;
  }
  return memcmp(x->bind, y->bind, sizeof x->bind) == 0;
}

// A pattern for `subj`: generalized, sometimes over an AC-shuffled or
// mutated copy, sometimes unrelated (variables past the substitution
// range included).
static Term ac_match_pattern(u32 *seed, Term subj, u32 i) {
  switch (i % 5u) {
    case 0:  return generalize(seed, subj);
    case 1:  return generalize(seed, ac_shuffle(seed, subj, AC_TEST_MASK));
    case 2:  return generalize(seed, mk_mutant(seed, subj));
    case 3:  return subj;
    default: return mk_random(seed);
  }
}

static void pin_ac_match(void) {
  u32 seed = 0x2b7e1516u;
  AtpAcInfo ac;
  ac.ac_mask = AC_TEST_MASK;
  AtpFt a;
  ft_init(&a);

  TEST_BEGIN("atp_deep/pin-ac-match");
  {
    u32 ok = 0, hits = 0, misses = 0;
    for (u32 i = 0; i < AC_N; i++) {
      Term subj = (i & 1u) ? mk_random(&seed)
                           : ac_shuffle(&seed, mk_random(&seed), AC_TEST_MASK);
      Term pat  = ac_match_pattern(&seed, subj, i);
      RewriteSubst rs, ns;
      memset(&rs, 0, sizeof rs);
      if (i & 8u) {
        rs.bindings[rng32(&seed) & 3u] = mk_small(&seed);
      }
      ns = rs;
      u8 r = ref_atp_match_ac(pat, subj, &ac, &rs);
      u8 n = atp_match_ac(pat, subj, &ac, &ns);
      if (r == n && memcmp(&rs, &ns, sizeof rs) == 0) {
        ok++;
      }
      hits += n;
      misses += !n;
    }
    CHECK_EQ(ok, (u32)AC_N);
    CHECK(hits > 0u);
    CHECK(misses > 0u);
  }

  TEST_BEGIN("atp_deep/pin-ft-ac-match");
  {
    u32 ok = 0, hits = 0, misses = 0;
    for (u32 i = 0; i < AC_N; i++) {
      Term subj = (i & 1u) ? mk_random(&seed)
                           : ac_shuffle(&seed, mk_random(&seed), AC_TEST_MASK);
      Term pat  = ac_match_pattern(&seed, subj, i);
      AtpFtCell *x = ft_from_term(&a, pat, 0);
      AtpFtCell *y = ft_from_term(&a, subj, 0);
      AtpFtSubst rs, ns;
      memset(&rs, 0, sizeof rs);
      if (i & 8u) {
        u32 id = rng32(&seed) & 3u;
        rs.bind[id] = ft_from_term(&a, mk_small(&seed), 0);
        rs.bound_ids[rs.wm++] = id;
      }
      ns = rs;
      // Both runs start from an empty scratch arena, so AC-chain
      // bindings land at equal addresses only if built in the same order.
      ft_scratch_reset(&a);
      u8 r = ref_ft_match_ac(&a, x, y, &ac, &rs);
      ft_scratch_reset(&a);
      u8 n = ft_match_ac(&a, x, y, &ac, &ns);
      if (r == n && ft_subst_same(&rs, &ns)) {
        ok++;
      }
      hits += n;
      misses += !n;
    }
    ft_scratch_reset(&a);
    CHECK_EQ(ok, (u32)AC_N);
    CHECK(hits > 0u);
    CHECK(misses > 0u);
    AtpFtSubst z;
    memset(&z, 0, sizeof z);
    CHECK_EQ(ft_match_ac(&a, NULL, NULL, &ac, &z), 0u);
  }

  ft_destroy(&a);
}

static void ac_match_vs_ref(void *p) {
  AcVsRef *m    = (AcVsRef *)p;
  Term     pat  = mk_deep(3000u, 1u, *m->pbot, NULL, 1u);
  Term     subj = mk_deep(3000u, 1u, m->vals[4], m->vals, 1u);
  Term     bad  = mk_deep(3000u, 1u, m->c9, m->vals, 1u);
  for (u32 k = 0; k < 2u; k++) {
    Term s = k ? bad : subj;
    RewriteSubst rs, ns;
    memset(&rs, 0, sizeof rs);
    memset(&ns, 0, sizeof ns);
    CHECK_EQ(atp_match_ac(pat, s, m->ac, &ns), ref_atp_match_ac(pat, s, m->ac, &rs));
    CHECK(memcmp(&rs, &ns, sizeof rs) == 0);
    AtpFtCell *xp = ft_from_term(m->a, pat, 0);
    AtpFtCell *xs = ft_from_term(m->a, s, 0);
    AtpFtSubst fsr, fsn;
    memset(&fsr, 0, sizeof fsr);
    memset(&fsn, 0, sizeof fsn);
    ft_scratch_reset(m->a);
    u8 r = ref_ft_match_ac(m->a, xp, xs, m->ac, &fsr);
    ft_scratch_reset(m->a);
    CHECK_EQ(ft_match_ac(m->a, xp, xs, m->ac, &fsn), r);
    CHECK(ft_subst_same(&fsr, &fsn));
  }
  ft_scratch_reset(m->a);
}

static void deep_ac_match(void) {
  AtpAcInfo ac;
  ac.ac_mask = AC_TEST_MASK;
  AtpFt a;
  ft_init(&a);
  // A non-AC spine over AC leaves f3(c_{20+v}, c9); the pattern has x_v
  // at those leaves and, at the bottom, the AC node f3(c9, x6), which
  // only matches the subject's f3(c24, c9) modulo commutativity.
  Term c9 = term_new_ctr(9u, NULL, 0u);
  Term vals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term k1[2] = { term_new_ctr(20u + v, NULL, 0u), c9 };
    vals[v] = term_new_ctr(3u, k1, 2u);
  }
  Term pk[2]  = { c9, term_new_fvr(6u) };
  Term pbot   = term_new_ctr(3u, pk, 2u);

  TEST_BEGIN("atp_deep/deep-ac-match");
  {
    Term pat  = mk_deep(DEEP, 1u, pbot, NULL, 1u);
    Term subj = mk_deep(DEEP, 1u, vals[4], vals, 1u);
    Term bad  = mk_deep(DEEP, 1u, c9, vals, 1u);
    RewriteSubst sb;
    memset(&sb, 0, sizeof sb);
    CHECK_EQ(atp_match_ac(pat, subj, &ac, &sb), 1u);
    for (u32 v = 0; v < 5u; v++) {
      CHECK(sb.bindings[v] == vals[v]);
    }
    CHECK(sb.bindings[6] == term_ctr_at(vals[4], 0));
    memset(&sb, 0, sizeof sb);
    CHECK_EQ(atp_match_ac(pat, bad, &ac, &sb), 0u);
    CHECK(sb.bindings[6] == 0);

    AtpFtCell *xp = ft_from_term(&a, pat, 0);
    AtpFtCell *xs = ft_from_term(&a, subj, 0);
    AtpFtCell *xb = ft_from_term(&a, bad, 0);
    AtpFtSubst fs;
    memset(&fs, 0, sizeof fs);
    CHECK_EQ(ft_match_ac(&a, xp, xs, &ac, &fs), 1u);
    CHECK_EQ(fs.wm, 6u);
    CHECK(fs.bind[6] != NULL && fs.bind[6]->sym == 24u);
    ft_scratch_reset(&a);
    memset(&fs, 0, sizeof fs);
    CHECK_EQ(ft_match_ac(&a, xp, xb, &ac, &fs), 0u);
    CHECK_EQ(fs.wm, 0u);
    u32 left = 0u;
    for (u32 v = 0; v < ATPFT_MAX_VARS; v++) {
      left += (fs.bind[v] != NULL);
    }
    CHECK_EQ(left, 0u);
    ft_scratch_reset(&a);
  }

  TEST_BEGIN("atp_deep/deep-ac-match-vs-ref");
  {
    // 3,000 deep against the reference recursions, on their stack.
    AcVsRef m = { &ac, &a, vals, NULL, &pbot, c9 };
    deep_stack_run(REF_STACK_BYTES, ac_match_vs_ref, &m);
  }

  ft_destroy(&a);
}

// ---- ft_norm.c: the full-depth discrimination-tree descent -----------------

// src/atp/ft_norm.c:1905, the v3 walk as it recursed
static void ref_ftdt_descend3(const FtDtDesc *d, u32 node,
                              const AtpFtCell *sc, u32 nbound, u8 pure) {
  FtPiIndex *ix = d->ix;
  const FtDtNode *nd = &ix->dt_nodes[node];
  const u8 want = d->want;
  const AtpFtCell **bind = d->bind;
  u16 ub[ATPFT_MAX_VARS];
  u32 un = 0u;
  u32 dp = 0u;
  if (g_ftdt_pd) dp = g_ftdt_dp;
  // Entry prune: only the root call arrives unchecked -- recursions and
  // iterative hops below pre-check the child mask before advancing.
  if ((nd->sub_mask & want) == 0u) {
    if (g_ftdt_pd) g_atp_pushn_dt_prune++;
    return;
  }
  for (;;) {
    if (g_ftdt_pd) {
      g_atp_pushn_dt_node++;
      g_atp_pushn_dt_hist[(dp < ATP_DT_HIST_N - 1u) ? dp
                                                    : ATP_DT_HIST_N - 1u]++;
    }
    if (sc == d->sc_end) {            // subject consumed: emit this node's leaves
      if (g_ftdt_pd) g_atp_pushn_dt_emit++;
      ftdt_emit_leaf(ix, node, d->out, want, bind, nbound, pure);
      goto out;
    }
    if (nd->n_edge == 1u) {
      u32 label0 = nd->label0;
      if (label0 < FTDT_VAR0) {
        // Single concrete edge: lockstep hop (as descend2's chain).
        if ((sc->sym & WF_VAR_BIT) != 0u || label0 != sc->sym) goto out;
        if (((nd->n_inline ? nd->kmask8[0] : nd->kid_mask[0]) & want) == 0u) {
          if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          goto out;
        }
        if (g_ftdt_pd) { g_atp_pushn_dt_chain++; dp++; }
        node = nd->kid0;
        nd   = &ix->dt_nodes[node];
        sc   = sc->next;
        continue;
      }
      if (label0 != FTDT_STAR) {
        // Single variable edge V_k: follow iteratively.  Filter order:
        // the one-byte child mask first, then the two O(1) cell
        // fail-fasts (first + last cell of the spans -- equal spans
        // end in equal cells), the full walk last.  Pure conjunction:
        // same edges survive, same candidates in the same order.
        u32 k = label0 - FTDT_VAR0;
        const AtpFtCell *bk = bind[k];
        if (((nd->n_inline ? nd->kmask8[0] : nd->kid_mask[0]) & want) == 0u) {
          if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          goto out;
        }
        if (bk != NULL) {
          if (g_ftdt_pd) {
            g_atp_pushn_dt_fteq++;
            g_atp_pushn_dt_fteq_chain++;
            g_atp_pushn_dt_fteq_cell += ftdt_prof_cells(sc);
          }
          if (bk->sym != sc->sym) {             // first-cell fail-fast
            if (g_ftdt_pd) g_atp_pushn_dt_fteq_symfail++;
            goto out;
          }
          if (bk->end != NULL && sc->end != NULL &&
              bk->end->sym != sc->end->sym) {   // last-cell fail-fast
            if (g_ftdt_pd) g_atp_pushn_dt_fteq_symfail++;
            goto out;
          }
          if (!ft_eq(bk, sc)) goto out;
          if (g_ftdt_pd) g_atp_pushn_dt_fteq_ok++;
        }
        if (bk == NULL) {
          if (g_ftdt_pd) g_atp_pushn_dt_bind1++;
          bind[k] = sc;
          ub[un++] = (u16)k;
          if (k + 1u > nbound) nbound = k + 1u;
        }
        if (g_ftdt_pd) { g_atp_pushn_dt_chain++; dp++; }
        node = nd->kid0;
        nd   = &ix->dt_nodes[node];
        sc   = (sc->end != NULL) ? sc->end->next : d->sc_end;
        continue;
      }
      // Single STAR edge: fall through to the branch path below.
    }
    // Branch node (or single STAR edge).  Var/star region first, then
    // the ascending concrete region.  All per-edge prune tests read the
    // parent-side kid_mask mirror.
    {
      const AtpFtCell *skip = (sc->end != NULL) ? sc->end->next : d->sc_end;
      u32 nv = nd->n_var_edge;
      const u32 *lbl = nd->n_inline ? nd->labels8 : nd->labels;
      const u32 *kds = nd->n_inline ? nd->kids8   : nd->kids;
      const u8  *kmk = nd->n_inline ? nd->kmask8  : nd->kid_mask;
      if (g_ftdt_pd) {
        g_atp_pushn_dt_edge += nd->n_edge;
        g_atp_pushn_dt_branch++;
        g_atp_pushn_dt_vedge += nv;
        u32 fb = (nd->n_edge < ATP_DT_FAN_N) ? nd->n_edge
                                             : ATP_DT_FAN_N - 1u;
        g_atp_pushn_dt_fan_visit[fb]++;
        g_atp_pushn_dt_fan_edge[fb] += nd->n_edge;
        ftdt_visit_bump(ix, node);
      }
      for (u32 e = 0; e < nv; e++) {
        u32 label = lbl[e];
        if (label == FTDT_STAR) {
          if ((kmk[e] & want) == 0u) {
            if (g_ftdt_pd) g_atp_pushn_dt_prune++;
            continue;
          }
          if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
          ref_ftdt_descend3(d, kds[e], skip, nbound, 0u);
          continue;
        }
        u32 k = label - FTDT_VAR0;
        const AtpFtCell *bk = bind[k];
        if ((kmk[e] & want) == 0u) {
          if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          continue;
        }
        if (bk != NULL) {
          if (g_ftdt_pd) {
            g_atp_pushn_dt_fteq++;
            g_atp_pushn_dt_fteq_cell += ftdt_prof_cells(sc);
          }
          if (bk->sym != sc->sym) {             // first-cell fail-fast
            if (g_ftdt_pd) g_atp_pushn_dt_fteq_symfail++;
            continue;
          }
          if (bk->end != NULL && sc->end != NULL &&
              bk->end->sym != sc->end->sym) {   // last-cell fail-fast
            if (g_ftdt_pd) g_atp_pushn_dt_fteq_symfail++;
            continue;
          }
          if (!ft_eq(bk, sc)) continue;
          if (g_ftdt_pd) g_atp_pushn_dt_fteq_ok++;
        }
        if (bk == NULL) {
          if (g_ftdt_pd) { g_atp_pushn_dt_bind1++; g_ftdt_dp = dp + 1u; }
          bind[k] = sc;
          ref_ftdt_descend3(d, kds[e], skip,
                            (k + 1u > nbound) ? k + 1u : nbound, pure);
          bind[k] = NULL;
        } else {
          if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
          ref_ftdt_descend3(d, kds[e], skip, nbound, pure);
        }
      }
      if ((sc->sym & WF_VAR_BIT) == 0u) {
        for (u32 e = nv; e < nd->n_edge; e++) {
          u32 label = lbl[e];
          if (g_ftdt_pd) g_atp_pushn_dt_cprobe++;
          if (label < sc->sym) continue;
          if (label == sc->sym) {
            if ((kmk[e] & want) != 0u) {
              // Last exploration step at this node: iterate instead of
              // recursing (descend2 recursed here and then fell out).
              if (g_ftdt_pd) dp++;
              node = kds[e];
              nd   = &ix->dt_nodes[node];
              sc   = sc->next;
              goto next_node;
            }
            if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          }
          break;
        }
      }
    }
    goto out;
  next_node:;
  }
out:
  while (un > 0u) bind[ub[--un]] = NULL;
}


// The descent's profile-tier counters (live under g_ftdt_pd).
typedef struct {
  u64 c[14];
  u64 hist[ATP_DT_HIST_N];
  u64 fan_visit[ATP_DT_FAN_N];
  u64 fan_edge[ATP_DT_FAN_N];
  u32 dp;
} DtProf;

static void dt_prof_snap(DtProf *p) {
  p->c[0]  = g_atp_pushn_dt_node;
  p->c[1]  = g_atp_pushn_dt_edge;
  p->c[2]  = g_atp_pushn_dt_chain;
  p->c[3]  = g_atp_pushn_dt_branch;
  p->c[4]  = g_atp_pushn_dt_prune;
  p->c[5]  = g_atp_pushn_dt_emit;
  p->c[6]  = g_atp_pushn_dt_bind1;
  p->c[7]  = g_atp_pushn_dt_fteq;
  p->c[8]  = g_atp_pushn_dt_fteq_ok;
  p->c[9]  = g_atp_pushn_dt_fteq_cell;
  p->c[10] = g_atp_pushn_dt_cprobe;
  p->c[11] = g_atp_pushn_dt_fteq_symfail;
  p->c[12] = g_atp_pushn_dt_fteq_chain;
  p->c[13] = g_atp_pushn_dt_vedge;
  memcpy(p->hist, g_atp_pushn_dt_hist, sizeof p->hist);
  memcpy(p->fan_visit, g_atp_pushn_dt_fan_visit, sizeof p->fan_visit);
  memcpy(p->fan_edge, g_atp_pushn_dt_fan_edge, sizeof p->fan_edge);
  p->dp = g_ftdt_dp;
}

// The counters moved alike from a to b and from b to c, and the depth
// hand-off ended alike.
static int dt_prof_same(const DtProf *a, const DtProf *b, const DtProf *c) {
  for (u32 i = 0; i < 14u; i++) {
    if (b->c[i] - a->c[i] != c->c[i] - b->c[i]) {
      return 0;
    }
  }
  for (u32 i = 0; i < ATP_DT_HIST_N; i++) {
    if (b->hist[i] - a->hist[i] != c->hist[i] - b->hist[i]) {
      return 0;
    }
  }
  for (u32 i = 0; i < ATP_DT_FAN_N; i++) {
    if (b->fan_visit[i] - a->fan_visit[i] != c->fan_visit[i] - b->fan_visit[i]) {
      return 0;
    }
    if (b->fan_edge[i] - a->fan_edge[i] != c->fan_edge[i] - b->fan_edge[i]) {
      return 0;
    }
  }
  return b->dp == c->dp;
}

// What one descent left behind: candidates, records, binding snapshots.
typedef struct {
  u32              n, n_rec, n_snap;
  u32             *cand;
  FtDtRec         *rec;
  const AtpFtCell **snap;
} DtRun;

static void dt_run_init(DtRun *r) {
  r->cand = (u32 *)malloc(65536u * sizeof(u32));
  r->rec  = (FtDtRec *)malloc(65536u * sizeof(FtDtRec));
  r->snap = (const AtpFtCell **)malloc(65536u * sizeof(const AtpFtCell *));
}

static void dt_run_free(DtRun *r) {
  free(r->cand);
  free(r->rec);
  free(r->snap);
}

// One descent from the root at subject cell `p` (the reference walk when
// `ref`), as ftdt_candidates opens it; 0 when a binding stayed set.
static int dt_query(FtPiIndex *ix, const AtpFtCell *p, u8 want, u8 pure,
                    u8 ref, DtRun *r) {
  static const AtpFtCell *bind[ATPFT_MAX_VARS];
  ix->dtb_snap_n = 0u;
  ix->dtb_rec_n  = 0u;
  if (g_ftdt_pd) {
    g_ftdt_dp = 0u;
  }
  u32 out = 0u;
  FtDtDesc d = { ix, (p->end != NULL) ? p->end->next : NULL, &out, bind, want };
  if (ref) {
    ref_ftdt_descend3(&d, 0u, p, 0u, pure);
  } else {
    ftdt_descend3(&d, 0u, p, 0u, pure);
  }
  r->n      = out < 65536u ? out : 65536u;
  r->n_rec  = ix->dtb_rec_n < 65536u ? ix->dtb_rec_n : 65536u;
  r->n_snap = ix->dtb_snap_n < 65536u ? ix->dtb_snap_n : 65536u;
  memcpy(r->cand, ix->cand, (size_t)r->n * sizeof(u32));
  if (r->n_rec > 0u) {
    memcpy(r->rec, ix->dtb_rec, (size_t)r->n_rec * sizeof(FtDtRec));
  }
  if (r->n_snap > 0u) {
    memcpy(r->snap, ix->dtb_snap, (size_t)r->n_snap * sizeof(const AtpFtCell *));
  }
  for (u32 k = 0; k < ATPFT_MAX_VARS; k++) {
    if (bind[k] != NULL) {
      return 0;
    }
  }
  return 1;
}

static int dt_runs_same(const DtRun *a, const DtRun *b) {
  if (a->n != b->n || a->n_rec != b->n_rec || a->n_snap != b->n_snap ||
      memcmp(a->cand, b->cand, (size_t)a->n * sizeof(u32)) != 0 ||
      memcmp(a->snap, b->snap,
             (size_t)a->n_snap * sizeof(const AtpFtCell *)) != 0) {
    return 0;
  }
  for (u32 i = 0; i < a->n_rec; i++) {
    const FtDtRec *x = &a->rec[i], *y = &b->rec[i];
    if (x->rule != y->rule || x->snap_ofs != y->snap_ofs ||
        x->idmap_ofs != y->idmap_ofs || x->nvar != y->nvar ||
        x->face != y->face) {
      return 0;
    }
  }
  return 1;
}

enum { DT_PATTERNS = 400, DT_N = 3000 };

static void pin_ftdt(u32 *seed) {
  FtPiIndex ix;
  memset(&ix, 0, sizeof ix);
  ftdt_reset(&ix);
  AtpFt a;
  ft_init(&a);
  // Generalized subterms (some with variables past ATPFT_MAX_VARS: star
  // edges) and small terms, in both orientation classes and faces.
  for (u32 r = 0; r < DT_PATTERNS; r++) {
    Term pt = (r % 4u == 0u) ? mk_small(seed)
                             : generalize(seed, pick_sub(seed, mk_random(seed)));
    ftdt_insert_pattern(&ix, ft_from_term(&a, pt, 0), r, (u8)(r & 1u),
                        (u8)((r >> 1) & 1u));
  }
  DtRun ra, na;
  dt_run_init(&ra);
  dt_run_init(&na);

  TEST_BEGIN("atp_deep/pin-ftdt-descend");
  {
    u32 ok = 0, hits = 0, recs = 0;
    for (u32 i = 0; i < DT_N; i++) {
      Term t = (i & 1u) ? mk_random(seed) : generalize(seed, mk_random(seed));
      AtpFtCell *x = ft_from_term(&a, t, 0);
      const AtpFtCell *p = x;
      if (i % 3u == 0u) {
        // a random subterm cell
        u32 skip = rng32(seed) % 16u;
        while (skip-- > 0u && p != x->end) {
          p = p->next;
        }
      }
      u8 want = (u8)(1u + rng32(seed) % 3u);
      u8 pure = (u8)((i >> 2) & 1u);
      g_ftdt_pd = (u8)(i % 4u == 0u);
      DtProf p0, p1, p2;
      dt_prof_snap(&p0);
      int rb = dt_query(&ix, p, want, pure, 1u, &ra);
      dt_prof_snap(&p1);
      int nb = dt_query(&ix, p, want, pure, 0u, &na);
      dt_prof_snap(&p2);
      if (rb && nb && dt_runs_same(&ra, &na) && dt_prof_same(&p0, &p1, &p2)) {
        ok++;
      }
      hits += (na.n > 0u);
      recs += na.n_rec;
    }
    g_ftdt_pd = 0u;
    CHECK_EQ(ok, (u32)DT_N);
    CHECK(hits > 0u);
    CHECK(recs > 0u);
  }

  dt_run_free(&ra);
  dt_run_free(&na);
  ft_destroy(&a);
}

static void deep_ftdt(void) {
  AtpFt a;
  ft_init(&a);
  DtRun ra, na;
  dt_run_init(&ra);
  dt_run_init(&na);
  Term c5 = term_new_ctr(5u, NULL, 0u);
  Term c6 = term_new_ctr(6u, NULL, 0u);
  // g(Y, g(Y, ... g(Y, c5))) with Y = x70 (past ATPFT_MAX_VARS): every Y
  // is a star edge, so the walk branches at each level; and the same
  // shape over x0, a single-variable chain with repeat consistency tests.
  Term qs = c5, qv = c5, s = c5, mqs = c5, ms = c5;
  for (u32 k = 0; k < DEEP; k++) {
    Term k1[2] = { term_new_fvr(70u), qs };
    Term k2[2] = { term_new_fvr(0u), qv };
    Term k3[2] = { c6, s };
    qs = term_new_ctr(3u, k1, 2u);
    qv = term_new_ctr(3u, k2, 2u);
    s  = term_new_ctr(3u, k3, 2u);
    if (k < 2000u) {
      Term k4[2] = { term_new_fvr(70u), mqs };
      Term k5[2] = { c6, ms };
      mqs = term_new_ctr(3u, k4, 2u);
      ms  = term_new_ctr(3u, k5, 2u);
    }
  }
  AtpFtCell *ss  = ft_from_term(&a, s, 0);
  AtpFtCell *mss = ft_from_term(&a, ms, 0);

  TEST_BEGIN("atp_deep/deep-ftdt-descend");
  {
    FtPiIndex ix;
    memset(&ix, 0, sizeof ix);
    ftdt_reset(&ix);
    ftdt_insert_pattern(&ix, ft_from_term(&a, qs, 0), 7u, 0u, 0u);
    ftdt_insert_pattern(&ix, ft_from_term(&a, qv, 0), 8u, 1u, 0u);
    CHECK(dt_query(&ix, ss, 3u, 1u, 0u, &na));
    CHECK_EQ(na.n, 2u);
    sort_u32(na.cand, na.n);
    CHECK_EQ(na.cand[0], 7u);
    CHECK_EQ(na.cand[1], 8u);
    CHECK_EQ(na.n_rec, 1u);                 // the star path records nothing
    CHECK_EQ(na.rec[0].rule, 8u);
    CHECK_EQ(na.rec[0].nvar, 1u);
    CHECK(dt_query(&ix, ss, 1u, 1u, 0u, &na));   // oriented class only
    CHECK_EQ(na.n, 1u);
    CHECK_EQ(na.cand[0], 7u);
    // A different leaf deep down: nothing matches.
    Term k6[2] = { c6, c6 };
    Term bad = term_new_ctr(3u, k6, 2u);
    Term sb = bad;
    for (u32 k = 1; k < DEEP; k++) {
      Term kk[2] = { c6, sb };
      sb = term_new_ctr(3u, kk, 2u);
    }
    CHECK(dt_query(&ix, ft_from_term(&a, sb, 0), 3u, 1u, 0u, &na));
    CHECK_EQ(na.n, 0u);
    // A 2,000-deep star spine against the reference.
    FtPiIndex mx;
    memset(&mx, 0, sizeof mx);
    ftdt_reset(&mx);
    ftdt_insert_pattern(&mx, ft_from_term(&a, mqs, 0), 3u, 0u, 1u);
    CHECK(dt_query(&mx, mss, 3u, 1u, 1u, &ra));
    CHECK(dt_query(&mx, mss, 3u, 1u, 0u, &na));
    CHECK(dt_runs_same(&ra, &na));
    CHECK_EQ(na.n, 1u);
  }

  dt_run_free(&ra);
  dt_run_free(&na);
  ft_destroy(&a);
}

// ---- src/kbo/_.c: the weight-memo eviction paths ---------------------------

// src/kbo/_.c:390 / :455 / :560, the memo fill as it recursed (an evicted
// child's walk nested inside its parent's combine).
static KboMemoFrame g_ref_kbo_memo_stack[KBO_MEMO_STACK_CAP];
static u8 g_ref_kbo_memo_active = 0u;
static KboWMemoEnt *ref_kbo_subtree_memo(const KboConfig *cfg, Term t);

static u64 ref_kbo_leaf_hash(Term t) {
  u32 tg = term_tag(t);
  if (tg == TAG_FVR) {
    return kbo_mix64(0xfacefacefaceull ^ ((u64)term_ext(t) * 0x100000001b3ull));
  }
  if (tg == TAG_CTR) {
    // A CTR child's hash MUST come from g_kbo_thash (filled by an
    // earlier post-order combine step).  Caller invariant: every
    // CTR child has been visited before its parent's combine fires.
    KboTHashEnt *ce = &g_kbo_thash[kbo_thash_slot(t)];
    if (ce->epoch == g_kbo_epoch && ce->t == t) return ce->hash;
    // Slot evicted under collision; fall back to inline recurse.
    // (Rare -- only on a same-walk thash collision.)
    u32 lab = term_ext(t);
    u64 base; u32 n = kbo_ctr_children(t, &base);
    u64 h = kbo_node_hash_init(lab);
    for (u32 i = 0; i < n; i++) {
      h = kbo_node_hash_add(h, i, ref_kbo_leaf_hash(heap_read(base + i)));
    }
    return kbo_node_hash_fini(h, n);
  }
  return kbo_mix64(0xdeadbeefcafebabeull ^ ((u64)tg * 0x100000001b3ull));
}

static KboWMemoEnt *ref_kbo_memo_combine(const KboConfig *cfg, Term t) {
  // Compute this node's structural hash from children (children's
  // hashes are in g_kbo_thash since post-order put them there first).
  // Position-sensitive + per-child avalanche (kbo_node_hash) so distinct
  // variable ids in symmetric positions can never cancel into a collision.
  u32 lab = term_ext(t);
  u64 base; u32 n = kbo_ctr_children(t, &base);
  u64 sh = kbo_node_hash_init(lab);
  for (u32 i = 0; i < n; i++) {
    sh = kbo_node_hash_add(sh, i, ref_kbo_leaf_hash(heap_read(base + i)));
  }
  sh = kbo_node_hash_fini(sh, n);

  // Register this Term ID in the per-Term-ID cache.
  u32 thidx = kbo_thash_slot(t);
  KboTHashEnt *th = &g_kbo_thash[thidx];
  u32 widx = kbo_wmemo_slot(sh);
  th->t = t; th->hash = sh; th->epoch = g_kbo_epoch; th->wmemo_idx = widx;

  // Wmemo hit?  Reuse precomputed weight + profile.
  KboWMemoEnt *e = &g_kbo_wmemo[widx];
  if (e->epoch == g_kbo_epoch && e->hash == sh) return e;

  // Wmemo miss: compute weight + profile.
  long long w = (long long)kbo_weight(cfg, t);
  u8 vc = 0u;
  u8 vid[KBO_VPROF_CAP];
  i16 vcnt[KBO_VPROF_CAP];
  int ovf = 0;
  for (u32 i = 0; i < n; i++) {
    Term c = heap_read(base + i);
    if (term_tag(c) == TAG_FVR) {
      w += (long long)cfg->var_weight;
      u32 id = term_ext(c);
      if (id < KBO_MAX_VAR && !ovf) {
        if (!kbo_vprof_add(vid, vcnt, &vc, id, +1)) ovf = 1;
      }
    } else if (term_tag(c) == TAG_CTR) {
      // Child must have a g_kbo_thash entry (post-order); from that,
      // read the wmemo slot.  If the slot still holds the child's
      // entry, use it; else recompute via kbo_subtree_memo.
      KboTHashEnt *cth = &g_kbo_thash[kbo_thash_slot(c)];
      KboWMemoEnt *ce;
      if (cth->epoch == g_kbo_epoch && cth->t == c) {
        ce = &g_kbo_wmemo[cth->wmemo_idx];
        if (!(ce->epoch == g_kbo_epoch && ce->hash == cth->hash)) {
          ce = ref_kbo_subtree_memo(cfg, c);
        }
      } else {
        ce = ref_kbo_subtree_memo(cfg, c);
      }
      w += ce->w;
      if (ce->vc == KBO_VPROF_OVF) ovf = 1;
      else if (!ovf) {
        for (u8 k = 0; k < ce->vc; k++) {
          if (!kbo_vprof_add(vid, vcnt, &vc, ce->vid[k], ce->vcnt[k])) {
            ovf = 1; break;
          }
        }
      }
    }
    // a non-CTR/non-FVR child contributes 0 weight, no vars.
  }
  e->hash = sh; e->epoch = g_kbo_epoch; e->w = w;
  if (ovf) {
    e->vc = KBO_VPROF_OVF;
  } else {
    e->vc = vc;
    for (u8 k = 0; k < vc; k++) { e->vid[k] = vid[k]; e->vcnt[k] = vcnt[k]; }
  }
  return e;
}

static KboWMemoEnt *ref_kbo_subtree_memo(const KboConfig *cfg, Term t) {
  // Fast Term-ID path: same Term ID seen this epoch -> O(1) jump to
  // its wmemo slot (verified by struct-hash equality to catch any
  // wmemo slot eviction since).
  u32 thidx = kbo_thash_slot(t);
  KboTHashEnt *th = &g_kbo_thash[thidx];
  if (th->epoch == g_kbo_epoch && th->t == t) {
    KboWMemoEnt *e = &g_kbo_wmemo[th->wmemo_idx];
    if (e->epoch == g_kbo_epoch && e->hash == th->hash) return e;
    // Slot evicted under collision; fall through to re-fill.
  }

  u8 reentrant = g_ref_kbo_memo_active;
  g_ref_kbo_memo_active = 1u;
  KboMemoFrame *heap_stack = NULL;
  KboMemoFrame *stack;
  u32 cap = KBO_MEMO_STACK_CAP;
  if (reentrant) {
    heap_stack = (KboMemoFrame *)malloc((size_t)cap * sizeof *heap_stack);
    stack = heap_stack;
  } else {
    stack = g_ref_kbo_memo_stack;
  }
  u32 sp = 0u;
  KboMemoFrame *f = &stack[sp++];
  f->t = t; f->n = kbo_ctr_children(t, &f->base); f->i = 0u;
  while (sp > 0u) {
    f = &stack[sp - 1u];
    // Advance over children that are leaves or already memoized; push the
    // first CTR child still needing computation.
    int descended = 0;
    while (f->i < f->n) {
      Term c = heap_read(f->base + f->i);
      f->i++;
      if (term_tag(c) != TAG_CTR) continue;          // FVR/atom: combined later
      KboTHashEnt *cth = &g_kbo_thash[kbo_thash_slot(c)];
      if (cth->epoch == g_kbo_epoch && cth->t == c) continue;  // already memoized this walk
      if (sp >= cap) {
        u32 ncap = cap * 2u;
        KboMemoFrame *nh = (KboMemoFrame *)malloc((size_t)ncap * sizeof *nh);
        for (u32 k = 0; k < sp; k++) nh[k] = stack[k];
        if (heap_stack) free(heap_stack);
        heap_stack = nh; stack = nh; cap = ncap;
        f = &stack[sp - 1u];
      }
      KboMemoFrame *nf = &stack[sp++];
      nf->t = c; nf->n = kbo_ctr_children(c, &nf->base); nf->i = 0u;
      descended = 1;
      break;
    }
    if (descended) continue;
    // children exhausted: combine this node, pop.
    ref_kbo_memo_combine(cfg, f->t);
    sp--;
  }
  if (heap_stack) free(heap_stack);
  // Read t's thash entry (filled by the combine above); from it, locate
  // the wmemo slot.  A late wmemo collision may have evicted it -- in
  // that case re-combine.
  th = &g_kbo_thash[thidx];
  KboWMemoEnt *e;
  if (th->epoch == g_kbo_epoch && th->t == t) {
    e = &g_kbo_wmemo[th->wmemo_idx];
    if (!(e->epoch == g_kbo_epoch && e->hash == th->hash)) e = ref_kbo_memo_combine(cfg, t);
  } else {
    e = ref_kbo_memo_combine(cfg, t);
  }
  if (!reentrant) g_ref_kbo_memo_active = 0u;
  return e;
}


// FNV over the memo tables' live entries (current epoch), their stored
// fields only: what a fill wrote, wherever it wrote it.
static u64 kbo_memo_digest(void) {
  u64 h = 0xcbf29ce484222325ull;
  for (u32 i = 0; i < KBO_THASH_SIZE; i++) {
    const KboTHashEnt *e = &g_kbo_thash[i];
    if (e->epoch != g_kbo_epoch) {
      continue;
    }
    h = (h ^ i) * 0x100000001b3ull;
    h = (h ^ (u64)e->t) * 0x100000001b3ull;
    h = (h ^ e->hash) * 0x100000001b3ull;
    h = (h ^ e->wmemo_idx) * 0x100000001b3ull;
  }
  for (u32 i = 0; i < KBO_WMEMO_SIZE; i++) {
    const KboWMemoEnt *e = &g_kbo_wmemo[i];
    if (e->epoch != g_kbo_epoch) {
      continue;
    }
    h = (h ^ i) * 0x100000001b3ull;
    h = (h ^ e->hash) * 0x100000001b3ull;
    h = (h ^ (u64)e->w) * 0x100000001b3ull;
    h = (h ^ e->vc) * 0x100000001b3ull;
    if (e->vc != KBO_VPROF_OVF) {
      for (u8 k = 0; k < e->vc; k++) {
        h = (h ^ e->vid[k]) * 0x100000001b3ull;
        h = (h ^ (u64)(u16)e->vcnt[k]) * 0x100000001b3ull;
      }
    }
  }
  return h;
}

// The CTR subterms of a shallow term, preorder.
static u32 ctr_subterms(Term t, Term *out, u32 n, u32 cap) {
  if (term_tag(t) != TAG_CTR || n >= cap) {
    return n;
  }
  out[n++] = t;
  u32 k = term_ctr_n(t);
  for (u32 i = 0; i < k; i++) {
    n = ctr_subterms(term_ctr_at(t, i), out, n, cap);
  }
  return n;
}

// Evict a live entry of `u`: its Term-ID slot (thash) or, behind a live
// one, the weight slot it points at (wmemo).
static void kbo_evict(Term u, u8 wmemo) {
  KboTHashEnt *th = &g_kbo_thash[kbo_thash_slot(u)];
  if (th->epoch != g_kbo_epoch || th->t != u) {
    return;
  }
  if (wmemo) {
    g_kbo_wmemo[th->wmemo_idx].hash ^= 1ull;
  } else {
    th->t = 0;
  }
}

static KboConfig g_km_cfg;
static u32       g_km_w[32];
static u32       g_km_p[32];

static void km_cfg_init(void) {
  for (u32 i = 0; i < 32u; i++) {
    g_km_w[i] = 1u + (i % 3u);
    g_km_p[i] = i;
  }
  g_km_cfg.weights    = g_km_w;
  g_km_cfg.precedence = g_km_p;
  g_km_cfg.n_labels   = 32u;
  g_km_cfg.var_weight = 1u;
}

enum { KM_N = 800, KM_SUBS = 4096 };

// A fresh epoch, unrelated fills, partial fills of `t` and evictions of
// its subterms -- all decided by `seed`, so a replay rebuilds the same
// live table contents.
static void kbo_memo_setup(u32 seed, Term t, const Term *pre, u32 npre,
                           const Term *subs, u32 nsub) {
  thvm_kbo_invalidate();
  for (u32 j = 0; j < npre; j++) {
    (void)kbo_subtree_memo(&g_km_cfg, pre[j]);
  }
  for (u32 j = 0; j < nsub; j++) {
    if ((rng32(&seed) & 3u) == 0u) {
      (void)kbo_subtree_memo(&g_km_cfg, subs[j]);
    }
  }
  (void)t;
  for (u32 j = 0; j < nsub; j++) {
    u32 r = rng32(&seed);
    if ((r & 3u) == 0u) {
      kbo_evict(subs[j], (u8)((r >> 2) & 1u));
    }
  }
}

static void pin_kbo_memo(u32 *seed) {
  km_cfg_init();
  Term *subs = (Term *)malloc(KM_SUBS * sizeof(Term));
  Term  pre[8];

  TEST_BEGIN("atp_deep/pin-kbo-leaf-hash");
  {
    u32 ok = 0;
    for (u32 i = 0; i < KM_N; i++) {
      Term t = mk_random(seed);
      if (term_tag(t) != TAG_CTR) {
        t = term_new_ctr(4u, &t, 1u);
      }
      u32 ns = ctr_subterms(t, subs, 0u, KM_SUBS);
      kbo_memo_setup(rng32(seed), t, pre, 0u, subs, ns);
      if (ref_kbo_leaf_hash(t) == kbo_leaf_hash(t)) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)KM_N);
  }

  TEST_BEGIN("atp_deep/pin-kbo-subtree-memo");
  {
    u32 ok = 0;
    for (u32 i = 0; i < KM_N; i++) {
      Term t = mk_random(seed);
      if (term_tag(t) != TAG_CTR) {
        t = term_new_ctr(4u, &t, 1u);
      }
      for (u32 j = 0; j < 8u; j++) {
        pre[j] = mk_small(seed);
        if (term_tag(pre[j]) != TAG_CTR) {
          pre[j] = term_new_ctr(4u, &pre[j], 1u);
        }
      }
      u32 ns = ctr_subterms(t, subs, 0u, KM_SUBS);
      u32 sd = rng32(seed);
      kbo_memo_setup(sd, t, pre, 8u, subs, ns);
      KboWMemoEnt *re = ref_kbo_subtree_memo(&g_km_cfg, t);
      u64 rd = kbo_memo_digest();
      long long rw = re->w;
      u32 ri = (u32)(re - g_kbo_wmemo);
      kbo_memo_setup(sd, t, pre, 8u, subs, ns);
      KboWMemoEnt *ne = kbo_subtree_memo(&g_km_cfg, t);
      u64 nd = kbo_memo_digest();
      if (rd == nd && rw == ne->w && ri == (u32)(ne - g_kbo_wmemo)) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)KM_N);
  }

  thvm_kbo_invalidate();
  free(subs);
}

static void deep_kbo_memo(void) {
  km_cfg_init();
  Term c5 = term_new_ctr(5u, NULL, 0u);
  Term x0 = term_new_fvr(0u);
  // s^DEEP(f(x0, c5)): weight 2 per s (label 2), 3 + 1 + 3 below.
  Term kids[2] = { x0, c5 };
  Term base = term_new_ctr(6u, kids, 2u);
  Term chain = mk_chain(DEEP, 2u, base);
  long long want = 3LL * DEEP + (1LL + 1LL + 3LL);

  TEST_BEGIN("atp_deep/deep-kbo-memo");
  {
    thvm_kbo_invalidate();
    KboWMemoEnt *e = kbo_subtree_memo(&g_km_cfg, chain);
    CHECK_EQ(e->w, want);
    CHECK_EQ(e->vc, 1u);
    CHECK_EQ(e->vid[0], 0u);
    CHECK_EQ(e->vcnt[0], 1);
    u64 sh = g_kbo_thash[kbo_thash_slot(chain)].hash;
    // Every Term-ID slot evicted: the leaf hash folds the whole spine.
    thvm_kbo_invalidate();
    CHECK_EQ(kbo_leaf_hash(chain), sh);
    // Refill, then evict the weight slot behind every live Term-ID slot
    // of the spine: a parent's combine must recompute its child by a
    // nested walk, which finds the same at the next level -- nesting as
    // deep as the spine.
    thvm_kbo_invalidate();
    (void)kbo_subtree_memo(&g_km_cfg, chain);
    for (Term u = chain; term_tag(u) == TAG_CTR && term_ext(u) == 2u;
         u = term_ctr_at(u, 0)) {
      kbo_evict(u, 1u);
    }
    Term top = term_new_ctr(2u, &chain, 1u);
    e = kbo_subtree_memo(&g_km_cfg, top);
    CHECK_EQ(e->w, want + 3LL);
    CHECK_EQ(thvm_kbo_term_weight(&g_km_cfg, top), want + 3LL);
    // A 200-deep spine, the same eviction, against the reference.
    Term mid = mk_chain(200u, 2u, base);
    Term mtop = term_new_ctr(2u, &mid, 1u);
    u64 rd = 0u, nd = 0u;
    long long rw = 0, nw = 0;
    for (u32 pass = 0; pass < 2u; pass++) {
      thvm_kbo_invalidate();
      (void)kbo_subtree_memo(&g_km_cfg, mid);
      for (Term u = mid; term_tag(u) == TAG_CTR && term_ext(u) == 2u;
           u = term_ctr_at(u, 0)) {
        kbo_evict(u, 1u);
      }
      if (pass == 0u) {
        rw = ref_kbo_subtree_memo(&g_km_cfg, mtop)->w;
        rd = kbo_memo_digest();
      } else {
        nw = kbo_subtree_memo(&g_km_cfg, mtop)->w;
        nd = kbo_memo_digest();
      }
    }
    CHECK_EQ(rw, nw);
    CHECK_EQ(rd, nd);
    thvm_kbo_invalidate();
  }
}

// ---- src/lpo/_.c: the Term, flat and DAG evaluators and their walks --------

static LpoCmp ref_lpo_rec_compute(Term s, Term t, const LpoConfig *cfg);
static LpoCmp ref_lpo_flat_rec(const KboFlatNode *a, u32 pa,
                               const KboFlatNode *b, u32 pb,
                               const LpoConfig *cfg);
static LpoCmp ref_lpo_dag_compute(u32 c1, u32 c2);

// src/lpo/_.c lpo_eq
static u8 ref_lpo_eq(Term s, Term t) {
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  switch (term_tag(s)) {
    case TAG_FVR: return 1;  // same id (ext) already checked
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!ref_lpo_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

// src/lpo/_.c lpo_var_occurs_in
static u8 ref_lpo_var_occurs_in(Term t, u32 var_id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == var_id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (ref_lpo_var_occurs_in(term_ctr_at(t, i), var_id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// src/lpo/_.c lpo_var_set_acc
static void ref_lpo_var_set_acc(Term t, u32 *seen) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < LPO_MAX_VAR) seen[id] = 1;
      return;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) ref_lpo_var_set_acc(term_ctr_at(t, i), seen);
      return;
    }
    default: return;
  }
}

// src/lpo/_.c lpo_vars_in_range
static u8 ref_lpo_vars_in_range(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) < LPO_MAX_VAR;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (!ref_lpo_vars_in_range(term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

// src/lpo/_.c lpo_rec
static LpoCmp ref_lpo_rec(Term s, Term t, const LpoConfig *cfg) {
  if (s == t) return LPO_EQ;            // pointer-identical, no memo
  if (lpo_deadline_fired()) return LPO_UN;   // wall passed: unwind fast
  u32 idx = lpo_memo_hash(s, t);
  LpoMemoEnt *e = &g_lpo_memo[idx];
  if (e->epoch == g_lpo_epoch && e->s == s && e->t == t)
    return (LpoCmp)(i8)e->cmp;
  LpoCmp c = ref_lpo_rec_compute(s, t, cfg);
  e->s = s; e->t = t; e->epoch = g_lpo_epoch; e->cmp = (u8)c;
  return c;
}

// src/lpo/_.c lpo_dominates_all_args
static u8 ref_lpo_dominates_all_args(Term s, Term t_parent,
                                     const LpoConfig *cfg) {
  if (term_tag(t_parent) != TAG_CTR) return 1;
  u32 n = term_ctr_n(t_parent);
  for (u32 j = 0; j < n; j++) {
    if (ref_lpo_rec(s, term_ctr_at(t_parent, j), cfg) != LPO_GT) return 0;
  }
  return 1;
}

// src/lpo/_.c lpo_some_arg_dominates
static u8 ref_lpo_some_arg_dominates(Term s_parent, Term t,
                                     const LpoConfig *cfg) {
  if (term_tag(s_parent) != TAG_CTR) return 0;
  u32 m = term_ctr_n(s_parent);
  for (u32 i = 0; i < m; i++) {
    Term s_i = term_ctr_at(s_parent, i);
    LpoCmp c = ref_lpo_rec(s_i, t, cfg);
    if (c == LPO_EQ || c == LPO_GT) return 1;
  }
  return 0;
}

// src/lpo/_.c lpo_lex
static LpoCmp ref_lpo_lex(Term s_parent, Term t_parent,
                          const LpoConfig *cfg) {
  u32 n = term_ctr_n(s_parent);
  for (u32 i = 0; i < n; i++) {
    LpoCmp c = ref_lpo_rec(term_ctr_at(s_parent, i),
                           term_ctr_at(t_parent, i), cfg);
    if (c != LPO_EQ) return c;
  }
  return LPO_EQ;
}

// src/lpo/_.c lpo_rec_compute
static LpoCmp ref_lpo_rec_compute(Term s, Term t, const LpoConfig *cfg) {
  if (ref_lpo_eq(s, t)) return LPO_EQ;

  u8 s_is_fvr = (term_tag(s) == TAG_FVR);
  u8 t_is_fvr = (term_tag(t) == TAG_FVR);

  // Variable cases.
  if (s_is_fvr && t_is_fvr) {
    // Distinct FVR ids: incomparable.
    return LPO_UN;
  }
  if (s_is_fvr) {
    // s is FVR, t is CTR.  s >_lpo t is false; check t >_lpo s
    // via subterm-occurrence: t > x iff x occurs in t.
    if (ref_lpo_var_occurs_in(t, term_ext(s))) return LPO_LT;
    return LPO_UN;
  }
  if (t_is_fvr) {
    // s is CTR, t is FVR.  s >_lpo x iff x occurs strictly in s.
    if (ref_lpo_var_occurs_in(s, term_ext(t))) return LPO_GT;
    return LPO_UN;
  }

  // Both are CTRs (or non-CTR/non-FVR which we treat as UN).
  if (term_tag(s) != TAG_CTR || term_tag(t) != TAG_CTR) return LPO_UN;

  // Case (1) check both directions: subterm domination.
  if (ref_lpo_some_arg_dominates(s, t, cfg)) return LPO_GT;
  if (ref_lpo_some_arg_dominates(t, s, cfg)) return LPO_LT;

  // Cases (2) and (3): compare top symbols.
  u32 lab_s = term_ext(s);
  u32 lab_t = term_ext(t);
  u32 prec_s = (lab_s < cfg->n_labels) ? cfg->precedence[lab_s] : 0;
  u32 prec_t = (lab_t < cfg->n_labels) ? cfg->precedence[lab_t] : 0;

  if (prec_s > prec_t) {
    // f >_F g: case (2), s >_lpo t iff s >_lpo every t_j.
    if (ref_lpo_dominates_all_args(s, t, cfg)) return LPO_GT;
    return LPO_UN;
  }
  if (prec_s < prec_t) {
    // g >_F f: symmetric.
    if (ref_lpo_dominates_all_args(t, s, cfg)) return LPO_LT;
    return LPO_UN;
  }

  // Equal precedence: distinct symbols are incomparable heads, which
  // case (1) above did not order.  Equal head symbols: case (3).  Lex
  // compare args; the winning direction must dominate the loser's args.
  if (lab_s != lab_t || term_ctr_n(s) != term_ctr_n(t)) {
    return LPO_UN;
  }
  LpoCmp lex = ref_lpo_lex(s, t, cfg);
  if (lex == LPO_EQ) {
    return LPO_EQ;   // can't happen if lpo_eq fired earlier
  }
  if (lex == LPO_GT) {
    return ref_lpo_dominates_all_args(s, t, cfg) ? LPO_GT : LPO_UN;
  }
  if (lex == LPO_LT) {
    return ref_lpo_dominates_all_args(t, s, cfg) ? LPO_LT : LPO_UN;
  }
  // An incomparable argument pair decides neither lex direction.
  return LPO_UN;
}

// src/lpo/_.c lpo_flat_encode_rec
static u8 ref_lpo_flat_encode_rec(Term t, const LpoConfig *cfg,
                                  KboFlatNode *out, u32 *pos) {
  u32 here = *pos;
  if (__builtin_expect(here >= KBO_FLAT_CAP, 0)) return 0;
  *pos = here + 1u;
  u32 tag = term_tag(t);
  if (tag == TAG_FVR) {
    out[here].sym = -(i32)(term_ext(t) + 1u);
    out[here].w   = 0;
    out[here].sz  = 1u;
    return 1;
  }
  if (__builtin_expect(tag != TAG_CTR, 0)) return 0;
  u32 lab = term_ext(t);
  out[here].sym = (i32)lab;
  out[here].w   = (lab < cfg->n_labels) ? (i32)cfg->precedence[lab] : 0;
  // Cache the children base loc once -- term_ctr_at would re-read the
  // arity cell via term_ctr_n per call.
  u64 base = term_val(t);
  Term n_cell = heap_read(base);
  if (__builtin_expect(term_tag(n_cell) != TAG_NUM, 0)) {
    out[here].sz = 1u; return 1;
  }
  u32 n = (u32)term_val(n_cell);
  for (u32 i = 0; i < n; i++) {
    Term child = heap_read(base + 1u + (u64)i);
    if (!ref_lpo_flat_encode_rec(child, cfg, out, pos)) return 0;
  }
  out[here].sz = *pos - here;
  return 1;
}

// src/lpo/_.c lpo_flat_dominates_all_args
static u8 ref_lpo_flat_dominates_all_args(const KboFlatNode *a, u32 pa,
                                          const KboFlatNode *b, u32 pb,
                                          const LpoConfig *cfg) {
  // b[pb] must be a CTR; iterate its children via Ende skip.
  if (b[pb].sym < 0) return 1;
  u32 child = pb + 1u;
  u32 end   = pb + b[pb].sz;
  while (child < end) {
    if (ref_lpo_flat_rec(a, pa, b, child, cfg) != LPO_GT) return 0;
    child += b[child].sz;
  }
  return 1;
}

// src/lpo/_.c lpo_flat_some_arg_dominates
static u8 ref_lpo_flat_some_arg_dominates(const KboFlatNode *a, u32 pa,
                                          const KboFlatNode *b, u32 pb,
                                          const LpoConfig *cfg) {
  if (a[pa].sym < 0) return 0;
  u32 child = pa + 1u;
  u32 end   = pa + a[pa].sz;
  while (child < end) {
    LpoCmp c = ref_lpo_flat_rec(a, child, b, pb, cfg);
    if (c == LPO_EQ || c == LPO_GT) return 1;
    child += a[child].sz;
  }
  return 0;
}

// src/lpo/_.c lpo_flat_lex_diff
static LpoCmp ref_lpo_flat_lex_diff(const KboFlatNode *a, u32 pa,
                                    const KboFlatNode *b, u32 pb,
                                    const LpoConfig *cfg, u32 *diff_a, u32 *diff_b) {
  u32 ca = pa + 1u, cb = pb + 1u;
  u32 ea = pa + a[pa].sz, eb = pb + b[pb].sz;
  // Fast path for the common arity-2 case (e.g. Sheffer's nand).
  if (ca < ea && cb < eb) {
    LpoCmp c = ref_lpo_flat_rec(a, ca, b, cb, cfg);
    if (c != LPO_EQ) { *diff_a = ca; *diff_b = cb; return c; }
    ca += a[ca].sz;
    cb += b[cb].sz;
  }
  if (ca < ea && cb < eb) {
    LpoCmp c = ref_lpo_flat_rec(a, ca, b, cb, cfg);
    if (c != LPO_EQ) { *diff_a = ca; *diff_b = cb; return c; }
    ca += a[ca].sz;
    cb += b[cb].sz;
  }
  while (ca < ea && cb < eb) {
    LpoCmp c = ref_lpo_flat_rec(a, ca, b, cb, cfg);
    if (c != LPO_EQ) { *diff_a = ca; *diff_b = cb; return c; }
    ca += a[ca].sz;
    cb += b[cb].sz;
  }
  return LPO_EQ;
}

// src/lpo/_.c lpo_flat_dominates_from
static u8 ref_lpo_flat_dominates_from(const KboFlatNode *a, u32 pa,
                                      const KboFlatNode *b, u32 pb,
                                      u32 start_child,
                                      const LpoConfig *cfg) {
  u32 child = start_child;
  u32 end   = pb + b[pb].sz;
  while (child < end) {
    if (ref_lpo_flat_rec(a, pa, b, child, cfg) != LPO_GT) return 0;
    child += b[child].sz;
  }
  return 1;
}

// src/lpo/_.c lpo_flat_rec_compute
static LpoCmp ref_lpo_flat_rec_compute(const KboFlatNode *a, u32 pa,
                                       const KboFlatNode *b, u32 pb,
                                       const LpoConfig *cfg) {
  if (lpo_flat_slice_eq(a, pa, b, pb)) return LPO_EQ;
  u8 a_is_var = (a[pa].sym < 0);
  u8 b_is_var = (b[pb].sym < 0);
  if (__builtin_expect(a_is_var | b_is_var, 0)) {
    if (a_is_var && b_is_var) return LPO_UN;
    if (a_is_var) {
      if (lpo_flat_var_occurs(b, pb, a[pa].sym)) return LPO_LT;
      return LPO_UN;
    }
    if (lpo_flat_var_occurs(a, pa, b[pb].sym)) return LPO_GT;
    return LPO_UN;
  }
  // Per-level subterm Vortest: pretest at each recursion level catches
  // the strict-subterm cases without paying the cost of the some_arg_dom
  // recursion below.  Mirrors WM's LP_LPOGroesser structure where each
  // recursive call enters via LV_VortestLPOGroesser before the
  // some-arg-dominates dispatch.
  {
    int r1 = lpo_pretest_groesser_flat(a + pa, a[pa].sz,
                                       b + pb, b[pb].sz, cfg);
    if (r1 > 0) return LPO_GT;
    int r2 = lpo_pretest_groesser_flat(b + pb, b[pb].sz,
                                       a + pa, a[pa].sz, cfg);
    if (r2 > 0) return LPO_LT;
  }
  // Precedence dispatch first (WM's structure): when precs differ, case
  // (2) dominates_all_args resolves directly; case (1) only fires for
  // the OTHER direction (children of the smaller-prec side might still
  // dominate via deeper subterm).  When precs are equal, lex + case (3)
  // applies and case (1) some_arg_dom is checked symmetrically.
  i32 prec_a = a[pa].w;
  i32 prec_b = b[pb].w;
  // For most signatures (e.g. Sheffer = single nand internal) precedences
  // are equal on the dominant recursive path; mark unequal-prec as cold.
  if (__builtin_expect(prec_a > prec_b, 0)) {
    if (ref_lpo_flat_dominates_all_args(a, pa, b, pb, cfg)) return LPO_GT;
    // case (1) symmetric: some child of b might still dominate a (rare).
    if (ref_lpo_flat_some_arg_dominates(b, pb, a, pa, cfg)) return LPO_LT;
    return LPO_UN;
  }
  if (__builtin_expect(prec_a < prec_b, 0)) {
    if (ref_lpo_flat_dominates_all_args(b, pb, a, pa, cfg)) return LPO_LT;
    if (ref_lpo_flat_some_arg_dominates(a, pa, b, pb, cfg)) return LPO_GT;
    return LPO_UN;
  }
  // Equal precedence: case (1) both directions can fire, then case (3)
  // lex.
  if (ref_lpo_flat_some_arg_dominates(a, pa, b, pb, cfg)) {
    return LPO_GT;
  }
  if (ref_lpo_flat_some_arg_dominates(b, pb, a, pa, cfg)) {
    return LPO_LT;
  }
  if (a[pa].sym != b[pb].sym) {
    return LPO_UN;
  }
  // Same head symbol implies same arity (signature constraint), but
  // subtree sizes (`sz`) may differ when children have different shapes
  // -- so DO NOT short-circuit on sz mismatch.  lex compare handles it.
  u32 diff_a = 0, diff_b = 0;
  LpoCmp lex = ref_lpo_flat_lex_diff(a, pa, b, pb, cfg, &diff_a, &diff_b);
  if (lex == LPO_EQ) {
    return LPO_EQ;
  }
  // After lex finds the differing position, earlier children are
  // pairwise EQ -- so dominates_all_args trivially holds for them via
  // case (1) at that child position.  Skip to the differing position
  // (WM's LPOGroesserAlleDiff structure, LPO.c:261/287).
  if (lex == LPO_GT) {
    return ref_lpo_flat_dominates_from(a, pa, b, pb, diff_b, cfg) ? LPO_GT : LPO_UN;
  }
  if (lex == LPO_LT) {
    return ref_lpo_flat_dominates_from(b, pb, a, pa, diff_a, cfg) ? LPO_LT : LPO_UN;
  }
  // An incomparable argument pair decides neither lex direction, and
  // case (1) failed both ways above (LPO.c:300-306).
  return LPO_UN;
}

// src/lpo/_.c lpo_flat_rec
static LpoCmp ref_lpo_flat_rec(const KboFlatNode *a, u32 pa,
                               const KboFlatNode *b, u32 pb,
                               const LpoConfig *cfg) {
  g_lpo_flat_rec_calls++;
  if (a == b && pa == pb) return LPO_EQ;
  if (lpo_deadline_fired()) return LPO_UN;   // wall passed: unwind fast
  u8 side = (a == g_lpo_flat_a) ? 0u : 1u;
  u32 idx = lpo_flat_hash(pa, pb, side);
  LpoFlatMemoEnt *e = &g_lpo_flat_memo[idx];
  if (e->epoch == g_lpo_flat_epoch && e->pa == pa && e->pb == pb && e->side == side) {
    g_lpo_flat_memo_hits++;
    return (LpoCmp)(i8)e->cmp;
  }
  g_lpo_flat_compute_calls++;
  LpoCmp c = ref_lpo_flat_rec_compute(a, pa, b, pb, cfg);
  e->pa = pa; e->pb = pb; e->epoch = g_lpo_flat_epoch; e->cmp = (u8)c; e->side = side;
  return c;
}

// src/lpo/_.c lpo_dag_rec
static LpoCmp ref_lpo_dag_rec(u32 c1, u32 c2) {
  g_lpo_flat_rec_calls++;
  if (c1 == c2) {
    return LPO_EQ;
  }
  if (lpo_deadline_fired()) {
    return LPO_UN;
  }
  u32 s = lpo_dag_memo_slot(c1, c2);
  while (g_ld_memo[s].k1 != 0u) {
    if (g_ld_memo[s].k1 == c1 + 1u && g_ld_memo[s].k2 == c2) {
      g_lpo_flat_memo_hits++;
      return (LpoCmp)(i8)(u8)g_ld_memo[s].v;
    }
    s = (s + 1u) & g_ld_memo_mask;
  }
  g_lpo_flat_compute_calls++;
  LpoCmp r = ref_lpo_dag_compute(c1, c2);
  lpo_dag_memo_put(c1, c2, r);
  return r;
}

// src/lpo/_.c lpo_dag_compute
static LpoCmp ref_lpo_dag_compute(u32 c1, u32 c2) {
  if (c1 == c2) {
    return LPO_EQ;
  }
  const LpoDagNode *x  = &g_ld_node[c1];
  const LpoDagNode *y  = &g_ld_node[c2];
  u8                xv = (u8)(x->sym < 0);
  u8                yv = (u8)(y->sym < 0);
  if (xv | yv) {
    if (xv && yv) {
      return LPO_UN;
    }
    if (xv) {
      return lpo_dag_var_in(c2, x->sym) ? LPO_LT : LPO_UN;
    }
    return lpo_dag_var_in(c1, y->sym) ? LPO_GT : LPO_UN;
  }
  if (lpo_dag_vortest(c1, c2)) {
    return LPO_GT;
  }
  if (lpo_dag_vortest(c2, c1)) {
    return LPO_LT;
  }
  const u32 *kx = &g_ld_kids[x->kids];
  const u32 *ky = &g_ld_kids[y->kids];
  u32        nx = x->arity;
  u32        ny = y->arity;
  if (x->w > y->w) {
    u32 j = 0u;
    while (j < ny && ref_lpo_dag_rec(c1, ky[j]) == LPO_GT) {
      j++;
    }
    if (j == ny) {
      return LPO_GT;
    }
    for (j = 0u; j < ny; j++) {
      LpoCmp c = ref_lpo_dag_rec(ky[j], c1);
      if (c == LPO_EQ || c == LPO_GT) {
        return LPO_LT;
      }
    }
    return LPO_UN;
  }
  if (x->w < y->w) {
    u32 i = 0u;
    while (i < nx && ref_lpo_dag_rec(c2, kx[i]) == LPO_GT) {
      i++;
    }
    if (i == nx) {
      return LPO_LT;
    }
    for (i = 0u; i < nx; i++) {
      LpoCmp c = ref_lpo_dag_rec(kx[i], c2);
      if (c == LPO_EQ || c == LPO_GT) {
        return LPO_GT;
      }
    }
    return LPO_UN;
  }
  for (u32 i = 0; i < nx; i++) {
    LpoCmp c = ref_lpo_dag_rec(kx[i], c2);
    if (c == LPO_EQ || c == LPO_GT) {
      return LPO_GT;
    }
  }
  for (u32 j = 0; j < ny; j++) {
    LpoCmp c = ref_lpo_dag_rec(ky[j], c1);
    if (c == LPO_EQ || c == LPO_GT) {
      return LPO_LT;
    }
  }
  if (x->sym != y->sym) {
    return LPO_UN;
  }
  u32    n   = (nx < ny) ? nx : ny;
  u32    d   = 0u;
  LpoCmp lex = LPO_EQ;
  while (d < n) {
    lex = ref_lpo_dag_rec(kx[d], ky[d]);
    if (lex != LPO_EQ) {
      break;
    }
    d++;
  }
  if (lex == LPO_EQ) {
    return LPO_EQ;
  }
  if (lex == LPO_UN) {
    return LPO_UN;
  }
  if (lex == LPO_GT) {
    for (u32 j = d; j < ny; j++) {
      if (ref_lpo_dag_rec(c1, ky[j]) != LPO_GT) {
        return LPO_UN;
      }
    }
    return LPO_GT;
  }
  for (u32 i = d; i < nx; i++) {
    if (ref_lpo_dag_rec(c2, kx[i]) != LPO_GT) {
      return LPO_UN;
    }
  }
  return LPO_LT;
}

// A deadline that never fires: every evaluator call then polls it, so the
// poll counter counts the calls.
static int lpo_hook_never(void) {
  return 0;
}

// FNV over the Term memo's live entries.
static u64 lpo_memo_digest(void) {
  u64 h = 0xcbf29ce484222325ull;
  for (u32 i = 0; i < LPO_MEMO_SIZE; i++) {
    const LpoMemoEnt *e = &g_lpo_memo[i];
    if (e->epoch != g_lpo_epoch) {
      continue;
    }
    h = (h ^ i) * 0x100000001b3ull;
    h = (h ^ (u64)e->s) * 0x100000001b3ull;
    h = (h ^ (u64)e->t) * 0x100000001b3ull;
    h = (h ^ e->cmp) * 0x100000001b3ull;
  }
  return h;
}

// FNV over the flat memo's live entries at every slot a pair of
// positions below `n` can hash to.
static u64 lpo_flat_memo_digest(u32 n) {
  u64 h = 0xcbf29ce484222325ull;
  for (u32 pa = 0; pa < n; pa++) {
    for (u32 pb = 0; pb < n; pb++) {
      for (u8 side = 0; side < 2u; side++) {
        u32 i = lpo_flat_hash(pa, pb, side);
        const LpoFlatMemoEnt *e = &g_lpo_flat_memo[i];
        if (e->epoch != g_lpo_flat_epoch) {
          continue;
        }
        h = (h ^ i) * 0x100000001b3ull;
        h = (h ^ e->pa) * 0x100000001b3ull;
        h = (h ^ e->pb) * 0x100000001b3ull;
        h = (h ^ e->cmp) * 0x100000001b3ull;
        h = (h ^ e->side) * 0x100000001b3ull;
      }
    }
  }
  return h;
}

// A fresh flat-memo epoch, as lpo_flat_ref opens one.
static void lpo_flat_epoch_bump(void) {
  if (++g_lpo_flat_epoch == 0) {
    for (u32 i = 0; i < LPO_FLAT_MEMO_SIZE; i++) {
      g_lpo_flat_memo[i].epoch = 0;
    }
    g_lpo_flat_epoch = 1;
  }
}

// FNV over the DAG memo (every slot) and its counters.
static u64 lpo_dag_memo_digest(void) {
  u64 h = 0xcbf29ce484222325ull;
  for (u32 i = 0; i <= g_ld_memo_mask; i++) {
    h = (h ^ g_ld_memo[i].k1) * 0x100000001b3ull;
    h = (h ^ g_ld_memo[i].k2) * 0x100000001b3ull;
    h = (h ^ g_ld_memo[i].v) * 0x100000001b3ull;
  }
  h = (h ^ g_ld_memo_cnt) * 0x100000001b3ull;
  h = (h ^ g_ld_memo_full) * 0x100000001b3ull;
  return h;
}

static u32       g_lpo_prec[32];
static LpoConfig g_lpo_cfg;

// Precedences over labels 0..31 with ties (distinct symbols of equal
// precedence are incomparable heads).
static void lpo_cfg_init(void) {
  for (u32 i = 0; i < 32u; i++) {
    g_lpo_prec[i] = i / 2u;
  }
  g_lpo_cfg.precedence = g_lpo_prec;
  g_lpo_cfg.n_labels   = 32u;
}

// A partner for `s`: itself, a copy, a mutant, a subterm, a
// generalization or an unrelated term.
static Term lpo_partner(u32 *seed, Term s) {
  switch (rng32(seed) % 6u) {
    case 0:  return s;
    case 1:  return copy_term(s);
    case 2:  return mk_mutant(seed, s);
    case 3:  return pick_sub(seed, s);
    case 4:  return generalize(seed, s);
    default: return mk_sized(seed, 40u);
  }
}

// A term over a fixed signature (labels 0..3 constants, 4..7 unary,
// 8..11 binary; variables x0..x5), so the DAG walk can hash-cons it.
static Term mk_sig_term(u32 *seed, u32 depth, u32 *budget) {
  u32 r = rng32(seed);
  if (depth == 0u || *budget == 0u || (r & 7u) < 2u) {
    if (*budget > 0u) {
      (*budget)--;
    }
    return (r & 8u) ? term_new_fvr((r >> 4) % 6u)
                    : term_new_ctr((r >> 4) % 4u, NULL, 0u);
  }
  (*budget)--;
  u32  lab = 4u + (r >> 4) % 8u;
  u32  n   = (lab < 8u) ? 1u : 2u;
  Term kids[2];
  for (u32 i = 0; i < n; i++) {
    kids[i] = mk_sig_term(seed, depth - 1u, budget);
  }
  return term_new_ctr(lab, kids, n);
}

static Term mk_sig(u32 *seed) {
  u32 budget = 40u;
  return mk_sig_term(seed, 7u, &budget);
}

// `t` with one random subterm replaced by a fresh signature term.
static Term sig_mutant(u32 *seed, Term t) {
  u32 n = (term_tag(t) == TAG_CTR) ? term_ctr_n(t) : 0u;
  if (n == 0u || (rng32(seed) % 3u) == 0u) {
    return mk_sig(seed);
  }
  Term kids[2];
  for (u32 i = 0; i < n; i++) {
    kids[i] = term_ctr_at(t, i);
  }
  u32 k = rng32(seed) % n;
  kids[k] = sig_mutant(seed, kids[k]);
  return term_new_ctr(term_ext(t), kids, n);
}

// A pair (s, t) for the evaluators: over the fixed signature or over
// random labels, t related to s or not.
static void lpo_pair(u32 *seed, u32 i, Term *s, Term *t) {
  if (i & 2u) {
    *s = mk_sig(seed);
    switch (rng32(seed) % 5u) {
      case 0:  *t = copy_term(*s); break;
      case 1:  *t = sig_mutant(seed, *s); break;
      case 2:  *t = pick_sub(seed, *s); break;
      case 3:  *t = generalize(seed, *s); break;
      default: *t = mk_sig(seed); break;
    }
  } else {
    *s = mk_sized(seed, 40u);
    *t = lpo_partner(seed, *s);
  }
  if (i & 1u) {
    Term x = *s;
    *s = *t;
    *t = x;
  }
}

enum { LPO_N = 2000 };

static void pin_lpo(u32 *seed) {
  lpo_cfg_init();
  thvm_lpo_set_deadline_hook(lpo_hook_never);
  KboFlatNode *ra = (KboFlatNode *)malloc(KBO_FLAT_CAP * sizeof(KboFlatNode));

  TEST_BEGIN("atp_deep/pin-lpo-term-walks");
  {
    u32 ok = 0;
    g_wide = 20u;
    for (u32 i = 0; i < LPO_N; i++) {
      Term t = mk_random(seed);
      Term u = lpo_partner(seed, t);
      u32 id = (rng32(seed) & 1u) ? rng32(seed) % 10u : 60u + rng32(seed) % 8u;
      u32 rs[LPO_MAX_VAR], ns[LPO_MAX_VAR];
      memset(rs, 0, sizeof rs);
      memset(ns, 0, sizeof ns);
      ref_lpo_var_set_acc(t, rs);
      lpo_var_set_acc(t, ns);
      if (ref_lpo_eq(t, u) == lpo_eq(t, u) &&
          ref_lpo_var_occurs_in(t, id) == lpo_var_occurs_in(t, id) &&
          ref_lpo_vars_in_range(t) == lpo_vars_in_range(t) &&
          memcmp(rs, ns, sizeof rs) == 0) {
        ok++;
      }
    }
    g_wide = 16u;
    CHECK_EQ(ok, (u32)LPO_N);
  }

  TEST_BEGIN("atp_deep/pin-lpo-rec");
  {
    u32 ok = 0, gt = 0, lt = 0;
    for (u32 i = 0; i < LPO_N; i++) {
      Term s, t;
      lpo_pair(seed, i, &s, &t);
      thvm_lpo_invalidate();
      u64 p0 = g_lpo_poll_ctr;
      LpoCmp r = ref_lpo_rec(s, t, &g_lpo_cfg);
      u64 p1 = g_lpo_poll_ctr;
      u64 rd = lpo_memo_digest();
      thvm_lpo_invalidate();
      LpoCmp n = lpo_rec(s, t, &g_lpo_cfg);
      u64 p2 = g_lpo_poll_ctr;
      u64 nd = lpo_memo_digest();
      if (r == n && p1 - p0 == p2 - p1 && rd == nd) {
        ok++;
      }
      gt += (n == LPO_GT);
      lt += (n == LPO_LT);
    }
    CHECK_EQ(ok, (u32)LPO_N);
    CHECK(gt > 0u);
    CHECK(lt > 0u);
  }

  TEST_BEGIN("atp_deep/pin-lpo-flat-encode-walk-dag");
  {
    u32 ok_enc = 0, ok_flat = 0, ok_dag = 0, dags = 0;
    for (u32 i = 0; i < LPO_N; i++) {
      Term s, t;
      lpo_pair(seed, i, &s, &t);
      // the encoder, against the recursive one
      u32 rn = 0u, nn = 0u, nb = 0u;
      u8  re = ref_lpo_flat_encode_rec(s, &g_lpo_cfg, ra, &rn);
      u8  ne = lpo_flat_encode_rec(s, &g_lpo_cfg, g_lpo_flat_a, &nn);
      if (re == ne && rn == nn &&
          (!ne || memcmp(ra, g_lpo_flat_a, (size_t)nn * sizeof(KboFlatNode)) == 0)) {
        ok_enc++;
      }
      if (!ne || !lpo_flat_encode_rec(t, &g_lpo_cfg, g_lpo_flat_b, &nb)) {
        ok_flat++;
        ok_dag++;
        continue;
      }
      u32 span = (nn > nb) ? nn : nb;
      // the flat walk
      lpo_flat_epoch_bump();
      u64 c0 = g_lpo_flat_rec_calls, h0 = g_lpo_flat_memo_hits;
      u64 k0 = g_lpo_flat_compute_calls, p0 = g_lpo_poll_ctr;
      LpoCmp r = ref_lpo_flat_rec_compute(g_lpo_flat_a, 0u, g_lpo_flat_b, 0u,
                                          &g_lpo_cfg);
      u64 c1 = g_lpo_flat_rec_calls, h1 = g_lpo_flat_memo_hits;
      u64 k1 = g_lpo_flat_compute_calls, p1 = g_lpo_poll_ctr;
      u64 rd = lpo_flat_memo_digest(span);
      lpo_flat_epoch_bump();
      LpoCmp n = lpo_flat_run(g_lpo_flat_a, 0u, g_lpo_flat_b, 0u, &g_lpo_cfg);
      u64 c2 = g_lpo_flat_rec_calls, h2 = g_lpo_flat_memo_hits;
      u64 k2 = g_lpo_flat_compute_calls, p2 = g_lpo_poll_ctr;
      u64 nd = lpo_flat_memo_digest(span);
      if (r == n && rd == nd && c1 - c0 == c2 - c1 && h1 - h0 == h2 - h1 &&
          k1 - k0 == k2 - k1 && p1 - p0 == p2 - p1) {
        ok_flat++;
      }
      // the DAG walk, over the same encoding
      if (!lpo_dag_build(nn, nb, &g_lpo_cfg)) {
        ok_dag++;
        continue;
      }
      dags++;
      c0 = g_lpo_flat_rec_calls; h0 = g_lpo_flat_memo_hits;
      k0 = g_lpo_flat_compute_calls; p0 = g_lpo_poll_ctr;
      r = ref_lpo_dag_compute(g_ld_cls[0][0], g_ld_cls[1][0]);
      c1 = g_lpo_flat_rec_calls; h1 = g_lpo_flat_memo_hits;
      k1 = g_lpo_flat_compute_calls; p1 = g_lpo_poll_ctr;
      rd = lpo_dag_memo_digest();
      (void)lpo_dag_build(nn, nb, &g_lpo_cfg);
      n = lpo_dag_compute(g_ld_cls[0][0], g_ld_cls[1][0]);
      c2 = g_lpo_flat_rec_calls; h2 = g_lpo_flat_memo_hits;
      k2 = g_lpo_flat_compute_calls; p2 = g_lpo_poll_ctr;
      nd = lpo_dag_memo_digest();
      if (r == n && rd == nd && c1 - c0 == c2 - c1 && h1 - h0 == h2 - h1 &&
          k1 - k0 == k2 - k1 && p1 - p0 == p2 - p1) {
        ok_dag++;
      }
    }
    CHECK_EQ(ok_enc, (u32)LPO_N);
    CHECK_EQ(ok_flat, (u32)LPO_N);
    CHECK_EQ(ok_dag, (u32)LPO_N);
    CHECK(dags > 0u);
  }

  thvm_lpo_set_deadline_hook(NULL);
  free(ra);
}

// The LPO reference recursions (a few frames of about 100 bytes a
// level) at 1,000-2,000 levels, run on the reference stack from inside
// the deep blocks.
typedef struct {
  Term a;    // the chain's bottom
  Term c;    // the constant it is compared with
  u32  na;
  u32  nb;   // c's flat encoding, already in g_lpo_flat_b
} LpoVsRef;

static void lpo_rec_vs_ref(void *p) {
  LpoVsRef *m  = (LpoVsRef *)p;
  Term      ma = mk_chain(2000u, 2u, m->a);
  thvm_lpo_invalidate();
  LpoCmp r  = ref_lpo_rec(ma, m->c, &g_lpo_cfg);
  u64    rd = lpo_memo_digest();
  thvm_lpo_invalidate();
  LpoCmp n  = lpo_rec(ma, m->c, &g_lpo_cfg);
  u64    nd = lpo_memo_digest();
  CHECK_EQ(r, n);
  CHECK_EQ(rd, nd);
}

static void lpo_flat_dag_vs_ref(void *p) {
  LpoVsRef *m  = (LpoVsRef *)p;
  u32       na = 0u;
  CHECK(lpo_flat_encode_rec(mk_chain(1000u, 2u, m->a), &g_lpo_cfg, g_lpo_flat_a, &na));
  lpo_flat_epoch_bump();
  LpoCmp r  = ref_lpo_flat_rec_compute(g_lpo_flat_a, 0u, g_lpo_flat_b, 0u, &g_lpo_cfg);
  u64    rd = lpo_flat_memo_digest(na);
  lpo_flat_epoch_bump();
  LpoCmp n  = lpo_flat_run(g_lpo_flat_a, 0u, g_lpo_flat_b, 0u, &g_lpo_cfg);
  u64    nd = lpo_flat_memo_digest(na);
  CHECK_EQ(r, n);
  CHECK_EQ(rd, nd);
  CHECK(lpo_dag_build(na, m->nb, &g_lpo_cfg));
  r  = ref_lpo_dag_compute(g_ld_cls[0][0], g_ld_cls[1][0]);
  rd = lpo_dag_memo_digest();
  CHECK(lpo_dag_build(na, m->nb, &g_lpo_cfg));
  n  = lpo_dag_compute(g_ld_cls[0][0], g_ld_cls[1][0]);
  nd = lpo_dag_memo_digest();
  CHECK_EQ(r, n);
  CHECK_EQ(rd, nd);
  CHECK_EQ(n, LPO_UN);
}

static void deep_lpo(u32 *seed) {
  (void)seed;
  lpo_cfg_init();
  // g (label 2) and c (label 3) share precedence 5; a (label 4) is below
  // them (3), b (label 5) above (9).  g^k(a) against c is UN at every
  // level, decided only at the bottom; g^k(b) against c is GT.
  g_lpo_prec[2] = 5u;
  g_lpo_prec[3] = 5u;
  g_lpo_prec[4] = 3u;
  g_lpo_prec[5] = 9u;
  Term c = term_new_ctr(3u, NULL, 0u);
  Term a = term_new_ctr(4u, NULL, 0u);
  Term b = term_new_ctr(5u, NULL, 0u);
  Term vals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term kid = term_new_ctr(9u, NULL, 0u);
    vals[v] = term_new_ctr(20u + v, &kid, v & 1u);
  }
  Term pat  = mk_deep(DEEP, 1u, term_new_fvr(4u), NULL, 1u);
  Term subj = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term same = mk_deep(DEEP, 1u, vals[4], vals, 1u);
  Term near = mk_deep(DEEP, 1u, vals[3], vals, 1u);

  TEST_BEGIN("atp_deep/deep-lpo-term-walks");
  {
    CHECK(lpo_eq(subj, same));
    CHECK(!lpo_eq(subj, near));
    CHECK(lpo_var_occurs_in(pat, 4u));
    CHECK(!lpo_var_occurs_in(pat, 7u));
    u32 seen[LPO_MAX_VAR];
    memset(seen, 0, sizeof seen);
    lpo_var_set_acc(pat, seen);
    u32 nseen = 0u;
    for (u32 v = 0; v < LPO_MAX_VAR; v++) {
      nseen += seen[v];
    }
    CHECK_EQ(nseen, 5u);
    CHECK(lpo_vars_in_range(pat));
    CHECK(!lpo_vars_in_range(mk_deep(DEEP, 1u, term_new_fvr(70u), NULL, 1u)));
  }

  TEST_BEGIN("atp_deep/deep-lpo-rec");
  {
    // Past the 65536-node flat encoding: thvm_lpo falls back to lpo_rec.
    Term ga = mk_chain(DEEP, 2u, a);
    Term gb = mk_chain(DEEP, 2u, b);
    CHECK_EQ(thvm_lpo(ga, c, &g_lpo_cfg), LPO_UN);
    CHECK_EQ(thvm_lpo(c, ga, &g_lpo_cfg), LPO_UN);
    CHECK_EQ(thvm_lpo(gb, c, &g_lpo_cfg), LPO_GT);
    CHECK_EQ(thvm_lpo(c, gb, &g_lpo_cfg), LPO_LT);
    CHECK_EQ(thvm_lpo(gb, gb, &g_lpo_cfg), LPO_EQ);
    // 2,000 deep against the recursion, on its stack.
    LpoVsRef m = { a, c, 0u, 0u };
    deep_stack_run(REF_STACK_BYTES, lpo_rec_vs_ref, &m);
  }

  TEST_BEGIN("atp_deep/deep-lpo-dag-flat");
  {
    // Within the flat encoding cap: the DAG walk, 60,000 levels.
    Term ga = mk_chain(60000u, 2u, a);
    CHECK_EQ(thvm_lpo(ga, c, &g_lpo_cfg), LPO_UN);
    CHECK_EQ(thvm_lpo(c, ga, &g_lpo_cfg), LPO_UN);
    // The flat walk re-scans each level's slice in its Vortest
    // (quadratic in depth): 3,000 levels, and 1,000 against the
    // recursion, flat and DAG.
    u32 na = 0u, nb = 0u;
    CHECK(lpo_flat_encode_rec(mk_chain(3000u, 2u, a), &g_lpo_cfg, g_lpo_flat_a, &na));
    CHECK(lpo_flat_encode_rec(c, &g_lpo_cfg, g_lpo_flat_b, &nb));
    CHECK_EQ(lpo_flat_ref(&g_lpo_cfg), LPO_UN);
    LpoVsRef m = { a, c, 0u, nb };
    deep_stack_run(REF_STACK_BYTES, lpo_flat_dag_vs_ref, &m);
  }
  lpo_cfg_init();
}

int main(void) {
  thvm_init();
  TEST_BEGIN("atp_deep/negative-control");
  CHECK(deep_stack_catches_recursion());
  u32 seed = 0x6c8e9cf5u;
  pin_ri_walks(&seed);
  deep_stack_call(deep_ri_walks);
  pin_term_libs(&seed);
  deep_stack_call(deep_term_libs);
  pin_serialize(&seed);
  deep_stack_call(deep_serialize);
  pin_weight_order(&seed);
  deep_stack_call_seed(deep_weight_order, &seed);
  pin_gc_gj(&seed);
  deep_stack_call_seed(deep_gc_gj, &seed);
  pin_index_queue(&seed);
  deep_stack_call_seed(deep_index_queue, &seed);
  pin_ac(&seed);
  deep_stack_call(deep_ac);
  pin_ac_match();
  deep_stack_call(deep_ac_match);
  pin_ftdt(&seed);
  deep_stack_call(deep_ftdt);
  pin_kbo_memo(&seed);
  deep_stack_call(deep_kbo_memo);
  pin_lpo(&seed);
  deep_stack_call_seed(deep_lpo, &seed);
  thvm_free();
  TEST_REPORT();
}
