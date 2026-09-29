// test_fol.c -- First-order clause representation + binary resolution.

#include "../src/thvm.c"
#include "test.h"

// Predicate labels.
#define P_P  1u   // P(_)
#define P_Q  2u   // Q(_)
#define P_R  3u   // R(_,_)
// Object constants / functions.
#define L_a  10u
#define L_b  11u
#define L_f  12u
#define L_c  13u
#define L_d  14u

static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0); }
static Term v(u32 id) { return term_new_fvr(id); }
static Term pred1(u32 lab, Term x) { Term cs[1] = {x}; return term_new_ctr(lab, cs, 1); }
static Term pred2(u32 lab, Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(lab, cs, 2); }

// The distribution fol_distribute replaced (every joined side
// distributed again), kept as the oracle its terms must equal.
static Term dist_ref(Term f) {
  if (term_tag(f) != TAG_CTR) return f;
  u32 lab = term_ext(f);
  if (lab == FOL_LAB_AND) {
    return fol_mk_and(dist_ref(term_ctr_at(f, 0u)), dist_ref(term_ctr_at(f, 1u)));
  }
  if (lab != FOL_LAB_OR) return f;
  Term dp = dist_ref(term_ctr_at(f, 0u));
  Term dq = dist_ref(term_ctr_at(f, 1u));
  if (term_tag(dp) == TAG_CTR && term_ext(dp) == FOL_LAB_AND) {
    return fol_mk_and(dist_ref(fol_mk_or(term_ctr_at(dp, 0u), dq)),
                      dist_ref(fol_mk_or(term_ctr_at(dp, 1u), dq)));
  }
  if (term_tag(dq) == TAG_CTR && term_ext(dq) == FOL_LAB_AND) {
    return fol_mk_and(dist_ref(fol_mk_or(dp, term_ctr_at(dq, 0u))),
                      dist_ref(fol_mk_or(dp, term_ctr_at(dq, 1u))));
  }
  return fol_mk_or(dp, dq);
}

// --- recursive oracles ------------------------------------------------
// The clausification walkers as they were before their explicit stacks
// (src/fol/cnf.c at 156e7d0f), with every pair of calls sequenced left
// to right: the order clang gave their argument lists, and the CASC
// binary is a clang (zig cc) build.  The walkers must agree with them
// term for term, label for label, clause for clause.
static Term nnf_ref(Term f, u8 neg) {
  if (term_tag(f) != TAG_CTR) return neg ? fol_mk_not(f) : f;
  u32 lab = term_ext(f);
  Term p = term_ctr_at(f, 0u), q = term_ctr_at(f, 1u);
  switch (lab) {
    case FOL_LAB_NOT: return nnf_ref(p, (u8)(neg ^ 1u));
    case FOL_LAB_AND: case FOL_LAB_OR: {
      Term a = nnf_ref(p, neg);
      Term b = nnf_ref(q, neg);
      return ((lab == FOL_LAB_AND) ^ neg) ? fol_mk_and(a, b) : fol_mk_or(a, b);
    }
    case FOL_LAB_IMP: {
      Term a = nnf_ref(p, (u8)(neg ^ 1u));
      Term b = nnf_ref(q, neg);
      return neg ? fol_mk_and(a, b) : fol_mk_or(a, b);
    }
    case FOL_LAB_IFF: {
      if (neg) {
        Term a = nnf_ref(p, 0u), b = nnf_ref(q, 1u);
        Term l = fol_mk_and(a, b);
        Term c = nnf_ref(p, 1u), d = nnf_ref(q, 0u);
        Term r = fol_mk_and(c, d);
        return fol_mk_or(l, r);
      }
      Term a = nnf_ref(p, 1u), b = nnf_ref(q, 0u);
      Term l = fol_mk_or(a, b);
      Term c = nnf_ref(p, 0u), d = nnf_ref(q, 1u);
      Term r = fol_mk_or(c, d);
      return fol_mk_and(l, r);
    }
    case FOL_LAB_ALL: case FOL_LAB_EX: {
      Term b = nnf_ref(q, neg);
      return ((lab == FOL_LAB_ALL) ^ neg) ? fol_mk_all(p, b) : fol_mk_ex(p, b);
    }
    default: return neg ? fol_mk_not(f) : f;
  }
}

static Term vsubst_ref(Term t, u32 vid, Term repl) {
  if (term_tag(t) == TAG_FVR) return (term_ext(t) == vid) ? repl : t;
  if (term_tag(t) != TAG_CTR) return t;
  u32 lab = term_ext(t);
  if (lab == FOL_LAB_ALL || lab == FOL_LAB_EX) {
    Term var = term_ctr_at(t, 0u);
    if (term_tag(var) == TAG_FVR && term_ext(var) == vid) return t;
  }
  u32 n = term_ctr_n(t);
  if (n == 0u) return t;
  Term *kids = (Term *)malloc((size_t)n * sizeof(Term));
  u8 changed = 0u;
  for (u32 i = 0; i < n; i++) {
    kids[i] = vsubst_ref(term_ctr_at(t, i), vid, repl);
    if (kids[i] != term_ctr_at(t, i)) changed = 1u;
  }
  Term r = changed ? term_new_ctr(lab, kids, n) : t;
  free(kids);
  return r;
}

static Term sk_ref(Term f, u32 n_univ) {
  if (term_tag(f) != TAG_CTR) return f;
  u32 lab = term_ext(f);
  switch (lab) {
    case FOL_LAB_NOT: {
      Term a = sk_ref(term_ctr_at(f, 0u), n_univ);
      return fol_mk_not(a);
    }
    case FOL_LAB_AND: case FOL_LAB_OR: case FOL_LAB_IMP: case FOL_LAB_IFF: {
      Term kids[2];
      kids[0] = sk_ref(term_ctr_at(f, 0u), n_univ);
      kids[1] = sk_ref(term_ctr_at(f, 1u), n_univ);
      return term_new_ctr(lab, kids, 2u);
    }
    case FOL_LAB_ALL:
      fol_sk_fit(&g_fol_sk_univ, &g_fol_sk_univ_cap, n_univ);
      g_fol_sk_univ[n_univ] = term_ctr_at(f, 0u);
      return sk_ref(term_ctr_at(f, 1u), n_univ + 1u);
    case FOL_LAB_EX: {
      Term var = term_ctr_at(f, 0u), body = term_ctr_at(f, 1u);
      if (term_tag(var) != TAG_FVR) return f;
      if (FOL_LAB_SKOLEM_BASE + g_fol_skolem_next > (u32)EXT_MASK) {
        g_fol_sk_out = 1u;
        return f;
      }
      u32 sk_label = FOL_LAB_SKOLEM_BASE + g_fol_skolem_next++;
      Term sk;
      if (n_univ == 0u) {
        sk = term_new_ctr(sk_label, NULL, 0u);
      } else if (n_univ <= REWRITE_MAX_ARITY) {
        sk = term_new_ctr(sk_label, g_fol_sk_univ, n_univ);
      } else {
        fol_vepoch_next();
        fol_stamp_vars(body);
        u32 na = 0u;
        for (u32 i = 0; i < n_univ; i++) {
          Term u = g_fol_sk_univ[i];
          if (term_tag(u) != TAG_FVR || !fol_var_stamped(term_ext(u))) continue;
          u32 dup = 0u;
          for (u32 j = 0; j < na && !dup; j++) dup = (g_fol_sk_args[j] == u);
          if (dup) continue;
          fol_sk_fit(&g_fol_sk_args, &g_fol_sk_args_cap, na);
          g_fol_sk_args[na++] = u;
        }
        sk = term_new_ctr(sk_label, na ? g_fol_sk_args : NULL, na);
      }
      return sk_ref(vsubst_ref(body, term_ext(var), sk), n_univ);
    }
    default: return f;
  }
}

static Term or_cnf_ref(Term x, Term y) {
  if (term_tag(x) == TAG_CTR && term_ext(x) == FOL_LAB_AND) {
    Term a = or_cnf_ref(term_ctr_at(x, 0u), y);
    Term b = or_cnf_ref(term_ctr_at(x, 1u), y);
    return fol_mk_and(a, b);
  }
  if (term_tag(y) == TAG_CTR && term_ext(y) == FOL_LAB_AND) {
    Term a = or_cnf_ref(x, term_ctr_at(y, 0u));
    Term b = or_cnf_ref(x, term_ctr_at(y, 1u));
    return fol_mk_and(a, b);
  }
  return fol_mk_or(x, y);
}

static Term distribute_ref(Term f) {
  if (term_tag(f) != TAG_CTR) return f;
  u32 lab = term_ext(f);
  if (lab != FOL_LAB_AND && lab != FOL_LAB_OR) return f;
  Term a = distribute_ref(term_ctr_at(f, 0u));
  Term b = distribute_ref(term_ctr_at(f, 1u));
  return (lab == FOL_LAB_AND) ? fol_mk_and(a, b) : or_cnf_ref(a, b);
}

static void extract_or_ref(Term f, FolLit **lits, u32 *n, u32 *cap) {
  if (term_tag(f) == TAG_CTR && term_ext(f) == FOL_LAB_OR) {
    extract_or_ref(term_ctr_at(f, 0u), lits, n, cap);
    extract_or_ref(term_ctr_at(f, 1u), lits, n, cap);
    return;
  }
  FolLit lit;
  if (term_tag(f) == TAG_CTR && term_ext(f) == FOL_LAB_NOT) {
    lit.atom = term_ctr_at(f, 0u);
    lit.sign = 1u;
  } else {
    lit.atom = f;
    lit.sign = 0u;
  }
  fol_extract_lit(lits, n, cap, lit);
}

static void extract_and_ref(Term f, FolClause ***out, u32 *n, u32 *cap) {
  if (term_tag(f) == TAG_CTR && term_ext(f) == FOL_LAB_AND) {
    extract_and_ref(term_ctr_at(f, 0u), out, n, cap);
    extract_and_ref(term_ctr_at(f, 1u), out, n, cap);
    return;
  }
  FolLit *lits = NULL;
  u32 nl = 0u, capl = 0u;
  extract_or_ref(f, &lits, &nl, &capl);
  if (nl > FOL_MAX_LITS) fol_append_wide(out, n, cap, lits, nl);
  else fol_push_lits(out, n, cap, lits, nl);
  free(lits);
}

static void collect_vars_ref(Term t, u32 *n) {
  if (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    if (fol_var_stamped(id)) return;
    fol_vstamp_fit(id);
    g_fol_vstamp[id] = g_fol_vepoch;
    fol_sk_fit(&g_fol_ts_vars, &g_fol_ts_vars_cap, *n);
    g_fol_ts_vars[(*n)++] = t;
    return;
  }
  if (term_tag(t) != TAG_CTR) return;
  u32 nk = term_ctr_n(t);
  for (u32 i = 0; i < nk; i++) collect_vars_ref(term_ctr_at(t, i), n);
}

static FolLit tseitin_ref(Term f, FolClause ***out, u32 *n, u32 *cap) {
  if (term_tag(f) == TAG_CTR && term_ext(f) == FOL_LAB_NOT)
    return (FolLit){ .atom = term_ctr_at(f, 0u), .sign = 1u };
  if (term_tag(f) != TAG_CTR || !fol_is_connective(term_ext(f)))
    return (FolLit){ .atom = f, .sign = 0u };
  u32 lab = term_ext(f);
  FolLit b = tseitin_ref(term_ctr_at(f, 0u), out, n, cap);
  FolLit c = tseitin_ref(term_ctr_at(f, 1u), out, n, cap);
  u32 nv = 0u;
  fol_vepoch_next();
  collect_vars_ref(f, &nv);
  Term aux = fol_fresh_aux(g_fol_ts_vars, nv);
  FolLit na = { .atom = aux, .sign = 1u }, pa = { .atom = aux, .sign = 0u };
  FolLit nb = { .atom = b.atom, .sign = (u8)(b.sign ^ 1u) };
  FolLit nc = { .atom = c.atom, .sign = (u8)(c.sign ^ 1u) };
  if (lab == FOL_LAB_AND) {
    FolLit c1[2] = { na, b }, c2[2] = { na, c }, c3[3] = { nb, nc, pa };
    fol_tseitin_emit(out, n, cap, c1, 2);
    fol_tseitin_emit(out, n, cap, c2, 2);
    fol_tseitin_emit(out, n, cap, c3, 3);
  } else {
    FolLit c1[3] = { na, b, c }, c2[2] = { nb, pa }, c3[2] = { nc, pa };
    fol_tseitin_emit(out, n, cap, c1, 3);
    fol_tseitin_emit(out, n, cap, c2, 2);
    fol_tseitin_emit(out, n, cap, c3, 2);
  }
  return pa;
}

// Naive-distribution clause count, saturating (small formulas only).
static u32 est_ref(Term f) {
  if (term_tag(f) != TAG_CTR) return 1u;
  u32 lab = term_ext(f);
  if (lab != FOL_LAB_AND && lab != FOL_LAB_OR) return 1u;
  u64 a = est_ref(term_ctr_at(f, 0u)), b = est_ref(term_ctr_at(f, 1u));
  u64 r = (lab == FOL_LAB_AND) ? a + b : a * b;
  return (r > 100000u) ? 100000u : (u32)r;
}

// Structural equality over an explicit stack (the deep cases outgrow any
// recursive comparison).
static int teq(Term a, Term b) {
  static Term *st = NULL;
  static u32 cap = 0u;
  u32 n = 0u;
  if (cap == 0u) { cap = 1024u; st = (Term *)malloc(cap * sizeof(Term)); }
  st[n++] = a;
  st[n++] = b;
  while (n > 0u) {
    Term y = st[--n], x = st[--n];
    if (x == y) continue;
    if (term_tag(x) != TAG_CTR || term_tag(y) != TAG_CTR) return 0;
    u32 k = term_ctr_n(x);
    if (term_ext(x) != term_ext(y) || term_ctr_n(y) != k) return 0;
    if (n + 2u * k > cap) {
      while (n + 2u * k > cap) cap *= 2u;
      st = (Term *)realloc(st, cap * sizeof(Term));
    }
    for (u32 i = 0; i < k; i++) {
      st[n++] = term_ctr_at(x, i);
      st[n++] = term_ctr_at(y, i);
    }
  }
  return 1;
}

static int clauses_eq(FolClause **a, u32 na, FolClause **b, u32 nb) {
  if (na != nb) return 0;
  for (u32 c = 0; c < na; c++) {
    if (a[c]->n_lits != b[c]->n_lits) return 0;
    for (u32 i = 0; i < a[c]->n_lits; i++) {
      if (a[c]->lits[i].sign != b[c]->lits[i].sign) return 0;
      if (!teq(a[c]->lits[i].atom, b[c]->lits[i].atom)) return 0;
    }
  }
  return 1;
}

static void clauses_free(FolClause **cs, u32 n) {
  for (u32 c = 0; c < n; c++) fol_clause_free(cs[c]);
  free(cs);
}

// A random first-order formula over every connective, depth <= d, with
// variables drawn from ids [0, nv) (binders over the same id nest and
// shadow; LCG state *s).
static Term rand_fol(u32 *s, u32 d, u32 nv) {
  *s = *s * 1103515245u + 12345u;
  u32 r = (*s >> 16) % 11u;
  u32 x = (*s >> 8) % nv, y = (*s >> 24) % nv;
  if (d == 0u || r < 3u) {
    if (r == 0u) return pred1(P_P, v(x));
    if (r == 1u) return pred2(P_R, v(x), k(L_a + (y & 3u)));
    return pred2(P_R, v(x), v(y));
  }
  if (r == 3u) return fol_mk_not(rand_fol(s, d - 1u, nv));
  if (r == 9u) return fol_mk_all(v(x), rand_fol(s, d - 1u, nv));
  if (r == 10u) return fol_mk_ex(v(x), rand_fol(s, d - 1u, nv));
  Term a = rand_fol(s, d - 1u, nv);
  Term b = rand_fol(s, d - 1u, nv);
  if (r <= 5u) return fol_mk_and(a, b);
  if (r <= 7u) return fol_mk_or(a, b);
  return (r == 8u) ? fol_mk_imp(a, b) : fol_mk_iff(a, b);
}

// A random matrix under random quantifier blocks: 1..4 blocks of 1..6
// binders each (universal or existential per block), variables drawn
// from [0, nv) -- runs of existential binders, variables bound twice,
// existential binders over a universal's name (capture), all under
// narrow or wide universal scopes.
static Term rand_prefixed(u32 *s, u32 nv, u32 wide) {
  Term f = rand_fol(s, 4u, nv);
  *s = *s * 1103515245u + 12345u;
  u32 nb = 1u + (*s >> 16) % 4u;
  for (u32 b = 0; b < nb; b++) {
    *s = *s * 1103515245u + 12345u;
    u32 ex = (*s >> 16) & 1u, len = 1u + (*s >> 20) % 6u;
    for (u32 i = 0; i < len; i++) {
      *s = *s * 1103515245u + 12345u;
      Term x = v((*s >> 16) % nv);
      f = ex ? fol_mk_ex(x, f) : fol_mk_all(x, f);
    }
  }
  // The wide scope's universals overlap the matrix's variables (ids 3
  // up): some are mentioned, some rebound below, some neither.
  for (u32 i = wide; i-- > 0u;) f = fol_mk_all(v(3u + i), f);
  return f;
}

// A random NNF formula over P/Q literals of depth <= d (LCG state *s).
static Term rand_nnf(u32 *s, u32 d) {
  *s = *s * 1103515245u + 12345u;
  u32 r = (*s >> 16) & 7u;
  if (d == 0u || r < 2u) {
    Term a = pred1((r & 1u) ? P_P : P_Q, k(L_a + ((*s >> 20) & 3u)));
    return (r == 0u) ? fol_mk_not(a) : a;
  }
  Term x = rand_nnf(s, d - 1u);
  Term y = rand_nnf(s, d - 1u);
  return (r & 1u) ? fol_mk_and(x, y) : fol_mk_or(x, y);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("fol/empty-clause-detection");
  {
    FolClause *c = fol_clause_new(0);
    CHECK(c != NULL);
    CHECK(fol_clause_is_empty(c));
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/clause-eq-modulo-order");
  {
    // P(a) v ¬Q(b)  ==  ¬Q(b) v P(a)  (multiset of signed atoms).
    FolClause *a = fol_clause_new(2);
    a->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    a->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    FolClause *b = fol_clause_new(2);
    b->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    b->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    CHECK(fol_clause_eq(a, b));
    // Different sign on otherwise-equal atoms => not equal.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    c->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    CHECK(!fol_clause_eq(a, c));
    fol_clause_free(a);
    fol_clause_free(b);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/binary-resolution-ground");
  {
    // C1 = P(a)        (one positive literal)
    // C2 = ¬P(a) v Q(b) (negative P(a), positive Q(b))
    // Resolvent = Q(b).
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(2);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    c2->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(r->lits[0].sign == 0);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_Q, k(L_b))));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/binary-resolution-with-unification");
  {
    // C1 = P(x)         (variable x)
    // C2 = ¬P(a) v Q(b) (ground)
    // Resolvent (after σ = {x ↦ a}) = Q(b).
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    FolClause *c2 = fol_clause_new(2);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    c2->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(r->lits[0].sign == 0);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_Q, k(L_b))));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/resolution-to-empty-clause");
  {
    // C1 = P(a)   C2 = ¬P(a).  Resolvent is the empty clause (FALSE).
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(fol_clause_is_empty(r));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/resolution-fails-same-sign");
  {
    // Two positives can't resolve directly.
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r == NULL);
    fol_clause_free(c1);
    fol_clause_free(c2);
  }

  TEST_BEGIN("fol/resolution-fails-non-unifying-atoms");
  {
    // P(a) and ¬P(b): different ground atoms => no unifier => no resolvent.
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 1 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r == NULL);
    fol_clause_free(c1);
    fol_clause_free(c2);
  }

  TEST_BEGIN("fol/resolution-with-shared-vars");
  {
    // C1 = P(x) v Q(x)   (x in both literals)
    // C2 = ¬P(a)
    // Resolvent: Q(x') where x' is the c1-side variable renamed (or
    // not -- here it's the c1's x, since c2 has no vars).
    // After σ = {x ↦ a}, resolvent = Q(a).
    FolClause *c1 = fol_clause_new(2);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    c1->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(r->lits[0].sign == 0);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_Q, k(L_a))));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/factoring");
  {
    // C = P(x) v P(a).  Factor at (0, 1): σ = {x ↦ a}, drop one P-lit.
    // Result = P(a).
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *r = fol_factor(c, 0, 1);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(r->lits[0].sign == 0);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_P, k(L_a))));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/factoring-non-unifying");
  {
    // P(a) v P(b): no unifier on distinct grounds.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 0 };
    FolClause *r = fol_factor(c, 0, 1);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/factoring-sign-mismatch");
  {
    // P(a) v ¬P(a): opposite signs -- factoring (same-polarity) fails.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    FolClause *r = fol_factor(c, 0, 1);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/two-place-predicate");
  {
    // C1 = R(x, b),  C2 = ¬R(a, y).  σ = {x ↦ a, y ↦ b'} (b in c2 renamed).
    // Actually y in c2 is FVR-id 0 too; after rename y becomes FVR-id
    // (FOL_RENAME_OFFSET).  σ unifies (x, b) with (a, y_renamed):
    //   x ↦ a, y_renamed ↦ b.  Empty resolvent.
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(P_R, v(0), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred2(P_R, k(L_a), v(0)), .sign = 1 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(fol_clause_is_empty(r));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  // === paramodulation ==========================================

  TEST_BEGIN("fol/paramod-basic");
  {
    // eq_clause = (a = b)        positive equality
    // target    = P(a)
    // path      = [0]            (P's child 0 = a)
    // paramodulant = P(b)
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));      // FOL_LAB_EQ = 0
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, /*swap*/ 0, tgt, 0, path, 1);
    CHECK(p != NULL);
    CHECK(p->n_lits == 1);
    CHECK(p->lits[0].sign == 0);
    CHECK(kbo_eq(p->lits[0].atom, pred1(P_P, k(L_b))));
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(p);
  }

  TEST_BEGIN("fol/paramod-swapped-orientation");
  {
    // eq_clause = (a = b)
    // target    = P(b)
    // path      = [0]
    // With swap=1, use orientation b -> a; paramodulant = P(a).
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, /*swap*/ 1, tgt, 0, path, 1);
    CHECK(p != NULL);
    CHECK(p->n_lits == 1);
    CHECK(kbo_eq(p->lits[0].atom, pred1(P_P, k(L_a))));
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(p);
  }

  TEST_BEGIN("fol/paramod-with-unification");
  {
    // eq_clause = (f(x) = b)     (with variable x)
    // target    = P(f(a))
    // path      = [0]            (P's child 0 = f(a))
    // unify f(x) with f(a): x ↦ a.  paramodulant = P(b).
    Term f_a = pred1(L_f, k(L_a));
    Term eq_atom = pred2(0u, pred1(L_f, v(0)), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = eq_atom, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, f_a), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, 0, tgt, 0, path, 1);
    CHECK(p != NULL);
    CHECK(p->n_lits == 1);
    CHECK(kbo_eq(p->lits[0].atom, pred1(P_P, k(L_b))));
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(p);
  }

  TEST_BEGIN("fol/paramod-fails-on-negative-equality");
  {
    // eq_clause = ¬(a = b): negative equality -- paramod fires only on
    // POSITIVE equality.  Should return NULL.
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 1 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, 0, tgt, 0, path, 1);
    CHECK(p == NULL);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
  }

  TEST_BEGIN("fol/paramod-into-variable-position-fails");
  {
    // target = P(x), path [0] = x (FVR).  Paramod into variable
    // positions is disallowed (no useful CP).
    Term eq_atom = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = eq_atom, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, v(5)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, 0, tgt, 0, path, 1);
    CHECK(p == NULL);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
  }

  TEST_BEGIN("fol/paramod-multi-literal");
  {
    // eq_clause = (a = b) v Q(c)
    // target    = P(a) v R(a, a)
    // paramod at target_idx=0, path=[0]:
    //   drop eq -> keep Q(c).  Replace P(a) with P(b).
    //   Final: Q(c) v P(b) v R(a, a).
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(2);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    eqc->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    FolClause *tgt = fol_clause_new(2);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    tgt->lits[1] = (FolLit){ .atom = pred2(P_R, k(L_a), k(L_a)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, 0, tgt, 0, path, 1);
    CHECK(p != NULL);
    CHECK(p->n_lits == 3);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(p);
  }

  // === reflexivity resolution =====================================

  TEST_BEGIN("fol/reflex-resolve-trivial");
  {
    // C = ¬(a = a) v P(b).  Atoms unify trivially => resolvent = P(b).
    Term a_eq_a = pred2(0u, k(L_a), k(L_a));
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = a_eq_a, .sign = 1 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 0 };
    FolClause *r = fol_reflex_resolve(c, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_P, k(L_b))));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/reflex-resolve-with-unifier");
  {
    // C = ¬(x = a) v P(x).  σ = {x ↦ a} unifies x and a.
    // Resolvent = P(a).
    Term eq_atom = pred2(0u, v(0), k(L_a));
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = eq_atom, .sign = 1 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    FolClause *r = fol_reflex_resolve(c, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_P, k(L_a))));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/reflex-resolve-fails-positive");
  {
    // Positive equality literal -- reflex-resolve requires negative.
    Term a_eq_a = pred2(0u, k(L_a), k(L_a));
    FolClause *c = fol_clause_new(1);
    c->lits[0] = (FolLit){ .atom = a_eq_a, .sign = 0 };
    FolClause *r = fol_reflex_resolve(c, 0);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/reflex-resolve-fails-non-eq");
  {
    // ¬P(a) is not an equality literal.
    FolClause *c = fol_clause_new(1);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    FolClause *r = fol_reflex_resolve(c, 0);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  // === subsumption =================================================

  TEST_BEGIN("fol/subsumes-identical");
  {
    FolClause *a = fol_clause_new(1);
    a->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *b = fol_clause_new(1);
    b->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    CHECK(fol_subsumes(a, b));
    CHECK(fol_subsumes(b, a));
    fol_clause_free(a);
    fol_clause_free(b);
  }

  TEST_BEGIN("fol/subsumes-via-instance");
  {
    // P(x) subsumes P(a) (σ = {x ↦ a}).  Not the reverse.
    FolClause *general = fol_clause_new(1);
    general->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    FolClause *specific = fol_clause_new(1);
    specific->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    CHECK(fol_subsumes(general, specific));
    CHECK(!fol_subsumes(specific, general));
    fol_clause_free(general);
    fol_clause_free(specific);
  }

  TEST_BEGIN("fol/subsumes-subset-of-literals");
  {
    // P(a) subsumes P(a) v Q(b)  (the proper-subset case).
    FolClause *small = fol_clause_new(1);
    small->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *big = fol_clause_new(2);
    big->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    big->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    CHECK(fol_subsumes(small, big));
    CHECK(!fol_subsumes(big, small));
    fol_clause_free(small);
    fol_clause_free(big);
  }

  TEST_BEGIN("fol/subsumes-sign-mismatch");
  {
    // P(a) does NOT subsume ¬P(a) (signs differ).
    FolClause *a = fol_clause_new(1);
    a->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *b = fol_clause_new(1);
    b->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    CHECK(!fol_subsumes(a, b));
    fol_clause_free(a);
    fol_clause_free(b);
  }

  TEST_BEGIN("fol/subsumes-shared-vars-consistent");
  {
    // P(x) v Q(x) subsumes P(a) v Q(a):
    //   σ = {x ↦ a} maps consistently across both literals.
    FolClause *gen = fol_clause_new(2);
    gen->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    gen->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *spec = fol_clause_new(2);
    spec->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    spec->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 0 };
    CHECK(fol_subsumes(gen, spec));
    // Inconsistent: P(x) v Q(x) does NOT subsume P(a) v Q(b).
    FolClause *bad = fol_clause_new(2);
    bad->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    bad->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    CHECK(!fol_subsumes(gen, bad));
    fol_clause_free(gen);
    fol_clause_free(spec);
    fol_clause_free(bad);
  }

  TEST_BEGIN("fol/subsumes-empty-clause");
  {
    // The empty clause subsumes everything.
    FolClause *empty = fol_clause_new(0);
    FolClause *anything = fol_clause_new(2);
    anything->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    anything->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    CHECK(fol_subsumes(empty, anything));
    // But a non-empty clause does not subsume the empty clause (no
    // literal to match into).
    CHECK(!fol_subsumes(anything, empty));
    fol_clause_free(empty);
    fol_clause_free(anything);
  }

  TEST_BEGIN("fol/subsumes-permuted");
  {
    // P(a) v Q(b)  subsumes  Q(b) v P(a)  (multiset match).
    FolClause *a = fol_clause_new(2);
    a->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    a->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    FolClause *b = fol_clause_new(2);
    b->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    b->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    CHECK(fol_subsumes(a, b));
    CHECK(fol_subsumes(b, a));
    fol_clause_free(a);
    fol_clause_free(b);
  }

  // === tautology ====================================================

  TEST_BEGIN("fol/tautology-pm-A");
  {
    // P(a) v ¬P(a) is a tautology.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    CHECK(fol_is_tautology(c));
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/tautology-reflex-eq");
  {
    // (a = a) is a tautology.
    Term a_eq_a = pred2(0u, k(L_a), k(L_a));
    FolClause *c = fol_clause_new(1);
    c->lits[0] = (FolLit){ .atom = a_eq_a, .sign = 0 };
    CHECK(fol_is_tautology(c));
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/tautology-negative");
  {
    // P(a) v Q(b): not a tautology.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    CHECK(!fol_is_tautology(c));
    // ¬(a = a) is also NOT a tautology (it's UNSAT in fact).
    Term a_eq_a = pred2(0u, k(L_a), k(L_a));
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = a_eq_a, .sign = 1 };
    CHECK(!fol_is_tautology(c2));
    fol_clause_free(c);
    fol_clause_free(c2);
  }

  // === equality factoring =========================================

  TEST_BEGIN("fol/eq-factor-ground");
  {
    // C = (a = b) v (a = c).
    // σ unifies a with a (trivial), derive (a = b) v ¬(b = c).
    Term a_b = pred2(0u, k(L_a), k(L_b));
    Term a_c = pred2(0u, k(L_a), k(L_c));
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = a_b, .sign = 0 };
    c->lits[1] = (FolLit){ .atom = a_c, .sign = 0 };
    FolClause *r = fol_eq_factor(c, 0, 1);
    CHECK(r != NULL);
    CHECK(r->n_lits == 2);
    // [0] = (a = b) positive
    CHECK(r->lits[0].sign == 0);
    CHECK(fol_atom_is_eq(r->lits[0].atom));
    // [1] = ¬(b = c) negative
    CHECK(r->lits[1].sign == 1);
    CHECK(fol_atom_is_eq(r->lits[1].atom));
    CHECK(kbo_eq(term_ctr_at(r->lits[1].atom, 0), k(L_b)));
    CHECK(kbo_eq(term_ctr_at(r->lits[1].atom, 1), k(L_c)));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/eq-factor-with-unification");
  {
    // C = (x = b) v (a = c).  σ = {x ↦ a} unifies x with a.
    // Result: (a = b) v ¬(b = c).
    Term x_b = pred2(0u, v(0), k(L_b));
    Term a_c = pred2(0u, k(L_a), k(L_c));
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = x_b, .sign = 0 };
    c->lits[1] = (FolLit){ .atom = a_c, .sign = 0 };
    FolClause *r = fol_eq_factor(c, 0, 1);
    CHECK(r != NULL);
    CHECK(r->n_lits == 2);
    // [0] = (a = b)
    CHECK(kbo_eq(term_ctr_at(r->lits[0].atom, 0), k(L_a)));
    CHECK(kbo_eq(term_ctr_at(r->lits[0].atom, 1), k(L_b)));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/eq-factor-non-eq-fails");
  {
    // Non-equality literal -- factoring doesn't fire.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *r = fol_eq_factor(c, 0, 1);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/eq-factor-non-unify-fails");
  {
    // (a = b) v (c = d): a doesn't unify with c.
    Term a_b = pred2(0u, k(L_a), k(L_b));
    Term c_d = pred2(0u, k(L_c), k(L_a));   // use a-distinct LHS for clarity
    FolClause *cl = fol_clause_new(2);
    cl->lits[0] = (FolLit){ .atom = a_b, .sign = 0 };
    cl->lits[1] = (FolLit){ .atom = c_d, .sign = 0 };
    FolClause *r = fol_eq_factor(cl, 0, 1);
    CHECK(r == NULL);
    fol_clause_free(cl);
  }

  // === saturation loop ============================================

  TEST_BEGIN("fol/sat-trivial-resolution");
  {
    // C1 = P(a)         (one clause)
    // C2 = ¬P(a)        (its negation)
    // Saturation should derive the empty clause -> PROVED.
    CnfState *s = cnf_init(64);
    CHECK(s != NULL);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-three-clause-chain");
  {
    // Classic example:
    //   C1: P(a)
    //   C2: ¬P(x) v Q(x)
    //   C3: ¬Q(a)
    // Saturate: resolve C1+C2 => Q(a).  Then Q(a)+C3 => empty clause.
    CnfState *s = cnf_init(128);
    CHECK(s != NULL);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(2);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 1 };
    c2->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-unsat-no-resolve");
  {
    // C1: P(a)
    // C2: Q(b)
    // No common predicate, no resolution possible -> QUEUE_EMPTY.
    CnfState *s = cnf_init(64);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_QUEUE_EMPTY);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-step-cap");
  {
    // P(x) v P(f(x)): self-resolves into P(f(f(x))), P(f(f(f(x)))),
    // etc. (no goal, no termination).  step_cap = 4 should bound it
    // to ABORTED before the queue grows unboundedly.
    CnfState *s = cnf_init(4);
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, pred1(L_f, v(0))), .sign = 1 };
    cnf_add_clause(s, c);
    AtpStatus st = cnf_run(s);
    // Saturation must reach a terminal status -- either QUEUE_EMPTY
    // (if the inference chain dies) or ABORTED via step_cap.
    CHECK(st == ATP_QUEUE_EMPTY || st == ATP_ABORTED || st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-tautology-dropped");
  {
    // C1: P(x) v ¬P(x) -- tautology.
    // After adding it (it goes to passive then is selected as given),
    // it survives in the active set but generates no useful CPs.
    // No goal -> QUEUE_EMPTY.
    CnfState *s = cnf_init(64);
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 1 };
    cnf_add_clause(s, c);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_QUEUE_EMPTY || st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-eq-reflexivity-resolution");
  {
    // ¬(x = x): negative reflexive equality.
    // Reflex-resolve fires immediately, yielding the empty clause.
    CnfState *s = cnf_init(64);
    FolClause *c = fol_clause_new(1);
    c->lits[0] = (FolLit){ .atom = pred2(0u, v(0), v(0)), .sign = 1 };
    cnf_add_clause(s, c);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  // === paramodulation in saturation ===============================

  TEST_BEGIN("fol/sat-demod-chain");
  {
    // C1=(a=b), C2=(b=c), C3=P(a), C4=¬P(c).
    // Forward demod chains: P(a)->P(b)->P(c), then resolves with ¬P(c).
    // Same shape as the paramod-chain test, but here demod (not
    // paramod-into-paramod) should drive the simplification too.
    CnfState *s = cnf_init(256);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred2(0u, k(L_b), k(L_c)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c4 = fol_clause_new(1);
    c4->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_c)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    cnf_add_clause(s, c4);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-ordered-demod-allows-correct-orientation");
  {
    // Attach KBO with precedence putting `a` strictly above `b`.
    // (a = b): under KBO orientation a -> b is allowed; b -> a is not.
    // With ordering on, demod should still rewrite a -> b in P(a).
    static const u32 W[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    // Make `a` (L_a=10) heavier than `b` (L_b=11): precedence-based,
    // a > b.  Higher precedence value = greater under thvm.
    static const u32 P[32] = {
      0,0,0,0,0,0,0,0,0,0,  // 0..9
      9,                    // L_a = 10 -> 9
      1,                    // L_b = 11 -> 1
      0,                    // L_f = 12 -> 0
      0,                    // L_c = 13 -> 0
      0,                    // L_d = 14 -> 0
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };
    KboConfig kbo = { .weights = W, .precedence = P, .n_labels = 16, .var_weight = 1u };

    CnfState *s = cnf_init(128);
    cnf_set_kbo(s, &kbo);

    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-ordered-demod-blocks-wrong-orientation");
  {
    // Cyclic rule pair (a=b) ∧ (b=a) under KBO with precedence a > b.
    // Without an ordering: demod would loop, capped at CNF_DEMOD_BUDGET.
    // With ordering: a -> b allowed (a > b), but b -> a blocked (b < a).
    // Saturation still terminates -- the cycle doesn't fire.
    static const u32 W[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    static const u32 P[32] = {
      0,0,0,0,0,0,0,0,0,0,
      9, 1,
      0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };
    KboConfig kbo = { .weights = W, .precedence = P, .n_labels = 16, .var_weight = 1u };

    CnfState *s = cnf_init(64);
    cnf_set_kbo(s, &kbo);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred2(0u, k(L_b), k(L_a)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st != ATP_RUNNING);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-trace-records-resolution");
  {
    // C1 = P(a), C2 = ¬P(a).  After cnf_run -> PROVED, the empty
    // clause should be at some id with rule=RESOLVE and parents
    // pointing to the two input clauses.
    CnfState *s = cnf_init(64);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    i32 id1 = cnf_add_clause(s, c1);
    i32 id2 = cnf_add_clause(s, c2);
    CHECK(id1 == 0);
    CHECK(id2 == 1);
    // Input clauses' trace.
    CHECK(s->trace[0].rule == FOL_INF_INPUT);
    CHECK(s->trace[0].parent_a == FOL_NO_PARENT);
    CHECK(s->trace[1].rule == FOL_INF_INPUT);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    // The empty clause must exist somewhere with rule=RESOLVE.
    u8 found_empty = 0;
    for (u32 i = 0; i < s->n; i++) {
      if (s->clauses[i] == NULL) continue;
      if (s->clauses[i]->n_lits == 0u) {
        found_empty = 1;
        CHECK(s->trace[i].rule == FOL_INF_RESOLVE);
        // Parents are c0 and c1 in some order.
        u8 parents_ok =
          (s->trace[i].parent_a == 0u && s->trace[i].parent_b == 1u) ||
          (s->trace[i].parent_a == 1u && s->trace[i].parent_b == 0u);
        CHECK(parents_ok);
        break;
      }
    }
    CHECK(found_empty);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-print-proof-doesnt-crash");
  {
    // Smoke test: run a small refutation and print its proof DAG to
    // /dev/null.  Verifies the printer walks the trace without
    // segfaulting.
    CnfState *s = cnf_init(64);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    u32 empty_id = 0;
    for (u32 i = 0; i < s->n; i++) {
      if (s->clauses[i] != NULL && s->clauses[i]->n_lits == 0u) {
        empty_id = i;
        break;
      }
    }
    FILE *null_out = fopen("/dev/null", "w");
    CHECK(null_out != NULL);
    cnf_print_proof(s, null_out, empty_id);
    fclose(null_out);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-trace-paramod-recorded");
  {
    // (a=b), P(a), ¬P(b) -> paramod chain.  The empty clause's
    // ancestor should include a PARAMOD node.
    CnfState *s = cnf_init(128);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    // At least one derived clause has rule=PARAMOD (or rule=DEMOD if
    // demod fired first since backward demod runs on the eq).
    u8 found_eq_inference = 0;
    for (u32 i = 0; i < s->n; i++) {
      if (s->clauses[i] == NULL) continue;
      if (s->trace[i].rule == FOL_INF_PARAMOD
          || s->trace[i].rule == FOL_INF_DEMOD) {
        found_eq_inference = 1;
        break;
      }
    }
    CHECK(found_eq_inference);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-selection-negative-completes");
  {
    // Three-clause chain under CNF_SELECT_NEGATIVE.  Selection picks
    // the negative literal in C2 (¬P(x)) and C3 (¬Q(a)); C1 = P(a)
    // has no negative literal so falls back to "all literals" -- the
    // positive P(a).  Saturation proceeds the same as the no-
    // selection case for Horn-style clauses.
    CnfState *s = cnf_init(128);
    cnf_set_select(s, CNF_SELECT_NEGATIVE);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(2);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 1 };
    c2->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-selection-positive-completes");
  {
    // Same chain under CNF_SELECT_POSITIVE.
    CnfState *s = cnf_init(128);
    cnf_set_select(s, CNF_SELECT_POSITIVE);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(2);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 1 };
    c2->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-selection-falls-back-when-no-target-polarity");
  {
    // C1 = P(a) v Q(a)        (all positive)
    // C2 = ¬P(a)               (one negative)
    // Under NEGATIVE selection: C1 has no neg lit -> falls back to
    // "all literals" -> P(a) and Q(a) both eligible.  Resolution
    // P(a) + ¬P(a) gives Q(a).  No more inferences possible against
    // ¬P(a) (already used), and Q(a) has no complement -> QUEUE_EMPTY
    // (Q(a) survives as a non-refuted fact).
    CnfState *s = cnf_init(128);
    cnf_set_select(s, CNF_SELECT_NEGATIVE);
    FolClause *c1 = fol_clause_new(2);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c1->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_QUEUE_EMPTY);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-backward-demod");
  {
    // Setup that requires BACKWARD demod (not just forward).
    //   C1: P(a)        (added first; lives in active when C2 arrives)
    //   C2: ¬P(b)
    //   C3: (a = b)     (the unit eq, added last)
    //
    // Without backward demod: forward demod on a NEW derivative
    // rewrites it, but C1, C2 sit unchanged in active.  Resolution
    // P(a) + ¬P(b) doesn't apply (different atoms).  Paramod is
    // wired but might fire after multiple steps.
    //
    // With backward demod: when C3 lands as a unit positive eq, it
    // rewrites C1 -> P(b).  Then resolution P(b) + ¬P(b) closes
    // immediately -> empty clause.
    CnfState *s = cnf_init(128);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 1 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-demod-budget-doesnt-loop");
  {
    // (a=b) and (b=a): cyclic rule pair.  Without an ordering check,
    // demod could loop a -> b -> a -> ...  CNF_DEMOD_BUDGET caps the
    // chain at 16; saturation must reach a terminal status.
    CnfState *s = cnf_init(64);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred2(0u, k(L_b), k(L_a)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    // Either QUEUE_EMPTY (saturation completed) or PROVED (some
    // derived empty clause).  Not RUNNING, not infinite-loop.
    CHECK(st != ATP_RUNNING);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-paramod-into-predicate");
  {
    // C1: (a = b)         positive equality
    // C2: P(a)            atom containing `a`
    // C3: ¬P(b)           the negation of the paramodulant
    // Paramod C1 into C2 at [0]: derive P(b).  Then resolve P(b) with
    // C3 -> empty clause -> PROVED.
    CnfState *s = cnf_init(128);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-paramod-chain");
  {
    // C1: (a = b)
    // C2: (b = c)
    // C3: ¬P(c)
    // C4: P(a)
    // Chain of paramodulations: P(a) -> P(b) -> P(c) closes against C3.
    CnfState *s = cnf_init(256);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred2(0u, k(L_b), k(L_c)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_c)), .sign = 1 };
    FolClause *c4 = fol_clause_new(1);
    c4->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    cnf_add_clause(s, c4);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-backward-subsumption");
  {
    // Setup that lets backward subsumption fire on a derived clause:
    //   C1: ¬P(x) v Q(x)
    //   C2: P(a) v P(b)            (two specific instances)
    //   C3: ¬Q(a)
    //   C4: ¬Q(b)
    // Resolving C1+C2 (twice) gives Q(a) v P(b) and P(a) v Q(b).
    // Resolving these against C3, C4 chains to empty -> PROVED.  The
    // derived clauses' active set may grow large; backward subsumption
    // keeps it bounded by pruning subsumed entries.  We assert
    // PROVED -- specific subsumed-count tracking would need a stat
    // counter that isn't shipped yet.
    CnfState *s = cnf_init(256);
    FolClause *c1 = fol_clause_new(2);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 1 };
    c1->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *c2 = fol_clause_new(2);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c2->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 1 };
    FolClause *c4 = fol_clause_new(1);
    c4->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    cnf_add_clause(s, c4);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/sat-paramod-with-unification");
  {
    // C1: (f(x) = a)        equality with variable
    // C2: P(f(b))            target containing f(b)
    // C3: ¬P(a)              negation of expected paramodulant
    // σ = {x ↦ b}: paramod C1 into C2 at [0] -> P(a).  Refutes against C3.
    CnfState *s = cnf_init(256);
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(0u, pred1(L_f, v(0)), k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, pred1(L_f, k(L_b))), .sign = 0 };
    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    cnf_add_clause(s, c1);
    cnf_add_clause(s, c2);
    cnf_add_clause(s, c3);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  // === CNF preprocessing: NNF =====================================

  TEST_BEGIN("fol/nnf-atom");
  {
    // A bare predicate atom is its own NNF.
    Term p = pred1(P_P, k(L_a));
    Term n = fol_nnf(p);
    CHECK(kbo_eq(n, p));
  }

  TEST_BEGIN("fol/nnf-double-neg");
  {
    // ¬¬P(a) -> P(a).
    Term p = pred1(P_P, k(L_a));
    Term ff = fol_mk_not(fol_mk_not(p));
    Term n = fol_nnf(ff);
    CHECK(kbo_eq(n, p));
  }

  TEST_BEGIN("fol/nnf-demorgan-and");
  {
    // ¬(P ∧ Q) -> ¬P ∨ ¬Q.
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term ff = fol_mk_not(fol_mk_and(p, q));
    Term n = fol_nnf(ff);
    Term expect = fol_mk_or(fol_mk_not(p), fol_mk_not(q));
    CHECK(kbo_eq(n, expect));
  }

  TEST_BEGIN("fol/nnf-demorgan-or");
  {
    // ¬(P ∨ Q) -> ¬P ∧ ¬Q.
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term ff = fol_mk_not(fol_mk_or(p, q));
    Term n = fol_nnf(ff);
    Term expect = fol_mk_and(fol_mk_not(p), fol_mk_not(q));
    CHECK(kbo_eq(n, expect));
  }

  TEST_BEGIN("fol/nnf-imp");
  {
    // P -> Q  ==>  ¬P ∨ Q.
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term n = fol_nnf(fol_mk_imp(p, q));
    Term expect = fol_mk_or(fol_mk_not(p), q);
    CHECK(kbo_eq(n, expect));
  }

  TEST_BEGIN("fol/nnf-neg-imp");
  {
    // ¬(P -> Q)  ==>  P ∧ ¬Q.
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term n = fol_nnf(fol_mk_not(fol_mk_imp(p, q)));
    Term expect = fol_mk_and(p, fol_mk_not(q));
    CHECK(kbo_eq(n, expect));
  }

  TEST_BEGIN("fol/nnf-iff");
  {
    // P <-> Q  ==>  (¬P ∨ Q) ∧ (P ∨ ¬Q).  Equivalent to the textbook
    // form (¬P ∨ Q) ∧ (¬Q ∨ P) up to OR-argument order; the NNF
    // implementation here emits the (P ∨ ¬Q) variant.
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term n = fol_nnf(fol_mk_iff(p, q));
    Term left  = fol_mk_or(fol_mk_not(p), q);
    Term right = fol_mk_or(p, fol_mk_not(q));
    Term expect = fol_mk_and(left, right);
    CHECK(kbo_eq(n, expect));
  }

  TEST_BEGIN("fol/nnf-quantifiers");
  {
    // ¬(∀x.P(x))  ==>  ∃x.¬P(x)
    // ¬(∃x.P(x))  ==>  ∀x.¬P(x)
    Term x = v(0);
    Term px = pred1(P_P, x);
    {
      Term n = fol_nnf(fol_mk_not(fol_mk_all(x, px)));
      Term expect = fol_mk_ex(x, fol_mk_not(px));
      CHECK(kbo_eq(n, expect));
    }
    {
      Term n = fol_nnf(fol_mk_not(fol_mk_ex(x, px)));
      Term expect = fol_mk_all(x, fol_mk_not(px));
      CHECK(kbo_eq(n, expect));
    }
  }

  TEST_BEGIN("fol/nnf-nested");
  {
    // ¬(P ∧ (Q -> R))  ==>  ¬P ∨ (Q ∧ ¬R).
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_a));
    Term r = pred2(P_R, k(L_a), k(L_b));
    Term inner = fol_mk_imp(q, r);
    Term ff = fol_mk_not(fol_mk_and(p, inner));
    Term n = fol_nnf(ff);
    Term expect = fol_mk_or(fol_mk_not(p), fol_mk_and(q, fol_mk_not(r)));
    CHECK(kbo_eq(n, expect));
  }

  TEST_BEGIN("fol/nnf-is-connective");
  {
    CHECK(fol_is_connective(FOL_LAB_NOT));
    CHECK(fol_is_connective(FOL_LAB_AND));
    CHECK(fol_is_connective(FOL_LAB_OR));
    CHECK(fol_is_connective(FOL_LAB_IMP));
    CHECK(fol_is_connective(FOL_LAB_IFF));
    CHECK(fol_is_connective(FOL_LAB_ALL));
    CHECK(fol_is_connective(FOL_LAB_EX));
    CHECK(!fol_is_connective(P_P));
    CHECK(!fol_is_connective(L_a));
  }

  // === Skolemization ==============================================

  TEST_BEGIN("fol/skolem-existential-constant");
  {
    // ∃y.P(y) -- no enclosing ∀.  Skolemizes y to a 0-ary
    // Skolem constant sk_0.  Result: P(sk_0).
    fol_reset_skolem();
    Term y = v(0);
    Term f = fol_mk_ex(y, pred1(P_P, y));
    Term n = fol_skolemize(fol_nnf(f));
    // n should be: P(sk_0)  where sk_0 is a CTR with label FOL_LAB_SKOLEM_BASE
    CHECK(term_tag(n) == TAG_CTR);
    CHECK(term_ext(n) == P_P);
    CHECK(term_ctr_n(n) == 1);
    Term arg = term_ctr_at(n, 0);
    CHECK(term_tag(arg) == TAG_CTR);
    CHECK(fol_is_skolem(term_ext(arg)));
    CHECK(term_ctr_n(arg) == 0);
  }

  TEST_BEGIN("fol/skolem-existential-under-universal");
  {
    // ∀x.∃y.P(x, y) -- y becomes sk(x).
    fol_reset_skolem();
    Term x = v(0);
    Term y = v(1);
    Term f = fol_mk_all(x, fol_mk_ex(y, pred2(P_R, x, y)));
    Term n = fol_skolemize(fol_nnf(f));
    // Expected: P(x, sk(x))  -- the ∀ is dropped; ∃ replaced.
    CHECK(term_tag(n) == TAG_CTR);
    CHECK(term_ext(n) == P_R);
    CHECK(term_ctr_n(n) == 2);
    Term arg0 = term_ctr_at(n, 0);
    Term arg1 = term_ctr_at(n, 1);
    CHECK(term_tag(arg0) == TAG_FVR);
    CHECK(term_ext(arg0) == 0);                   // x
    CHECK(term_tag(arg1) == TAG_CTR);
    CHECK(fol_is_skolem(term_ext(arg1)));
    CHECK(term_ctr_n(arg1) == 1);                 // unary sk(x)
    CHECK(kbo_eq(term_ctr_at(arg1, 0), x));
  }

  TEST_BEGIN("fol/skolem-drops-forall");
  {
    // ∀x.P(x) -- no existentials.  Just drops the ∀.
    fol_reset_skolem();
    Term x = v(0);
    Term f = fol_mk_all(x, pred1(P_P, x));
    Term n = fol_skolemize(fol_nnf(f));
    Term expect = pred1(P_P, x);
    CHECK(kbo_eq(n, expect));
  }

  TEST_BEGIN("fol/skolem-fresh-labels-distinct");
  {
    // Two unrelated existentials should get distinct Skolem labels.
    fol_reset_skolem();
    Term y1 = v(0);
    Term y2 = v(1);
    Term f1 = fol_mk_ex(y1, pred1(P_P, y1));
    Term f2 = fol_mk_ex(y2, pred1(P_Q, y2));
    Term and = fol_mk_and(f1, f2);
    Term n = fol_skolemize(fol_nnf(and));
    // n = P(sk_a) ∧ Q(sk_b) with sk_a != sk_b.
    CHECK(term_ext(n) == FOL_LAB_AND);
    Term left  = term_ctr_at(n, 0);
    Term right = term_ctr_at(n, 1);
    Term sk_a = term_ctr_at(left, 0);
    Term sk_b = term_ctr_at(right, 0);
    CHECK(fol_is_skolem(term_ext(sk_a)));
    CHECK(fol_is_skolem(term_ext(sk_b)));
    CHECK(term_ext(sk_a) != term_ext(sk_b));
  }

  // === demodulation ===============================================

  TEST_BEGIN("fol/demod-atom-no-match");
  {
    // a -> b applied to P(c): no rewrite (c doesn't match a).
    Term s = k(L_a), t = k(L_b);
    Term atom = pred1(P_P, k(L_c));
    Term out = fol_demodulate_atom(s, t, atom);
    CHECK(kbo_eq(out, atom));
  }

  TEST_BEGIN("fol/demod-atom-replaces-subterm");
  {
    // a -> b applied to P(a): yields P(b).
    Term s = k(L_a), t = k(L_b);
    Term atom = pred1(P_P, k(L_a));
    Term out = fol_demodulate_atom(s, t, atom);
    Term expect = pred1(P_P, k(L_b));
    CHECK(kbo_eq(out, expect));
  }

  TEST_BEGIN("fol/demod-atom-replaces-deep");
  {
    // a -> b applied to P(f(g(a, c))): rewrites the deep `a`.
    Term s = k(L_a), t = k(L_b);
    Term inner = pred2(L_a, k(L_a), k(L_c));   // uses L_a as a binary CTR
    Term atom  = pred1(P_P, inner);            // P(a(a, c))
    Term out   = fol_demodulate_atom(s, t, atom);
    // After rewrite: a -> b matches at root of `inner`? Yes, `inner`
    // is CTR with label L_a so its label MATCHES our s = L_a-CTR
    // (also unary in s).  Wait -- s is k(L_a) which is a 0-ary CTR.
    // `inner` is a 2-ary L_a-CTR.  They have the SAME label but
    // different arities.  thvm_match checks arity equality, so they
    // don't match at the root of `inner`.  But the leaf `a` inside
    // does match.  So `inner` becomes `a(b, c)`.
    Term expect_inner = pred2(L_a, k(L_b), k(L_c));
    Term expect = pred1(P_P, expect_inner);
    CHECK(kbo_eq(out, expect));
  }

  TEST_BEGIN("fol/demod-clause-unit-eq");
  {
    // eq_clause = (a = b); target = P(a) v Q(a).
    // Result: P(b) v Q(b).
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    FolClause *tgt = fol_clause_new(2);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    tgt->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 0 };
    FolClause *r = fol_demodulate(eqc, tgt);
    CHECK(r != NULL);
    CHECK(r->n_lits == 2);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_P, k(L_b))));
    CHECK(kbo_eq(r->lits[1].atom, pred1(P_Q, k(L_b))));
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/demod-clause-no-change");
  {
    // eq_clause = (a = b); target = P(c).  No rewrite -> NULL.
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_c)), .sign = 0 };
    FolClause *r = fol_demodulate(eqc, tgt);
    CHECK(r == NULL);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
  }

  TEST_BEGIN("fol/demod-not-unit-rejects");
  {
    // eq_clause has 2 literals -- not a unit equality, rejects.
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(2);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    eqc->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *r = fol_demodulate(eqc, tgt);
    CHECK(r == NULL);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
  }

  TEST_BEGIN("fol/demod-negative-eq-rejects");
  {
    // Negative equality -- not a rewrite rule, rejects.
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 1 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *r = fol_demodulate(eqc, tgt);
    CHECK(r == NULL);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
  }

  TEST_BEGIN("fol/demod-preserves-target-vars");
  {
    // Rule: f(x) = a.  Target: P(f(b), x).
    // Rewrite f(b) -> a in literal 0.  Literal 1 has bare `x`.
    // The result must keep `x` (var id 0) consistent across both
    // literals of the result -- not split into rule-x and target-x.
    Term f_x = pred1(L_f, v(0));
    Term eq_atom = pred2(0u, f_x, k(L_a));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = eq_atom, .sign = 0 };
    FolClause *tgt = fol_clause_new(2);
    tgt->lits[0] = (FolLit){ .atom = pred2(P_R, pred1(L_f, k(L_b)), v(0)), .sign = 0 };
    tgt->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *r = fol_demodulate(eqc, tgt);
    CHECK(r != NULL);
    CHECK(r->n_lits == 2);
    // Lit 0: R(a, v0) -- f(b) rewrote to a, but v0 must stay v0.
    Term lit0 = r->lits[0].atom;
    CHECK(term_ext(lit0) == P_R);
    Term lit0_kid0 = term_ctr_at(lit0, 0);
    CHECK(kbo_eq(lit0_kid0, k(L_a)));
    Term lit0_kid1 = term_ctr_at(lit0, 1);
    CHECK(term_tag(lit0_kid1) == TAG_FVR);
    CHECK(term_ext(lit0_kid1) == 0);
    // Lit 1: Q(v0) -- unchanged, v0 stays.
    Term lit1 = r->lits[1].atom;
    Term lit1_kid0 = term_ctr_at(lit1, 0);
    CHECK(term_tag(lit1_kid0) == TAG_FVR);
    CHECK(term_ext(lit1_kid0) == 0);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/demod-with-unification");
  {
    // eq_clause = (f(x) = a).  target = P(f(b)).
    // σ = {x ↦ b}.  After rewrite: P(a).
    Term f_x = pred1(L_f, v(0));
    Term eq_atom = pred2(0u, f_x, k(L_a));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = eq_atom, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, pred1(L_f, k(L_b))), .sign = 0 };
    FolClause *r = fol_demodulate(eqc, tgt);
    CHECK(r != NULL);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_P, k(L_a))));
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(r);
  }

  // === CNF distribution ===========================================

  TEST_BEGIN("fol/distribute-atom-passthrough");
  {
    Term atom = pred1(P_P, k(L_a));
    Term d = fol_distribute(atom);
    CHECK(kbo_eq(d, atom));
  }

  TEST_BEGIN("fol/distribute-and-passthrough");
  {
    // (P ∧ Q) stays unchanged (already CNF).
    Term f = fol_mk_and(pred1(P_P, k(L_a)), pred1(P_Q, k(L_b)));
    Term d = fol_distribute(f);
    CHECK(kbo_eq(d, f));
  }

  TEST_BEGIN("fol/distribute-or-over-and");
  {
    // P ∨ (Q ∧ R)  =>  (P ∨ Q) ∧ (P ∨ R)
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term r = pred2(P_R, k(L_a), k(L_b));
    Term f = fol_mk_or(p, fol_mk_and(q, r));
    Term d = fol_distribute(f);
    Term expect = fol_mk_and(fol_mk_or(p, q), fol_mk_or(p, r));
    CHECK(kbo_eq(d, expect));
  }

  TEST_BEGIN("fol/distribute-and-over-or-left");
  {
    // (P ∧ Q) ∨ R  =>  (P ∨ R) ∧ (Q ∨ R)
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term r = pred2(P_R, k(L_a), k(L_b));
    Term f = fol_mk_or(fol_mk_and(p, q), r);
    Term d = fol_distribute(f);
    Term expect = fol_mk_and(fol_mk_or(p, r), fol_mk_or(q, r));
    CHECK(kbo_eq(d, expect));
  }

  // === clause extraction ==========================================

  TEST_BEGIN("fol/extract-unit-clause");
  {
    // Just an atom -> one clause with one positive literal.
    u32 n = 0;
    Term atom = pred1(P_P, k(L_a));
    FolClause **cs = fol_extract_clauses(atom, &n);
    CHECK(n == 1);
    CHECK(cs[0]->n_lits == 1);
    CHECK(cs[0]->lits[0].sign == 0);
    CHECK(kbo_eq(cs[0]->lits[0].atom, atom));
    fol_clause_free(cs[0]);
    free(cs);
  }

  TEST_BEGIN("fol/extract-negated-unit");
  {
    Term atom = pred1(P_P, k(L_a));
    Term neg  = fol_mk_not(atom);
    u32 n = 0;
    FolClause **cs = fol_extract_clauses(neg, &n);
    CHECK(n == 1);
    CHECK(cs[0]->n_lits == 1);
    CHECK(cs[0]->lits[0].sign == 1);
    CHECK(kbo_eq(cs[0]->lits[0].atom, atom));
    fol_clause_free(cs[0]);
    free(cs);
  }

  TEST_BEGIN("fol/extract-multi-clause");
  {
    // (P ∨ Q) ∧ (¬R ∨ S)
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term r = pred2(P_R, k(L_a), k(L_b));
    Term s_atom = pred1(P_P, k(L_c));
    Term f = fol_mk_and(fol_mk_or(p, q),
                         fol_mk_or(fol_mk_not(r), s_atom));
    u32 n = 0;
    FolClause **cs = fol_extract_clauses(f, &n);
    CHECK(n == 2);
    CHECK(cs[0]->n_lits == 2);
    CHECK(cs[1]->n_lits == 2);
    // First clause: P, Q positive.
    CHECK(cs[0]->lits[0].sign == 0);
    CHECK(cs[0]->lits[1].sign == 0);
    // Second clause: ¬R, S.
    CHECK(cs[1]->lits[0].sign == 1);
    CHECK(cs[1]->lits[1].sign == 0);
    fol_clause_free(cs[0]);
    fol_clause_free(cs[1]);
    free(cs);
  }

  // === Tseitin structural CNF ====================================

  TEST_BEGIN("fol/tseitin-atom-single-clause");
  {
    // Atom F: just emits the unit clause { F }.
    Term p = pred1(P_P, k(L_a));
    u32 n = 0;
    FolClause **cs = fol_tseitin_extract_clauses(p, &n);
    CHECK(n == 1);
    CHECK(cs[0]->n_lits == 1);
    CHECK(cs[0]->lits[0].sign == 0);
    CHECK(kbo_eq(cs[0]->lits[0].atom, p));
    fol_clause_free(cs[0]);
    free(cs);
  }

  TEST_BEGIN("fol/tseitin-and-emits-three-encoding-plus-unit");
  {
    // P ∧ Q: Tseitin emits
    //   aux ↔ P ∧ Q:  3 clauses
    //   + unit aux
    // = 4 clauses total.  Naïve distribute would emit { P }, { Q }
    // i.e. 2 clauses; Tseitin is BIGGER for this trivial case but
    // the asymptotic win shows on deep formulas.
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term f = fol_mk_and(p, q);
    u32 n = 0;
    FolClause **cs = fol_tseitin_extract_clauses(f, &n);
    CHECK(n == 4);
    for (u32 i = 0; i < n; i++) fol_clause_free(cs[i]);
    free(cs);
  }

  TEST_BEGIN("fol/tseitin-or-emits-three-encoding-plus-unit");
  {
    // P ∨ Q: similar shape, 4 clauses total.
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term f = fol_mk_or(p, q);
    u32 n = 0;
    FolClause **cs = fol_tseitin_extract_clauses(f, &n);
    CHECK(n == 4);
    for (u32 i = 0; i < n; i++) fol_clause_free(cs[i]);
    free(cs);
  }

  TEST_BEGIN("fol/tseitin-aux-labels-are-recognized");
  {
    // After fol_tseitin_extract_clauses runs, each aux's label
    // should pass fol_is_tseitin_aux().  And NOT pass fol_is_skolem
    // (they're disjoint ranges).
    Term p = pred1(P_P, k(L_a));
    Term q = pred1(P_Q, k(L_b));
    Term f = fol_mk_and(p, q);
    u32 n = 0;
    FolClause **cs = fol_tseitin_extract_clauses(f, &n);
    u8 found_aux = 0;
    for (u32 i = 0; i < n; i++) {
      for (u32 j = 0; j < cs[i]->n_lits; j++) {
        u32 lab = term_ext(cs[i]->lits[j].atom);
        if (fol_is_tseitin_aux(lab)) {
          found_aux = 1;
          CHECK(!fol_is_skolem(lab));
        }
      }
    }
    CHECK(found_aux);
    for (u32 i = 0; i < n; i++) fol_clause_free(cs[i]);
    free(cs);
  }

  TEST_BEGIN("fol/tseitin-e2e-smoke");
  {
    // Smallest end-to-end test: P ∧ ¬P (contradiction) -> empty.
    // Via Tseitin: aux ↔ (P ∧ ¬P) yields the contradiction encoding;
    // saturation should reach UNSAT.
    Term p = pred1(P_P, k(L_a));
    Term conj = fol_mk_and(p, fol_mk_not(p));    // unsatisfiable
    u32 nc = 0;
    FolClause **clauses = fol_formula_to_clauses_tseitin(conj, &nc);
    CHECK(nc > 0);
    CnfState *s = cnf_init(1024);
    for (u32 i = 0; i < nc; i++) cnf_add_clause(s, clauses[i]);
    free(clauses);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  // === end-to-end formula -> clauses -> PROVED ====================

  TEST_BEGIN("fol/e2e-modus-ponens");
  {
    // ∀x. P(x) -> Q(x)
    // P(a)
    // ¬Q(a)
    // Expect: empty clause derivable via cnf_run.
    Term x = v(0);
    Term px = pred1(P_P, x);
    Term qx = pred1(P_Q, x);
    Term formula = fol_mk_all(x, fol_mk_imp(px, qx));

    u32 nc = 0;
    FolClause **clauses = fol_formula_to_clauses(formula, &nc);
    CHECK(nc == 1);
    // The single clause is ¬P(x) ∨ Q(x).
    CHECK(clauses[0]->n_lits == 2);

    CnfState *s = cnf_init(128);
    cnf_add_clause(s, clauses[0]);
    free(clauses);

    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    cnf_add_clause(s, c2);

    FolClause *c3 = fol_clause_new(1);
    c3->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 1 };
    cnf_add_clause(s, c3);

    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/e2e-existential-witness");
  {
    // ∃x.P(x)   (one positive existential, no other axioms).
    // After Skolemization: P(sk_0).  Saturation: one clause, no
    // resolution possible -> QUEUE_EMPTY (satisfiable).
    Term y = v(0);
    Term formula = fol_mk_ex(y, pred1(P_P, y));
    u32 nc = 0;
    FolClause **clauses = fol_formula_to_clauses(formula, &nc);
    CHECK(nc == 1);
    CHECK(clauses[0]->n_lits == 1);
    // The atom should be P(sk_0) where sk_0 is a fresh Skolem.
    Term arg = term_ctr_at(clauses[0]->lits[0].atom, 0);
    CHECK(fol_is_skolem(term_ext(arg)));

    CnfState *s = cnf_init(64);
    cnf_add_clause(s, clauses[0]);
    free(clauses);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_QUEUE_EMPTY);
    cnf_free(s);
  }

  TEST_BEGIN("fol/e2e-russell-style");
  {
    // The "barber" / Russell-style: ¬(∃x.∀y. shaves(x, y) <-> ¬shaves(y, y)).
    // The negation is the test conjecture; if we drop the negation
    // and produce clauses, saturation should derive the contradiction.
    // We craft it directly as the clause-form refutation target:
    //
    // ∃x.∀y.(shaves(x, y) <-> ¬shaves(y, y))
    // ==> Skolemize x -> b (the barber).
    // ∀y. shaves(b, y) <-> ¬shaves(y, y)
    // <-> expansion produces two clauses each direction.
    //
    // For simplicity: prove that ∃y.(¬shaves(y, y)) implies that we
    // have a witness whose shaves(b, witness) leads to contradiction
    // with both polarities.
    //
    // Direct refutation: assume shaves(b, b) <-> ¬shaves(b, b).
    // After NNF + skolem + CNF: yields the empty clause via
    // resolution.
    Term b = v(0);                                  // barber
    Term shaves_bb = pred2(P_R, b, b);              // shaves(b, b)
    Term phi = fol_mk_iff(shaves_bb, fol_mk_not(shaves_bb));
    // Wrap to make x_0 universal (so saturation works post-Skolem).
    Term f = fol_mk_all(b, phi);
    u32 nc = 0;
    FolClause **clauses = fol_formula_to_clauses(f, &nc);
    CnfState *s = cnf_init(256);
    for (u32 i = 0; i < nc; i++) cnf_add_clause(s, clauses[i]);
    free(clauses);
    AtpStatus st = cnf_run(s);
    // Russell paradox: this should yield the empty clause.
    CHECK(st == ATP_PROVED);
    cnf_free(s);
  }

  TEST_BEGIN("fol/skolem-nested-quantifier-scope");
  {
    // ∀x.∀y.∃z.R(x, y, z): z becomes sk(x, y).
    // (Need 3-arg predicate; use existing labels P_P / etc. for a
    //  simpler test: ∀x.∃z.(P(x) ∧ Q(z)).)
    fol_reset_skolem();
    Term x = v(0);
    Term z = v(1);
    Term inner = fol_mk_and(pred1(P_P, x), pred1(P_Q, z));
    Term f = fol_mk_all(x, fol_mk_ex(z, inner));
    Term n = fol_skolemize(fol_nnf(f));
    // Result: P(x) ∧ Q(sk(x)).
    CHECK(term_ext(n) == FOL_LAB_AND);
    Term q_atom = term_ctr_at(n, 1);
    CHECK(term_ext(q_atom) == P_Q);
    Term sk = term_ctr_at(q_atom, 0);
    CHECK(fol_is_skolem(term_ext(sk)));
    CHECK(term_ctr_n(sk) == 1);
    CHECK(kbo_eq(term_ctr_at(sk, 0), x));
  }

  TEST_BEGIN("fol/wide-clause-chunks-into-narrow-pieces");
  {
    // R(x, c0) | ... | R(x, c69): one variable-connected component past
    // FOL_MAX_LITS.  It chunks into pieces of at most 8 literals linked by
    // aux atoms over x, every original literal kept, and the pieces
    // refute against the 70 units ~R(a, ci).
    fol_reset_tseitin();
    const u32 W = 70u;
    Term x = v(0);
    Term f = pred2(P_R, x, k(100u));
    for (u32 i = 1; i < W; i++) f = fol_mk_or(f, pred2(P_R, x, k(100u + i)));
    u32 n = 0;
    FolClause **cs = fol_extract_clauses(f, &n);
    CHECK(n > 1u);
    u32 orig = 0u, aux_ok = 1u, narrow = 1u;
    for (u32 c = 0; c < n; c++) {
      if (cs[c]->n_lits > 8u) narrow = 0u;
      for (u32 l = 0; l < cs[c]->n_lits; l++) {
        Term a = cs[c]->lits[l].atom;
        if (term_ext(a) == P_R) orig++;
        else if (!fol_is_tseitin_aux(term_ext(a)) || term_ctr_n(a) != 1u) aux_ok = 0u;
      }
    }
    CHECK(narrow);
    CHECK(aux_ok);
    CHECK_EQ(orig, W);
    CnfState *st = cnf_init(1u << 20);
    for (u32 c = 0; c < n; c++) cnf_add_clause(st, cs[c]);
    free(cs);
    for (u32 i = 0; i < W; i++) {
      FolClause *u = fol_clause_new(1);
      u->lits[0] = (FolLit){ .atom = pred2(P_R, k(L_a), k(100u + i)), .sign = 1 };
      cnf_add_clause(st, u);
    }
    CHECK(cnf_run(st) == ATP_PROVED);
    cnf_free(st);
  }

  TEST_BEGIN("fol/wide-clause-names-disjoint-components");
  {
    // ~P0(x0) | ... | ~P84(x84), no variable shared: the negated
    // conjecture ?[X]:P0(X) & ... & ?[X]:P84(X).  Each literal is named
    // by a propositional atom (P_i(x) -> d_i, a one-condition Horn rule)
    // and the goal over the d_i chunks into Horn pieces of at most 8
    // literals: no clause gains a second positive literal.  With the 85
    // facts P_i(ci) the set refutes.
    fol_reset_tseitin();
    const u32 W = 85u;
    Term f = fol_mk_not(pred1(200u, v(0)));
    for (u32 i = 1; i < W; i++) f = fol_mk_or(f, fol_mk_not(pred1(200u + i, v(i))));
    u32 n = 0;
    FolClause **cs = fol_extract_clauses(f, &n);
    u32 named = 0u, narrow = 1u, horn = 1u;
    for (u32 c = 0; c < n; c++) {
      if (cs[c]->n_lits > 8u) narrow = 0u;
      u32 pos = 0u, orig = 0u;
      for (u32 l = 0; l < cs[c]->n_lits; l++) {
        Term a = cs[c]->lits[l].atom;
        if (cs[c]->lits[l].sign == 0u) pos++;
        if (!fol_is_tseitin_aux(term_ext(a))) orig++;
        else if (term_ctr_n(a) != 0u) horn = 0u;   // names are propositional
      }
      if (pos > 1u) horn = 0u;
      if (orig > 0u) {
        CHECK_EQ(orig, 1u);
        CHECK_EQ(cs[c]->n_lits, 2u);
        named++;
      }
    }
    CHECK(narrow);
    CHECK(horn);
    CHECK_EQ(named, W);
    CnfState *st = cnf_init(1u << 20);
    for (u32 c = 0; c < n; c++) cnf_add_clause(st, cs[c]);
    free(cs);
    for (u32 i = 0; i < W; i++) {
      FolClause *u = fol_clause_new(1);
      u->lits[0] = (FolLit){ .atom = pred1(200u + i, k(300u + i)), .sign = 0 };
      cnf_add_clause(st, u);
    }
    CHECK(cnf_run(st) == ATP_PROVED);
    cnf_free(st);
    // Satisfiable without the fact for P84: the names add no refutation.
    fol_reset_tseitin();
    u32 n2 = 0;
    FolClause **cs2 = fol_extract_clauses(f, &n2);
    CnfState *st2 = cnf_init(1u << 20);
    for (u32 c = 0; c < n2; c++) cnf_add_clause(st2, cs2[c]);
    free(cs2);
    for (u32 i = 0; i + 1u < W; i++) {
      FolClause *u = fol_clause_new(1);
      u->lits[0] = (FolLit){ .atom = pred1(200u + i, k(300u + i)), .sign = 0 };
      cnf_add_clause(st2, u);
    }
    CHECK(cnf_run(st2) == ATP_QUEUE_EMPTY);
    cnf_free(st2);
  }

  TEST_BEGIN("fol/distribute-builds-each-clause-once");
  {
    // (a1 & ... & a60) | (b1 & ... & b60): 3600 clauses ai | bj,
    // a-major, and the heap grows by the result alone -- distributing a
    // joined side again per conjunct of the other allocated ~N^3 nodes.
    const u32 N = 60u;
    Term A = pred1(P_P, k(1000u)), B = pred1(P_Q, k(2000u));
    for (u32 i = 1; i < N; i++) {
      A = fol_mk_and(A, pred1(P_P, k(1000u + i)));
      B = fol_mk_and(B, pred1(P_Q, k(2000u + i)));
    }
    Term f = fol_mk_or(A, B);
    u64 h0 = CURRENT_CTX->heap_next;
    Term d = fol_distribute(f);
    u64 used = CURRENT_CTX->heap_next - h0;
    CHECK(used < 16u * (u64)N * N);
    u32 n = 0;
    FolClause **cs = fol_extract_clauses(d, &n);
    CHECK_EQ(n, N * N);
    u32 ok = (cs != NULL);
    for (u32 c = 0; ok && c < n; c++) {
      ok = cs[c]->n_lits == 2u &&
           kbo_eq(cs[c]->lits[0].atom, pred1(P_P, k(1000u + c / N))) &&
           kbo_eq(cs[c]->lits[1].atom, pred1(P_Q, k(2000u + c % N)));
    }
    CHECK(ok);
    for (u32 c = 0; c < n; c++) fol_clause_free(cs[c]);
    free(cs);
    CHECK(kbo_eq(d, dist_ref(f)));
    // Random NNF formulas: the same terms as the old distribution.
    u32 s = 20260929u, same = 1u;
    for (u32 t = 0; t < 300u && same; t++) {
      Term g = rand_nnf(&s, 6u);
      same = kbo_eq(fol_distribute(g), dist_ref(g));
    }
    CHECK(same);
  }

  TEST_BEGIN("fol/walkers-match-recursive-oracles");
  {
    // Random formulas over every connective, shadowing binders
    // included: NNF terms; Skolem terms and label counts (from raw and
    // from NNF input); distributed terms and their clauses, in order;
    // Tseitin clauses and name counts.
    u32 s = 20260930u;
    int nnf_ok = 1, sk_ok = 1, dist_ok = 1, ts_ok = 1, n_dist = 0;
    for (u32 t = 0; t < 600u; t++) {
      Term f = rand_fol(&s, 2u + t % 7u, (t % 3u == 0u) ? 12u : 4u);
      Term n1 = fol_nnf(f);
      nnf_ok &= teq(n1, nnf_ref(f, 0u));
      for (int raw = 0; raw < 2; raw++) {
        Term in = raw ? f : n1;
        fol_reset_skolem();
        Term s1 = fol_skolemize(in);
        u32 c1 = g_fol_skolem_next;
        fol_reset_skolem();
        Term s0 = sk_ref(in, 0u);
        sk_ok &= teq(s1, s0) && c1 == g_fol_skolem_next;
      }
      fol_reset_skolem();
      Term sk = fol_skolemize(n1);
      if (dist_ok && est_ref(sk) <= 2000u) {
        Term d1 = fol_distribute(sk);
        Term d0 = distribute_ref(sk);
        dist_ok &= teq(d1, d0);
        u32 m1 = 0u, m0 = 0u, cap0 = 0u;
        fol_reset_tseitin();
        FolClause **x1 = fol_extract_clauses(d1, &m1);
        fol_reset_tseitin();
        FolClause **x0 = NULL;
        extract_and_ref(d0, &x0, &m0, &cap0);
        dist_ok &= clauses_eq(x1, m1, x0, m0);
        clauses_free(x1, m1);
        clauses_free(x0, m0);
        n_dist++;
      }
      u32 m1 = 0u, m0 = 0u, cap0 = 0u;
      fol_reset_tseitin();
      FolClause **t1 = fol_tseitin_extract_clauses(sk, &m1);
      u32 a1 = g_fol_tseitin_next;
      fol_reset_tseitin();
      FolClause **t0 = NULL;
      FolLit root = tseitin_ref(sk, &t0, &m0, &cap0);
      fol_tseitin_emit(&t0, &m0, &cap0, &root, 1u);
      ts_ok &= clauses_eq(t1, m1, t0, m0) && a1 == g_fol_tseitin_next;
      clauses_free(t1, m1);
      clauses_free(t0, m0);
    }
    CHECK(nnf_ok);
    CHECK(sk_ok);
    CHECK(dist_ok);
    CHECK(ts_ok);
    CHECK(n_dist > 200);
    // Wide universal scopes (past REWRITE_MAX_ARITY universals): the
    // Skolem term keeps the universals its body mentions, an inner one
    // included.
    int wide_ok = 1;
    for (u32 w = 15u; w < 24u; w++) {
      Term inner = fol_mk_ex(v(41u), fol_mk_and(pred2(P_R, v(41u), v(40u)),
                                                pred2(P_R, v(w - 3u), v(41u))));
      Term f = fol_mk_ex(v(40u), fol_mk_and(pred2(P_R, v(40u), v(w % 5u)), inner));
      for (u32 i = w; i-- > 0u;) f = fol_mk_all(v(i), f);
      fol_reset_skolem();
      Term s1 = fol_skolemize(f);
      fol_reset_skolem();
      wide_ok &= teq(s1, sk_ref(f, 0u)) && g_fol_skolem_next == 2u;
    }
    CHECK(wide_ok);
    // Quantifier blocks: existential runs Skolemized at once must give
    // what one binder at a time gives (shadowing and capture included),
    // under narrow and wide universal scopes.
    int run_ok = 1;
    for (u32 t = 0; t < 1500u; t++) {
      Term f = rand_prefixed(&s, (t % 2u) ? 5u : 9u, (t % 5u == 0u) ? 17u : t % 4u);
      for (int raw = 0; raw < 2; raw++) {
        Term in = raw ? f : fol_nnf(f);
        fol_reset_skolem();
        Term s1 = fol_skolemize(in);
        u32 c1 = g_fol_skolem_next;
        fol_reset_skolem();
        Term s0 = sk_ref(in, 0u);
        run_ok &= teq(s1, s0) && c1 == g_fol_skolem_next;
      }
    }
    CHECK(run_ok);
    // A 30k-binder existential prefix over a formula mentioning every
    // variable: one substitution walk, each variable its own Skolem
    // constant.
    {
      const u32 K = 30000u;
      Term body = pred1(P_P, v(0u));
      for (u32 i = 1; i < K; i++) body = fol_mk_and(body, pred1(P_P, v(i)));
      Term f = body;
      for (u32 i = K; i-- > 0u;) f = fol_mk_ex(v(i), f);
      fol_reset_skolem();
      Term sk = fol_skolemize(f);
      CHECK_EQ(g_fol_skolem_next, K);
      int consts = 1;
      Term spine = sk;
      for (u32 i = K - 1u; consts && i > 0u; i--) {
        Term a = term_ctr_at(spine, 1u);
        Term c = term_ctr_at(a, 0u);
        consts = term_ext(spine) == FOL_LAB_AND && term_ext(a) == P_P &&
                 term_ext(c) == FOL_LAB_SKOLEM_BASE + i && term_ctr_n(c) == 0u;
        spine = term_ctr_at(spine, 0u);
      }
      CHECK(consts);
    }
  }

  TEST_BEGIN("fol/walkers-take-any-depth");
  {
    // A 200k-conjunct chain, left-nested as the TPTP parser builds it,
    // under a universal and an existential binder: past any C stack
    // (HWV061+1 is 117k levels; one frame per level overflowed 8 MB in
    // fol_nnf_rec).
    const u32 N = 200000u;
    Term chain = pred2(P_R, v(0u), k(L_a));
    for (u32 i = 1; i < N; i++) {
      chain = fol_mk_and(chain, pred2(P_R, v(i % 2u), k(1000u + i % 5000u)));
    }
    Term f = fol_mk_all(v(0u), fol_mk_ex(v(1u), chain));
    CHECK_EQ(fol_depth(f, 1000000u), N + 3u);
    CHECK_EQ(fol_depth(f, 100u), 100u);
    CHECK(teq(fol_nnf(fol_mk_not(fol_mk_not(f))), f));
    // ~f: ?[x0] ![x1] of an OR chain of negated atoms, left-nested.
    Term g = fol_nnf(fol_mk_not(f));
    int shape = term_ext(g) == FOL_LAB_EX && term_ext(term_ctr_at(g, 1u)) == FOL_LAB_ALL;
    Term spine = term_ctr_at(term_ctr_at(g, 1u), 1u);
    for (u32 i = N - 1u; shape && i > 0u; i--) {
      shape = term_ext(spine) == FOL_LAB_OR &&
              term_ext(term_ctr_at(spine, 1u)) == FOL_LAB_NOT;
      spine = term_ctr_at(spine, 0u);
    }
    CHECK(shape && term_ext(spine) == FOL_LAB_NOT);
    // Skolemized, distributed, extracted: N unit clauses in chain order,
    // x1 read as sk0(x0).
    fol_reset_skolem();
    Term sk = fol_skolemize(fol_nnf(f));
    CHECK_EQ(g_fol_skolem_next, 1u);
    Term sk0 = term_new_ctr(FOL_LAB_SKOLEM_BASE, (Term[]){ v(0u) }, 1u);
    u32 m = 0u;
    FolClause **cs = fol_extract_clauses(fol_distribute(sk), &m);
    CHECK_EQ(m, N);
    int units = (cs != NULL && m == N);
    for (u32 i = 0; units && i < N; i++) {
      Term want = (i == 0u) ? pred2(P_R, v(0u), k(L_a))
                            : pred2(P_R, (i % 2u) ? sk0 : v(0u), k(1000u + i % 5000u));
      units = cs[i]->n_lits == 1u && cs[i]->lits[0].sign == 0u &&
              teq(cs[i]->lits[0].atom, want);
    }
    CHECK(units);
    clauses_free(cs, m);
    // Tseitin over a 12k-deep disjunction chain: one name per compound,
    // the recursive oracle's clauses exactly (the oracle's own frames
    // bound the depth; every compound's name collects its subformula's
    // variables, quadratic in the chain).
    const u32 M = 12000u;
    Term ors = pred1(P_P, k(L_a));
    for (u32 i = 1; i < M; i++) ors = fol_mk_or(ors, pred1(P_Q, k(1000u + i)));
    u32 m1 = 0u, m0 = 0u, cap0 = 0u;
    fol_reset_tseitin();
    FolClause **t1 = fol_tseitin_extract_clauses(ors, &m1);
    CHECK_EQ(g_fol_tseitin_next, M - 1u);
    CHECK_EQ(m1, 3u * (M - 1u) + 1u);
    fol_reset_tseitin();
    FolClause **t0 = NULL;
    FolLit root = tseitin_ref(ors, &t0, &m0, &cap0);
    fol_tseitin_emit(&t0, &m0, &cap0, &root, 1u);
    CHECK(clauses_eq(t1, m1, t0, m0));
    clauses_free(t1, m1);
    clauses_free(t0, m0);
  }

  thvm_free();
  TEST_REPORT();
}
