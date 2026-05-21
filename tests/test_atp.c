// test_atp.c - AtpState construction (stage 5.1).
//
// Init/free/add_equation/set_goal only.  The saturation step lands
// in 5.2 and gets its own broader test.

#include "../src/thvm.c"
#include "test.h"

#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_i(Term x)         { Term cs[1] = {x};    return term_new_ctr(LAB_i, cs, 1); }

static u32 dummy_weights   [5] = {0, 1, 0, 1, 1};
static u32 dummy_precedence[5] = {0, 2, 4, 3, 1};
static const KboConfig DUMMY_CFG = {
  .weights     = dummy_weights,
  .precedence  = dummy_precedence,
  .n_labels    = 5,
  .var_weight  = 1,
};

// Test wrapper for atp_cp_queue_subsumed.  These tests populate
// the queue (thvm_atp_cp_set) / s->n_cps directly, so with -DATP_CP_GRAPH the
// IC-native cp_graph is stale until thvm_atp_cp_reheapify resyncs
// it -- 8e routes queue subsumption through cp_graph (one
// thvm_match_multi traversal), so the resync must happen before the
// check.  7d's -DATP_FV_INDEX is the same story: the FV index is
// maintained incrementally on heap push/pop, so a test that pokes
// the arrays directly must reheapify to rebuild it.  Off both flags
// this is a thin pass-through to the array scan.
static int tt_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
#if defined(ATP_CP_GRAPH) || defined(ATP_FV_INDEX)
  thvm_atp_cp_reheapify(s);
#endif
  return (int)atp_cp_queue_subsumed(s, lhs, rhs);
}

// 7c: a stored rule / CP equals its input term-for-term off
// -DATP_VAR_NORM, but ON the storage path canonically renumbers the
// (lhs, rhs) pair -- it is a freshly-rebuilt, alpha-renamed term, so
// raw Term-pointer equality no longer holds.  `tt_norm_lhs` /
// `tt_norm_rhs` return the side of the EXPECTED (lhs, rhs) pair as
// the storage path would have stored it: off the flag the input
// term unchanged, on the flag the canonically renumbered side.  The
// caller then asserts structural equality (kbo_eq) against the
// stored term -- an alpha-equivalence check, one assertion per side
// (so the assertion count is identical with the flag on or off).
static Term tt_norm_lhs(Term exp_l, Term exp_r) {
#ifdef ATP_VAR_NORM
  thvm_normalize_vars(&exp_l, &exp_r);
#endif
  (void)exp_r;
  return exp_l;
}
static Term tt_norm_rhs(Term exp_l, Term exp_r) {
#ifdef ATP_VAR_NORM
  thvm_normalize_vars(&exp_l, &exp_r);
#endif
  (void)exp_l;
  return exp_r;
}

#ifdef ATP_CP_GROUND_JOIN
// Differential soundness cross-check for a DELETE verdict.  When the
// symbolic ground-join test says DELETE, we INDEPENDENTLY verify that a
// large sample of concrete GROUND instances (variables -> fresh distinct
// minimal constants ordered by a randomly sampled total preorder) really
// do join under ordinary ground-KBO rewriting.  A failure here would mean
// the symbolic test deleted a CP that has a non-joinable ground instance
// -- a soundness bug -- so the test fails loudly.  Returns 1 iff every
// sampled instance joins.  This uses the real thvm_kbo on an EXTENDED
// config (fresh constants placed minimal & ordered), an oracle distinct
// from the symbolic >=_C path it is checking.
static u32 gjt_rng = 0x1234567u;
static u32 gjt_rand(void) {
  gjt_rng = gjt_rng * 1664525u + 1013904223u;
  return (gjt_rng >> 8) & 0x7fffffu;
}
static void gjt_collect_vars(Term t, u32 *ids, u32 *n) {
  if (term_tag(t) == TAG_FVR) {
    for (u32 i = 0; i < *n; i++) if (ids[i] == term_ext(t)) return;
    ids[(*n)++] = term_ext(t);
  } else if (term_tag(t) == TAG_CTR) {
    u32 k = term_ctr_n(t);
    for (u32 i = 0; i < k; i++) gjt_collect_vars(term_ctr_at(t, i), ids, n);
  }
}
static u8 gjt_gfired;
static Term gjt_gstep(AtpState *s, Term u, const KboConfig *ec) {
  for (u32 i = 0; i < s->n_rules; i++) {
    if (term_tag(s->lhs[i]) == TAG_FVR) continue;
    RewriteSubst sub = {{0}};
    if (thvm_match(s->lhs[i], u, &sub)) {
      Term rd = thvm_subst_apply(s->rhs[i], &sub);
      if (thvm_kbo(u, rd, ec) == KBO_GT) { gjt_gfired = 1; return rd; }
    }
  }
  if (term_tag(u) == TAG_CTR) {
    u32 n = term_ctr_n(u);
    if (n > REWRITE_MAX_ARITY) return u;
    Term ch[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) ch[i] = term_ctr_at(u, i);
    for (u32 i = 0; i < n; i++) {
      Term r = gjt_gstep(s, ch[i], ec);
      if (gjt_gfired) { ch[i] = r; return term_new_ctr(term_ext(u), ch, n); }
    }
  }
  return u;
}
static Term gjt_gnorm(AtpState *s, Term t, const KboConfig *ec) {
  for (u32 k = 0; k < 512u; k++) {
    gjt_gfired = 0;
    Term t2 = gjt_gstep(s, t, ec);
    if (!gjt_gfired) return t;
    t = t2;
  }
  return t;
}
static int gjt_differential(AtpState *s, Term l, Term r,
                            u32 base_labels, u32 trials) {
  u32 vids[8]; u32 nv = 0;
  gjt_collect_vars(l, vids, &nv);
  gjt_collect_vars(r, vids, &nv);
  for (u32 t = 0; t < trials; t++) {
    u32 cls[8]; u32 ncls = 0;
    for (u32 i = 0; i < nv; i++) {
      cls[i] = gjt_rand() % (i + 1u);
      if (cls[i] + 1u > ncls) ncls = cls[i] + 1u;
    }
    u32 perm[8];
    for (u32 i = 0; i < ncls; i++) perm[i] = i;
    for (u32 i = ncls; i > 1u; i--) {
      u32 j = gjt_rand() % i;
      u32 tmp = perm[i - 1u]; perm[i - 1u] = perm[j]; perm[j] = tmp;
    }
    u32 nl = base_labels + ncls;
    u32 ew[24], ep[24];
    for (u32 L = 0; L < base_labels; L++) {
      ew[L] = s->kbo->weights[L];
      ep[L] = s->kbo->precedence[L] + ncls;
    }
    for (u32 c = 0; c < ncls; c++) {
      ew[base_labels + c] = s->kbo->var_weight;
      ep[base_labels + c] = c;
    }
    KboConfig ec = {
      .weights = ew, .precedence = ep, .n_labels = nl,
      .var_weight = s->kbo->var_weight,
    };
    RewriteSubst sub = {{0}};
    for (u32 i = 0; i < nv; i++)
      sub.bindings[vids[i]] = term_new_ctr(base_labels + perm[cls[i]], NULL, 0);
    Term gl = thvm_subst_apply(l, &sub);
    Term gr = thvm_subst_apply(r, &sub);
    Term n1 = gjt_gnorm(s, gl, &ec);
    Term n2 = gjt_gnorm(s, gr, &ec);
    if (!kbo_eq(n1, n2)) return 0;   // a ground instance did NOT join
  }
  return 1;
}
#endif  // ATP_CP_GROUND_JOIN

int main(void) {
  thvm_init();

  TEST_BEGIN("atp/init-and-free");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK(s != NULL);
    CHECK(s->kbo == &DUMMY_CFG);
    CHECK_EQ(s->step_cap,     100u);
    CHECK_EQ(s->n_rules,        0u);
    CHECK_EQ(s->n_cps,          0u);
    CHECK_EQ(s->goal_lhs,       0u);
    CHECK_EQ(s->goal_rhs,       0u);
    CHECK_EQ(s->step,           0u);
    CHECK_EQ(s->n_trace,        0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-push-axiom-decodes");
  {
    // atp_trace_push is static; #include of thvm.c brings it into
    // scope for the test TU.  Decode the resulting TAG_CTR to
    // verify the [NUM(p_a), NUM(p_b), lhs, rhs] layout + reason
    // label.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    u32 idx = atp_trace_push(s, TRACE_AXIOM,
                             ATP_TRACE_NONE, ATP_TRACE_NONE,
                             lhs, rhs);
    CHECK_EQ(idx,         0u);
    CHECK_EQ(s->n_trace,  1u);
    Term entry = s->trace[0];
    CHECK_EQ(term_tag(entry),    TAG_CTR);
    CHECK_EQ(term_ext(entry),    TRACE_AXIOM);
    CHECK_EQ(term_ctr_n(entry),  4u);
    Term p_a = term_ctr_at(entry, 0);
    Term p_b = term_ctr_at(entry, 1);
    CHECK_EQ(term_tag(p_a), TAG_NUM);
    CHECK_EQ(term_val(p_a), ATP_TRACE_NONE);
    CHECK_EQ(term_tag(p_b), TAG_NUM);
    CHECK_EQ(term_val(p_b), ATP_TRACE_NONE);
    CHECK_EQ(term_ctr_at(entry, 2), lhs);
    CHECK_EQ(term_ctr_at(entry, 3), rhs);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-push-orient-with-parent");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_e();
    Term rhs = mk_e();
    u32 axiom_idx = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   lhs, rhs);
    u32 orient_idx = atp_trace_push(s, TRACE_ORIENT,
                                    axiom_idx, ATP_TRACE_NONE,
                                    lhs, rhs);
    CHECK_EQ(orient_idx, 1u);
    CHECK_EQ(s->n_trace, 2u);
    Term entry = s->trace[orient_idx];
    CHECK_EQ(term_ext(entry), TRACE_ORIENT);
    CHECK_EQ(term_val(term_ctr_at(entry, 0)), axiom_idx);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-push-rejects-when-full");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    // Fill up to ATP_MAX_TRACE entries.
    Term lhs = mk_e(), rhs = mk_e();
    for (u32 i = 0; i < ATP_MAX_TRACE; i++) {
      u32 idx = atp_trace_push(s, TRACE_AXIOM,
                               ATP_TRACE_NONE, ATP_TRACE_NONE,
                               lhs, rhs);
      CHECK_EQ(idx, i);
    }
    CHECK_EQ(s->n_trace, (u64)ATP_MAX_TRACE);
    // One more should yield ATP_TRACE_NONE.
    u32 ovf = atp_trace_push(s, TRACE_AXIOM,
                             ATP_TRACE_NONE, ATP_TRACE_NONE,
                             lhs, rhs);
    CHECK_EQ(ovf, ATP_TRACE_NONE);
    CHECK_EQ(s->n_trace, (u64)ATP_MAX_TRACE);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/free-null-is-safe");
  {
    thvm_atp_free(NULL);   // no crash, no-op
  }

  TEST_BEGIN("atp/add-equation-pushes-to-queue");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    CHECK(thvm_atp_add_equation(s, lhs, rhs));
    CHECK_EQ(s->n_cps,      1u);
    // 7c: under -DATP_VAR_NORM the queued CP is the canonically
    // renumbered (alpha-renamed) input -- compare up to that.
    Term q_lhs = 0, q_rhs = 0;
    thvm_atp_cp_get(s, 0, &q_lhs, &q_rhs);
    CHECK(kbo_eq(q_lhs, tt_norm_lhs(lhs, rhs)));
    CHECK(kbo_eq(q_rhs, tt_norm_rhs(lhs, rhs)));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/add-equation-queue-grows-past-initial-cap");
  {
    // 7a: the CP queue is growable -- add_equation never rejects
    // for being full.  Push well past ATP_INIT_CPS and confirm the
    // queue keeps every entry (capacity doubled on demand).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_e();
    Term rhs = mk_e();
    u32 n_push = ATP_INIT_CPS + 17u;
    for (u32 i = 0; i < n_push; i++) {
      CHECK(thvm_atp_add_equation(s, lhs, rhs));
    }
    CHECK_EQ(s->n_cps, (u64)n_push);
    CHECK(s->cp_cap >= n_push);
    // One more still succeeds -- no ceiling.
    CHECK_EQ(thvm_atp_add_equation(s, lhs, rhs), 1u);
    CHECK_EQ(s->n_cps, (u64)n_push + 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/set-goal-stores-pair");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_a(), mk_e());
    Term rhs = mk_a();
    thvm_atp_set_goal(s, lhs, rhs);
    CHECK_EQ(s->goal_lhs, lhs);
    CHECK_EQ(s->goal_rhs, rhs);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/set-goal-zero-clears");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_e(), mk_e());
    thvm_atp_set_goal(s, 0, 0);
    CHECK_EQ(s->goal_lhs, 0u);
    CHECK_EQ(s->goal_rhs, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/select-cp-empty-queue");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term l = 0, r = 0;
    CHECK_EQ(thvm_atp_select_cp(s, &l, &r), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/select-cp-priority-order");
  {
    // Stage 5.3: cheapest CP wins.  Symbol counts:
    //   l1 = f(x, e), r1 = x       -> k_1 = (1+1+1) + 1 = 4
    //   l2 = a,        r2 = e       -> k_2 = 1 + 1     = 2
    //   l3 = e,        r3 = a       -> k_3 = 1 + 1     = 2
    // Pop order: l2/r2 (k=2, dfs=1), l3/r3 (k=2, dfs=2), l1/r1 (k=4).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term l1 = mk_f(mk_v(VAR_x), mk_e()),  r1 = mk_v(VAR_x);
    Term l2 = mk_a(),                      r2 = mk_e();
    Term l3 = mk_e(),                      r3 = mk_a();
    thvm_atp_add_equation(s, l1, r1);
    thvm_atp_add_equation(s, l2, r2);
    thvm_atp_add_equation(s, l3, r3);
    CHECK_EQ(s->n_cps, 3u);

    // 7c: select_cp returns the stored (canonically renumbered) CP;
    // compare up to that alpha-renaming.  The priority ORDER (which
    // CP pops when) is unchanged -- renumbering preserves symbol
    // counts, so the heap key is identical.
    Term lo = 0, ro = 0;
    CHECK(thvm_atp_select_cp(s, &lo, &ro));
    CHECK(kbo_eq(lo, tt_norm_lhs(l2, r2)));
    CHECK(kbo_eq(ro, tt_norm_rhs(l2, r2)));
    CHECK_EQ(s->n_cps, 2u);

    CHECK(thvm_atp_select_cp(s, &lo, &ro));
    CHECK(kbo_eq(lo, tt_norm_lhs(l3, r3)));
    CHECK(kbo_eq(ro, tt_norm_rhs(l3, r3)));
    CHECK_EQ(s->n_cps, 1u);

    CHECK(thvm_atp_select_cp(s, &lo, &ro));
    CHECK(kbo_eq(lo, tt_norm_lhs(l1, r1)));
    CHECK(kbo_eq(ro, tt_norm_rhs(l1, r1)));
    CHECK_EQ(s->n_cps, 0u);

    // Now empty.
    CHECK_EQ(thvm_atp_select_cp(s, &lo, &ro), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/select-cp-fifo-interleave");
  {
    // Waldmeister CP-queue interleaving (port of KPVerwaltung.c
    // CPdimension): 1 FIFO pick per ATP_CP_FIFO_MODULO (=11)
    // selections.  CP 0 is the oldest (lowest cp_seq) AND the
    // heaviest; CPs 1..11 are 11 identical light CPs.  The first 10
    // selections take the weight min -> light CPs; selection 11 is
    // the FIFO dimension -> the oldest CP (CP 0), which pure weight
    // would defer to last.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term heavy = mk_f(mk_f(mk_a(), mk_a()), mk_f(mk_a(), mk_a()));
    thvm_atp_cp_set(s, 0, heavy, mk_a());          // oldest + heaviest
    for (u32 i = 1; i <= 11; i++) {
      thvm_atp_cp_set(s, i, mk_a(), mk_a());       // light
      s->cp_trace[i] = ATP_TRACE_NONE;
    }
    s->cp_trace[0] = ATP_TRACE_NONE;
    s->n_cps = 12;
    thvm_atp_cp_reheapify(s);

    Term lo = 0, ro = 0;
    for (u32 i = 0; i < 10; i++) {
      CHECK(thvm_atp_select_cp(s, &lo, &ro));
      CHECK_EQ(term_ext(lo), LAB_a);               // weight picks: light
    }
    // Selection 11 (cp_select_count == 10) -- the FIFO dimension.
    CHECK(thvm_atp_select_cp(s, &lo, &ro));
    CHECK_EQ(term_tag(lo), TAG_CTR);
    CHECK_EQ(term_ext(lo), LAB_f);                 // the heavy oldest CP
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/select-cp-shifts-tail-densely");
  {
    // After one pop, the remaining items should occupy slots [0..n-1).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_e(), mk_e());
    thvm_atp_add_equation(s, mk_a(), mk_a());

    Term lo = 0, ro = 0;
    thvm_atp_select_cp(s, &lo, &ro);
    CHECK_EQ(s->n_cps, 1u);
    // The remaining equation is now at slot 0.
    Term q_lhs = 0, q_rhs = 0;
    thvm_atp_cp_get(s, 0, &q_lhs, &q_rhs);
    CHECK(q_lhs != 0);
    CHECK_EQ(term_ext(q_lhs), LAB_a);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/orient-and-add-kbo-gt");
  {
    // f(x, e) > x  -> push f(x, e) -> x as a single rule.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    AtpAddedRange r = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(r.first, 0u);
    CHECK_EQ(r.count, 1u);
    CHECK_EQ(s->n_rules, 1u);
    // 7c: the stored rule is the canonically renumbered input.
    CHECK(kbo_eq(s->lhs[0], tt_norm_lhs(lhs, rhs)));
    CHECK(kbo_eq(s->rhs[0], tt_norm_rhs(lhs, rhs)));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/orient-and-add-kbo-lt-swaps");
  {
    // x < f(x, e)  -> KBO_LT, push f(x, e) -> x (swapped).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_v(VAR_x);
    Term rhs = mk_f(mk_v(VAR_x), mk_e());
    AtpAddedRange r = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(r.count, 1u);
    CHECK_EQ(s->n_rules, 1u);
    // Stored as rhs -> lhs (the swap); 7c renumbers it canonically.
    CHECK(kbo_eq(s->lhs[0], tt_norm_lhs(rhs, lhs)));
    CHECK(kbo_eq(s->rhs[0], tt_norm_rhs(rhs, lhs)));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/orient-and-add-kbo-un-pushes-both");
  {
    // Two distinct FVRs: x and y.  Neither dominates the other on
    // var counts, so KBO returns UN -- unfailing fallback adds both
    // orientations.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_v(VAR_x);
    Term rhs = mk_v(1u);   // VAR_y
    AtpAddedRange r = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(r.first, 0u);
#ifdef ATP_VAR_NORM
    // 7c: canonical renumbering numbers by first occurrence shared
    // across both sides.  The forward leg (x, y) renumbers to
    // (v0, v1); the reverse leg (y, x) ALSO renumbers to (v0, v1) --
    // the two orientations of `x = y` are alpha-equivalent.  The 7c
    // duplicate-rule guard therefore rejects the reverse leg as a
    // byte-identical duplicate, so exactly one rule is stored.  This
    // is a genuine behavior change, not a hidden regression: two
    // byte-identical rules are indistinguishable to the rewriter, so
    // collapsing them is behavior-neutral for rewriting -- only
    // n_rules changes.  The 2-leg unfailing path is still exercised
    // wherever the two orientations are NOT alpha-equivalent.
    CHECK_EQ(r.count, 1u);
    CHECK_EQ(s->n_rules, 1u);
    CHECK(kbo_eq(s->lhs[0], tt_norm_lhs(lhs, rhs)));
    CHECK(kbo_eq(s->rhs[0], tt_norm_rhs(lhs, rhs)));
    // 7c: re-adding the same unorientable equation stores nothing --
    // both legs renumber to the rule already in R, so the
    // duplicate-rule guard rejects both.  R stays at one rule.
    AtpAddedRange r2 = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(r2.count, 0u);
    CHECK_EQ(s->n_rules, 1u);
#else
    CHECK_EQ(r.count, 2u);
    CHECK_EQ(s->n_rules, 2u);
    CHECK_EQ(s->lhs[0], lhs);
    CHECK_EQ(s->rhs[0], rhs);
    CHECK_EQ(s->lhs[1], rhs);
    CHECK_EQ(s->rhs[1], lhs);
#endif
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/orient-and-add-kbo-eq-no-op");
  {
    // Caller bug case: lhs and rhs structurally identical (KBO_EQ).
    // orient_and_add returns count = 0, R unchanged.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    AtpAddedRange r = thvm_atp_orient_and_add(s, mk_e(), mk_e());
    CHECK_EQ(r.count, 0u);
    CHECK_EQ(s->n_rules, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/generate-cps-empty-added-no-op");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    AtpAddedRange empty = {0, 0};
    u32 pushed = thvm_atp_generate_cps(s, empty);
    CHECK_EQ(pushed, 0u);
    CHECK_EQ(s->n_cps, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/generate-cps-single-rule-self-overlap");
  {
    // Add one rule via orient_and_add, then generate_cps.  Since R
    // contains only the new rule, the only enumeration is the 1x1
    // self-overlap (always produced at the top position via the
    // fresh rename of j).
    //
    // Stage 7.1: that self-overlap CP is always trivially joinable
    // (both sides reduce to the same renamed RHS), so the filter
    // drops it -- pushed must be 0 and the dropped-counter ticks.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(added.count, 1u);
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK_EQ(pushed, 0u);
    CHECK_EQ(s->n_cps, 0u);
    CHECK(s->n_cps_dropped_joinable >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/generate-cps-old-times-new-direction");
  {
    // Pre-populate R with the assoc rule (manually, no orient).
    // Then add the left-id rule via orient_and_add and run
    // generate_cps.  The (new x all) sweep covers
    // left-id-overlapped-into-assoc; the (old x new) sweep
    // covers assoc-overlapping-leftid.
    //
    // Stage 7.1: under {assoc, left-id}, every survivable overlap
    // produces a CP that's already joinable by R (e.g. assoc x
    // left-id at the inner f gives `(f(b,c), f(e, f(b,c)))`, which
    // collapses to `f(b,c) = f(b,c)` after applying left-id).
    // So the filter drops them all and the counter ticks.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    // R[0] = assoc: f(f(x,y), z) -> f(x, f(y, z))
    s->lhs[0] = mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u));
    s->rhs[0] = mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u)));
    s->n_rules = 1;

    // Add left-id: f(e, x) -> x.  Right side is var, left side is
    // a CTR, so KBO_GT under our config.
    Term lhs = mk_f(mk_e(), mk_v(VAR_x));
    Term rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(added.count, 1u);
    CHECK_EQ(added.first, 1u);

    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK_EQ(pushed, 0u);
    CHECK(s->n_cps_dropped_joinable >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-range-equals-full-when-bounds-cover-all");
  {
    // thvm_critical_pairs == thvm_critical_pairs_range over [0, n) x [0, n).
    Term lhs[1] = { mk_f(mk_v(VAR_x), mk_e()) };
    Term rhs[1] = { mk_v(VAR_x) };
    CriticalPair a[16] = {{0, 0}}, b[16] = {{0, 0}};
    u32 na = thvm_critical_pairs(lhs, rhs, 1, a, 16);
    u32 nb = thvm_critical_pairs_range(lhs, rhs, 1, 0, 1, 0, 1, b, 16);
    CHECK_EQ(na, nb);
  }

  TEST_BEGIN("atp/interreduce-empty-added-no-op");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    AtpAddedRange empty = {0, 0};
    CHECK_EQ(thvm_atp_interreduce(s, empty), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/interreduce-drops-specialization");
  {
    // Pre-populate R with a SPECIALIZED rule:
    //   R[0]: f(a, e) -> f(a, a)
    // Add the more-general rule via orient_and_add:
    //   R[1]: f(x, e) -> x          (KBO_GT under DUMMY_CFG)
    // After interreduce, R[0]'s LHS reduces under R[1] (top match
    // with x = a), so it should be dropped and the simplified
    // equation (a, f(a, a)) requeued.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    s->lhs[0] = mk_f(mk_a(), mk_e());
    s->rhs[0] = mk_f(mk_a(), mk_a());
    s->n_rules = 1;
    u32 n_cps_before = s->n_cps;

    Term gen_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term gen_rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, gen_lhs, gen_rhs);
    CHECK_EQ(added.count, 1u);
    CHECK_EQ(added.first, 1u);
    CHECK_EQ(s->n_rules, 2u);

    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 1u);
    // R now holds only the new general rule, shifted down to slot 0.
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(term_ext(s->lhs[0]), LAB_f);
    // CP queue grew by one: the requeued (reduced, old_rhs).
    CHECK_EQ(s->n_cps, n_cps_before + 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/interreduce-requeue-records-simplify-lineage");
  {
    // Regression guard for the trace-DAG severance fix: a rule
    // dropped by interreduce is re-queued as the equation
    // (reduced, old_rhs).  That re-queue must record a
    // TRACE_SIMPLIFY entry parented on the dropped rule's trace
    // index -- a fresh TRACE_AXIOM would disconnect the proof DAG.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    // R[0] enters through the saturation path so it carries a real
    // trace lineage: add_equation queues it (TRACE_AXIOM at idx 0),
    // a step orients it (TRACE_ORIENT, r_trace[0] -> that ORIENT).
    thvm_atp_add_equation(s, mk_f(mk_a(), mk_e()), mk_f(mk_a(), mk_a()));
    thvm_atp_step(s);
    CHECK_EQ(s->n_rules, 1u);
    u32 old_trace = s->r_trace[0];
    CHECK(old_trace != ATP_TRACE_NONE);

    u32 trace_before = s->n_trace;

    // The more-general rule subsumes R[0]'s lhs at top.
    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    CHECK_EQ(added.count, 1u);
    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 1u);

    // The interreduce re-queue pushed exactly one new trace entry:
    // a TRACE_SIMPLIFY whose parent_a is the dropped rule's trace.
    CHECK_EQ(s->n_trace, trace_before + 1u);
    Term simp = s->trace[trace_before];
    CHECK_EQ(term_ext(simp), TRACE_SIMPLIFY);
    CHECK_EQ((u32)term_val(term_ctr_at(simp, 0)), old_trace);
    CHECK_EQ((u32)term_val(term_ctr_at(simp, 1)), ATP_TRACE_NONE);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/interreduce-keeps-irreducible-rules");
  {
    // R[0]: i(a) -> i(a)         (degenerate; just to fill a slot)
    // Add R[1]: f(x, e) -> x.  R[0]'s LHS i(a) doesn't match
    // f(?, ?) at top, so nothing reduces; R[0] stays.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term ia0 = term_new_ctr(LAB_f - 1, /* unused; want different label */ NULL, 0);
    (void)ia0;
    Term cs[1] = { mk_a() };
    s->lhs[0] = term_new_ctr(2, cs, 1);  // label 2 (i)
    s->rhs[0] = term_new_ctr(2, cs, 1);
    s->n_rules = 1;

    Term gen_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term gen_rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, gen_lhs, gen_rhs);
    CHECK_EQ(added.count, 1u);

    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 0u);
    CHECK_EQ(s->n_rules, 2u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/interreduce-no-old-rules-no-op");
  {
    // First-rule-add case: added.first == 0, nothing older to
    // interreduce.  Function should return 0 without underflowing.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term gen_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term gen_rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, gen_lhs, gen_rhs);
    CHECK_EQ(added.first, 0u);
    CHECK_EQ(thvm_atp_interreduce(s, added), 0u);
    CHECK_EQ(s->n_rules, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-no-goal-runs-on");
  {
    // goal_lhs == 0 -> completion mode -> never returns PROVED.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_RUNNING);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-trivial-goal-proves-without-rules");
  {
    // Goal is e == e under empty R; both sides normalize to e
    // (identity), kbo_eq holds, returns ATP_PROVED.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_e(), mk_e());
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_PROVED);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-closes-under-rule");
  {
    // Goal: f(a, e) == a.  Add rule f(x, e) -> x.  Normalizing
    // f(a, e) under R -> a; rhs already a; goal proves.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_e()), mk_a());
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_PROVED);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-doesnt-close-still-running");
  {
    // Goal: a == e.  No rule applies; both sides normalize to
    // themselves; kbo_eq fails (different labels); RUNNING.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_a(), mk_e());
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_RUNNING);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/step-empty-queue-no-goal-yields-queue-empty");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ((int)thvm_atp_step(s), (int)ATP_QUEUE_EMPTY);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/step-trivial-goal-proves-without-work");
  {
    // Goal e == e is already trivially true; goal_check at top of
    // step fires before anything else.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_e(), mk_e());
    CHECK_EQ((int)thvm_atp_step(s), (int)ATP_PROVED);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/step-cap-zero-times-out-when-no-goal-trivial");
  {
    // step_cap = 0 with a non-trivial goal -- should TIMEOUT before
    // doing any work.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 0);
    thvm_atp_set_goal(s, mk_a(), mk_e());
    CHECK_EQ((int)thvm_atp_step(s), (int)ATP_TIMEOUT);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/run-one-step-prove");
  {
    // Goal: f(a, e) == a.  Push axiom f(x, e) = x onto queue.
    // First step: pops the axiom, normalizes (no R, both stay),
    // not trivially equal, KBO_GT orients it as f(x, e) -> x,
    // adds to R, no interreduce (no old rules), generates self-CPs,
    // goal_check fires: f(a, e) reduces to a, equal -> ATP_PROVED.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_e()), mk_a());
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);
    CHECK(s->n_rules >= 1u);
    CHECK(s->step <= 4u);   // converges in one or two real steps
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/run-saturates-empty-queue-completion-mode");
  {
    // No goal, push one trivially-self-equal equation.  Step pops
    // it, normalizes (both sides identical), trivializes; queue
    // empties; next step returns QUEUE_EMPTY.  thvm_atp_run loops
    // through and returns QUEUE_EMPTY.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_e(), mk_e());
    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_QUEUE_EMPTY);
    CHECK(s->step >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-add-equation-records-axiom");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    thvm_atp_add_equation(s, lhs, rhs);
    CHECK_EQ(s->n_trace, 1u);
    Term entry = s->trace[0];
    CHECK_EQ(term_ext(entry), TRACE_AXIOM);
    CHECK_EQ(term_val(term_ctr_at(entry, 0)), ATP_TRACE_NONE);   // p_a
    CHECK_EQ(term_val(term_ctr_at(entry, 1)), ATP_TRACE_NONE);   // p_b
    // cp_trace[0] points back to the axiom we just pushed.
    CHECK_EQ(s->cp_trace[0], 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-orient-records-source-cp-as-parent");
  {
    // After one step, the orient entry should carry the source CP's
    // trace index as parent_a.  With one axiom on the queue, the
    // step pops it, orients (TRACE_ORIENT at index 1 with parent_a=0),
    // then generate_cps fires on the new rule's self-overlap and
    // pushes one or more TRACE_CP entries.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    thvm_atp_add_equation(s, lhs, rhs);
    CHECK_EQ(s->n_trace, 1u);    // axiom recorded
    AtpStatus st = thvm_atp_step(s);
    CHECK_EQ((int)st, (int)ATP_RUNNING);
    CHECK_EQ(s->n_rules, 1u);
    // At minimum: axiom + orient (TRACE_CP entries from generate_cps
    // bump n_trace further; just check >= 2 here, exact CP count
    // covered separately below).
    CHECK(s->n_trace >= 2u);
    Term orient = s->trace[1];
    CHECK_EQ(term_ext(orient), TRACE_ORIENT);
    CHECK_EQ(term_val(term_ctr_at(orient, 0)), 0u);   // parent_a = axiom
    CHECK_EQ(term_val(term_ctr_at(orient, 1)), ATP_TRACE_NONE);   // parent_b
    // r_trace[0] (the new rule) was set to the orient trace index.
    CHECK_EQ(s->r_trace[0], 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-cp-records-source-rules-as-parents");
  {
    // Stage 7.1 changed self-overlap behavior: a single rule's
    // self-overlap is always trivially joinable, so the filter
    // drops it.  To get a CP that SURVIVES the filter we need
    // two non-confluent rules:
    //   r0: f(e, x) -> x   (left-id; rhs is variable)
    //   r1: f(x, e) -> a   (rhs is constant a)
    // Cross-overlap at the top unifies f(e, x) with f(y, e),
    // giving y=e, x=e; r0 says result = e, r1 says result = a;
    // CP = (e, a), NOT joinable under R.
    //
    // We pre-install r0, then orient_and_add r1, then run
    // generate_cps.  The (new x all) sweep produces TRACE_CP
    // entries; the cross-overlap CP's parent_a/parent_b should
    // be r_trace[1] / r_trace[0] (r1 is i=1, r0 is j=0).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    // Pre-install r0: f(e, x) -> x.  Manually plumbed; r_trace[0]
    // gets a synthetic TRACE_AXIOM entry so the parent-pointer
    // assertions below have something to check against.
    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   s->lhs[0], s->rhs[0]);
    s->n_rules = 1;

    // Orient and add r1: f(x, e) -> a.  orient_and_add itself
    // doesn't emit TRACE_ORIENT or set r_trace -- that's done by
    // thvm_atp_step.  Since this test bypasses step, we plumb
    // r_trace[1] manually so generate_cps can read it.
    Term lhs1 = mk_f(mk_v(VAR_x), mk_e());
    Term rhs1 = mk_a();
    AtpAddedRange added = thvm_atp_orient_and_add(s, lhs1, rhs1);
    CHECK_EQ(added.count, 1u);
    CHECK_EQ(added.first, 1u);
    s->r_trace[1] = atp_trace_push(s, TRACE_ORIENT, s->r_trace[0],
                                   ATP_TRACE_NONE,
                                   s->lhs[1], s->rhs[1]);

    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK(pushed >= 1u);
    // The trace contains: [r0 axiom (0), r1 orient (1),
    //                      then >= 1 CP entries from this point].
    CHECK(s->n_trace >= 3u);

    // Find the first TRACE_CP entry and check its parents.
    u8 found_cp = 0;
    for (u32 i = 2; i < s->n_trace; i++) {
      Term entry = s->trace[i];
      if (term_ext(entry) == TRACE_CP) {
        u32 pa = (u32)term_val(term_ctr_at(entry, 0));
        u32 pb = (u32)term_val(term_ctr_at(entry, 1));
        // Parents come from r_trace[i] / r_trace[j].  Both must be
        // valid trace indices (not ATP_TRACE_NONE) and < entry idx.
        CHECK(pa != ATP_TRACE_NONE);
        CHECK(pb != ATP_TRACE_NONE);
        CHECK(pa < i);
        CHECK(pb < i);
        found_cp = 1;
        break;
      }
    }
    CHECK(found_cp == 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/headline-prove-f-a-ia-equals-e-from-group-axioms");
  {
    // Stage 5.5 demo from docs/plans/waldmeister_ic_atp.md sec.5:
    // prove f(a, i(a)) == e from the standard group axioms via
    // saturation.  Under the same KBO config as test_kbo.c
    // (weights i=0, f=1, e=1, a=1; precedence i > f > e > a; w0=1)
    // the right-inverse axiom directly closes the goal once it
    // lands in R.
    //
    //   right-id:    f(x, e)         = x        k = 4
    //   right-inv:   f(x, i(x))      = e        k = 5
    //   assoc:       f(f(x, y), z)   = f(x, f(y, z))   k = 10
    //
    // Cheapest-first selection pops trivial self-CPs early; the
    // right-inv axiom typically fires within ~5 steps.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);

    thvm_atp_set_goal(s,
                      mk_f(mk_a(), mk_i(mk_a())),
                      mk_e());

    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),       mk_e());
    thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));

    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);
    // The proof is short: well under the step cap.
    CHECK(s->step <= 20u);
    // R picked up at least the right-inverse rule.
    CHECK(s->n_rules >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-serialize-empty-trace");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    char buf[256] = {0};
    u32 n = thvm_atp_trace_serialize(s, buf, sizeof(buf));
    CHECK_EQ(n, 0u);
    CHECK_EQ((int)buf[0], 0);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-serialize-single-axiom");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_e(), mk_a());
    char buf[256] = {0};
    u32 n = thvm_atp_trace_serialize(s, buf, sizeof(buf));
    CHECK(n > 0u);
    CHECK(strstr(buf, "0 (axiom): ") != NULL);
    CHECK(strstr(buf, "C1") != NULL);   // LAB_e = 1
    CHECK(strstr(buf, "C4") != NULL);   // LAB_a = 4
    CHECK(strstr(buf, " = ") != NULL);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-serialize-renders-fvr-and-ctr-args");
  {
    // f(x_0, e) = x_0 -- exercises the TAG_CTR with two children
    // and TAG_FVR rendering.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    char buf[256] = {0};
    thvm_atp_trace_serialize(s, buf, sizeof(buf));
    CHECK(strstr(buf, "C3(x_0, C1)") != NULL);
    CHECK(strstr(buf, "= x_0")        != NULL);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-serialize-orient-with-parent");
  {
    // Push axiom + step.  Trace includes axiom and orient
    // (parent=0); after stage 7.1 the self-overlap CP is
    // trivially joinable and gets filtered out, so no "(cp from
    // ...): " line appears.  Verify the axiom and orient lines
    // are still present and well-formed.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_step(s);
    char buf[1024] = {0};
    thvm_atp_trace_serialize(s, buf, sizeof(buf));
    CHECK(strstr(buf, "0 (axiom): ")        != NULL);
    CHECK(strstr(buf, "1 (orient from 0): ") != NULL);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-serialize-truncates-on-small-buffer");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_a(), mk_e());
    char buf[16] = {0};
    u32 n = thvm_atp_trace_serialize(s, buf, sizeof(buf));
    CHECK(n <= sizeof(buf) - 1);
    CHECK_EQ((int)buf[sizeof(buf) - 1], 0);   // null-terminated
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/headline-trace-shape-and-walk-to-axiom");
  {
    // Stage 6.1d: same headline demo, but verify the trace makes
    // sense.  After ATP_PROVED we should see:
    //   - exactly 3 TRACE_AXIOM entries (the 3 axioms we pushed)
    //   - at least 1 TRACE_ORIENT entry (the rule(s) added to R)
    //   - some number of TRACE_CP entries from generate_cps
    // Then walk parent_a from the latest TRACE_ORIENT back through
    // the trace; the walk must terminate at a TRACE_AXIOM.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);
    thvm_atp_set_goal(s,
                      mk_f(mk_a(), mk_i(mk_a())),
                      mk_e());
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),       mk_e());
    thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));

    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);

    u32 n_axiom = 0, n_orient = 0, n_cp = 0;
    for (u32 i = 0; i < s->n_trace; i++) {
      u32 r = term_ext(s->trace[i]);
      if      (r == TRACE_AXIOM)  n_axiom++;
      else if (r == TRACE_ORIENT) n_orient++;
      else if (r == TRACE_CP)     n_cp++;
    }
    CHECK_EQ(n_axiom, 3u);     // exactly the 3 axioms pushed
    CHECK(n_orient >= 1u);     // proof needed at least one rule
    (void)n_cp;                // count varies; just confirm structure walks

    // Find the latest TRACE_ORIENT entry (closest to the proof).
    u32 walk_idx = ATP_TRACE_NONE;
    for (u32 i = s->n_trace; i > 0; i--) {
      if (term_ext(s->trace[i - 1]) == TRACE_ORIENT) {
        walk_idx = i - 1;
        break;
      }
    }
    CHECK(walk_idx != ATP_TRACE_NONE);

    // Walk parent_a until we hit a TRACE_AXIOM.  Cap hops so a
    // corrupted pointer can't loop forever.
    u32 hops = 0;
    u32 final_reason = ATP_TRACE_NONE;
    while (walk_idx != ATP_TRACE_NONE && hops < 100) {
      Term entry = s->trace[walk_idx];
      u32  reason = term_ext(entry);
      if (reason == TRACE_AXIOM) {
        final_reason = TRACE_AXIOM;
        break;
      }
      walk_idx = (u32)term_val(term_ctr_at(entry, 0));   // parent_a
      hops++;
    }
    CHECK_EQ(final_reason, TRACE_AXIOM);

    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/trace-cp-carries-superposition-position");
  {
    // A TRACE_CP entry is a wider CTR: children 0..3 are the base
    // [NUM(pa), NUM(pb), lhs, rhs], child 4 is NUM(pos_len),
    // children 5.. are the superposition path.  Push a surviving CP
    // carrying pos {0} and verify atp_push_cps_traced records it.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);
    s->n_rules = 0;   // empty R -> the CP is not trivially joinable
    CriticalPair batch[1] = {{0}};
    batch[0].lhs     = mk_f(mk_a(), mk_e());   // f(a, e) != a under {}
    batch[0].rhs     = mk_a();
    batch[0].pos[0]  = 0u;
    batch[0].pos_len = 1u;
    u32 before = s->n_trace;
    u32 pushed = atp_push_cps_traced(s, batch, 1,
                                     ATP_TRACE_NONE, ATP_TRACE_NONE,
                                     ATP_RULE_NONE, ATP_RULE_NONE);
    CHECK_EQ(pushed, 1u);
    CHECK_EQ(s->n_trace, before + 1u);
    Term e = s->trace[s->n_trace - 1u];
    CHECK_EQ((int)term_ext(e), (int)TRACE_CP);
    CHECK_EQ(term_ctr_n(e), 6u);                       // 4 base + len + 1
    Term plen = term_ctr_at(e, 4);
    CHECK_EQ((int)term_tag(plen), (int)TAG_NUM);
    CHECK_EQ((u32)term_val(plen), 1u);                 // pos_len
    CHECK_EQ((u32)term_val(term_ctr_at(e, 5)), 0u);    // pos[0]
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/proof-extract-no-goal-returns-zero");
  {
    // No goal set -> nothing to extract.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);
    AtpProofStep steps[8];
    CHECK_EQ(thvm_atp_proof_extract(s, steps, 8u), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/proof-extract-headline-group-chain");
  {
    // Same headline demo (prove f(a, i(a)) == e from the group
    // axioms); after PROVED, extract the equational rewrite chain.
    // The chain is the goal_lhs walk (side 0) then the goal_rhs walk
    // (side 1), both forward, meeting at a common normal form.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);
    Term goal_l = mk_f(mk_a(), mk_i(mk_a()));
    Term goal_r = mk_e();
    thvm_atp_set_goal(s, goal_l, goal_r);
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))), mk_e());
    thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));

    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);

    AtpProofStep steps[ATP_PROOF_MAX_STEPS];
    u32 n = thvm_atp_proof_extract(s, steps, ATP_PROOF_MAX_STEPS);
    CHECK(n >= 1u);                         // a non-trivial proof

    // Per-side contiguity: each step hands its result to the next
    // within the same side; the two sides start at goal_lhs /
    // goal_rhs and end at a common normal form.
    Term lhs_end = goal_l, rhs_end = goal_r;
    u8   seen_lhs = 0, seen_rhs = 0;
    for (u32 i = 0; i < n; i++) {
      CHECK(steps[i].rule < s->n_rules);    // cited rule in range
      if (steps[i].side == 0u) {
        CHECK(kbo_eq(steps[i].before, lhs_end));
        lhs_end = steps[i].after;
        seen_lhs = 1;
      } else {
        CHECK(kbo_eq(steps[i].before, rhs_end));
        rhs_end = steps[i].after;
        seen_rhs = 1;
      }
    }
    (void)seen_lhs; (void)seen_rhs;
    CHECK(kbo_eq(lhs_end, rhs_end));         // the two sides meet

    // The serializer renders non-empty, null-terminated text.
    char buf[2048] = {0};
    u32 w = thvm_atp_proof_serialize(steps, n, buf, sizeof(buf));
    CHECK(w > 0u);
    CHECK_EQ((int)buf[w], 0);
    CHECK(strstr(buf, "rule ") != NULL);

    thvm_atp_free(s);
  }

  // === Stage 7.1: trivial-joinability filter ==========================

  TEST_BEGIN("atp/cp-joinability-filter-self-overlap-counter");
  {
    // A single rule's self-overlap is always trivially joinable;
    // generate_cps should drop it and bump n_cps_dropped_joinable.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = ATP_TRACE_NONE;
    s->n_rules = 1;

    AtpAddedRange added = {0, 1};
    CHECK_EQ(s->n_cps_dropped_joinable, 0u);
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK_EQ(pushed, 0u);
    CHECK(s->n_cps_dropped_joinable >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-joinability-filter-survives-non-joinable");
  {
    // Two non-confluent rules: cross-overlap is NOT trivially
    // joinable.  Verify the survivor reaches the queue.
    //   r0: f(e, x) -> x   (left-id; rhs is variable)
    //   r1: f(x, e) -> a   (rhs is constant a)
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = ATP_TRACE_NONE;
    s->lhs[1] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[1] = mk_a();
    s->r_trace[1] = ATP_TRACE_NONE;
    s->n_rules = 2;

    AtpAddedRange added = {1, 1};
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK(pushed >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-joinability-filter-counter-on-saturation");
  {
    // Full group-axiom saturation: many self-overlaps and
    // assoc-driven CPs are trivially joinable.  After running to
    // completion or timeout, the dropped counter must be > 0.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    (void)thvm_atp_run(s);
    CHECK(s->n_cps_dropped_joinable >= 1u);
    thvm_atp_free(s);
  }

  // === Stage 7.2b: source-rule-disjoint connectedness counter ========

  TEST_BEGIN("atp/cp-connectedness-counter-on-self-overlap");
  {
    // A single rule's self-overlap is trivially joinable -- the
    // CP collapses to (var, var) under any substitution.  Both
    // counters tick, with `dropped_connected` tracking the
    // domination relationship.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = ATP_TRACE_NONE;
    s->n_rules = 1;

    AtpAddedRange added = {0, 1};
    CHECK_EQ(s->n_cps_dropped_joinable,  0u);
    CHECK_EQ(s->n_cps_dropped_connected, 0u);
    (void)thvm_atp_generate_cps(s, added);
    // Domination lemma: connected count <= joinable count.
    CHECK(s->n_cps_dropped_connected <= s->n_cps_dropped_joinable);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-connectedness-genuine-CP-not-dropped");
  {
    // Two non-confluent rules whose top-overlap CP is genuine:
    //   r0: f(a, x) -> a   (rhs is constant)
    //   r1: f(y, b) -> b   (rhs is a different constant)
    // Cross-overlap unifies y=a, x=b; CP = (a, b).  Without rules
    // 0 and 1, R is empty -- (a, b) cannot be joined.  Both
    // counters stay at zero for this CP.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(),         mk_v(VAR_x));
    s->rhs[0] = mk_a();
    s->r_trace[0] = ATP_TRACE_NONE;
    s->lhs[1] = mk_f(mk_v(VAR_x),    mk_e());
    s->rhs[1] = mk_e();
    s->r_trace[1] = ATP_TRACE_NONE;
    s->n_rules = 2;

    // Manually invoke the connectedness check on (a, e) under
    // R \ {0, 1} = {} -- expected NOT joinable.
    Term na = mk_a();
    Term ne = mk_e();
    u8 conn = atp_cp_source_disjoint_connected(s, na, ne, 0u, 1u);
    CHECK_EQ(conn, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-connectedness-empty-filter-falls-through");
  {
    // ATP_RULE_NONE as the "exclude no rules" sentinel: with both
    // rule_a and rule_b out of range, the filtered set equals R
    // and the result matches trivial-joinability.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;

    // (f(a, e), a) joins under r0 to (a, a).  joinable AND
    // connected (with sentinel exclusion).
    Term l = mk_f(mk_a(), mk_e());
    Term r = mk_a();
    Term jl = l, jr = r;          // joinability reduces its args in place
    u8 join = atp_cp_trivially_joinable(s, &jl, &jr);
    u8 conn = atp_cp_source_disjoint_connected(s, l, r,
                                               ATP_RULE_NONE,
                                               ATP_RULE_NONE);
    CHECK_EQ(join, 1u);
    CHECK_EQ(conn, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-connectedness-domination-on-saturation");
  {
    // Empirical confirmation of the domination lemma on a real
    // saturation: connected count <= joinable count throughout.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    (void)thvm_atp_run(s);
    CHECK(s->n_cps_dropped_connected <= s->n_cps_dropped_joinable);
    thvm_atp_free(s);
  }

  // === Stage 7.3a: rule-subsumption counter ==========================

  TEST_BEGIN("atp/cp-rule-subsumed-direct-instance");
  {
    // Rule r0: f(x, e) -> x.  CP candidate (f(a, e), a) is a
    // direct substitution instance under σ = {x -> a}; so
    // rule-subsumed and (since the rule reduces lhs to rhs in
    // one step) also trivially joinable.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;

    Term lhs = mk_f(mk_a(), mk_e());
    Term rhs = mk_a();
    Term jl = lhs, jr = rhs;      // joinability reduces its args in place
    CHECK_EQ((int)atp_cp_rule_subsumed(s, lhs, rhs),       1);
    CHECK_EQ((int)atp_cp_trivially_joinable(s, &jl, &jr), 1);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-rule-subsumed-symmetric-instance");
  {
    // Same rule r0: f(x, e) -> x.  CP candidate (a, f(a, e))
    // is the symmetric direction (rhs = σ l_k, lhs = σ r_k).
    // Should still register as rule-subsumed.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;

    Term lhs = mk_a();
    Term rhs = mk_f(mk_a(), mk_e());
    CHECK_EQ((int)atp_cp_rule_subsumed(s, lhs, rhs), 1);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-rule-subsumed-non-instance-no-fire");
  {
    // Rule r0: f(x, e) -> x.  CP candidate (a, e) is not a
    // substitution instance of either direction (the rule's
    // lhs is f(_, _), can't match an atom).  Should NOT fire.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;

    Term lhs = mk_a();
    Term rhs = mk_e();
    CHECK_EQ((int)atp_cp_rule_subsumed(s, lhs, rhs), 0);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-rule-subsumed-domination-on-saturation");
  {
    // Empirical: rule-subsumed count is bounded above by
    // joinable count throughout the group-axiom saturation.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    (void)thvm_atp_run(s);
    CHECK(s->n_cps_dropped_rule_subsumed <= s->n_cps_dropped_joinable);
    thvm_atp_free(s);
  }

  // === Stage 7.3b: queue-subsumption filter ==========================

  TEST_BEGIN("atp/cp-queue-subsumed-direct-instance");
  {
    // Pre-populate the queue with the more-general CP
    // (f(x, e), x).  A candidate (f(a, e), a) is its instance
    // under σ = {x -> a}; queue-subsumed should fire.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_cp_set(s, 0, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    s->cp_trace[0] = ATP_TRACE_NONE;
    s->n_cps = 1;

    Term lhs = mk_f(mk_a(), mk_e());
    Term rhs = mk_a();
    CHECK_EQ(tt_queue_subsumed(s, lhs, rhs), 1);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-queue-subsumed-symmetric-instance");
  {
    // Same setup but candidate sides swapped.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_cp_set(s, 0, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    s->cp_trace[0] = ATP_TRACE_NONE;
    s->n_cps = 1;

    Term lhs = mk_a();
    Term rhs = mk_f(mk_a(), mk_e());
    CHECK_EQ(tt_queue_subsumed(s, lhs, rhs), 1);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-queue-subsumed-empty-queue-no-fire");
  {
    // Empty queue: nothing to subsume against.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_a(), mk_e());
    Term rhs = mk_a();
    CHECK_EQ(tt_queue_subsumed(s, lhs, rhs), 0);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-queue-subsumed-non-instance-no-fire");
  {
    // Queue has (f(x, e), x).  Candidate (g(a), a) does not
    // unify with the queued LHS (different head symbol).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_cp_set(s, 0, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    s->n_cps = 1;

    Term lhs = mk_a();         // not a CTR with the f label
    Term rhs = mk_e();
    CHECK_EQ(tt_queue_subsumed(s, lhs, rhs), 0);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-queue-subsumed-filter-drops-instance");
  {
    // Functional test: pre-queue a general CP, then run
    // generate_cps with a setup that produces an instance.
    // Verify the instance is dropped by the queue filter
    // (n_cps_dropped_queue_subsumed ticks) and the queue size
    // does not grow.
    //
    // Pre-queue the more-general (f(x, e), x).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_cp_set(s, 0, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    s->cp_trace[0] = ATP_TRACE_NONE;
    s->n_cps = 1;

    // Two rules whose cross-overlap manufactures the instance
    // (f(a, e), a):
    //   r0: f(a, x) -> a
    //   r1: f(x, e) -> x
    // Top unification: r0.lhs = f(a, x), r1.lhs = f(y, e);
    // unify y=a, x=e; r0 RHS subst -> a, r1 RHS subst -> e.
    // CP = (a, e).  Hmm, that's not the instance we wanted.
    //
    // Take the simpler route: directly verify the filter via
    // atp_push_cps_traced with a hand-built CriticalPair
    // batch.
    s->n_rules = 0;   // no rules so trivially-joinable doesn't fire spuriously

    // 8e: atp_push_cps_traced routes queue subsumption through
    // cp_graph; resync it from the hand-populated cp_packed[]/n_cps.
    thvm_atp_cp_reheapify(s);

    CriticalPair batch[1] = {{0}};
    batch[0].lhs = mk_f(mk_a(), mk_e());
    batch[0].rhs = mk_a();
    u32 before_cnt = s->n_cps;
    u32 pushed = atp_push_cps_traced(s, batch, 1,
                                     ATP_TRACE_NONE, ATP_TRACE_NONE,
                                     ATP_RULE_NONE, ATP_RULE_NONE);
    CHECK_EQ(pushed, 0u);
    CHECK_EQ(s->n_cps, before_cnt);   // queue did not grow
    CHECK(s->n_cps_dropped_queue_subsumed >= 1u);
    thvm_atp_free(s);
  }

  // === Stage 8.1e-i: use_ic_cp_gen flag round-trip ====================

  TEST_BEGIN("atp/cp-gen-flag-default-off");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->use_ic_cp_gen, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-gen-flag-toggle-preserves-output");
  {
    // Stage 8.1e-ii landed the IC-routed enumerator: setting
    // use_ic_cp_gen = 1 routes the per-position unify+apply
    // through APP-PRI / prim_unify_apply3 instead of calling
    // thvm_unify_apply directly.  Two parallel runs on the same
    // axiom; verify n_cps, n_rules, and the joinability counter
    // agree exactly between the two paths.
    AtpState *s_c = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s_c, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    AtpStatus st_c = thvm_atp_step(s_c);

    AtpState *s_ic = thvm_atp_init(&DUMMY_CFG, 100);
    s_ic->use_ic_cp_gen = 1;
    CHECK_EQ(s_ic->use_ic_cp_gen, 1u);
    thvm_atp_add_equation(s_ic, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    AtpStatus st_ic = thvm_atp_step(s_ic);

    CHECK_EQ((int)st_c, (int)st_ic);
    CHECK_EQ(s_c->n_cps,                    s_ic->n_cps);
    CHECK_EQ(s_c->n_rules,                  s_ic->n_rules);
    CHECK_EQ(s_c->n_cps_dropped_joinable,   s_ic->n_cps_dropped_joinable);

    thvm_atp_free(s_c);
    thvm_atp_free(s_ic);
  }

  TEST_BEGIN("atp/rewrite-flag-default-off");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->use_ic_rewrite, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/rewrite-flag-toggle-preserves-output");
  {
    // 8.3e-ii: enabling use_ic_rewrite routes per-rule matching
    // through APP-PRI / prim_rewrite_step.  Two parallel runs on
    // the same axiom; verify n_cps, n_rules, and the joinability
    // counter agree exactly between the two paths.
    AtpState *s_c = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s_c, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    AtpStatus st_c = thvm_atp_step(s_c);

    AtpState *s_ic = thvm_atp_init(&DUMMY_CFG, 100);
    s_ic->use_ic_rewrite = 1;
    CHECK_EQ(s_ic->use_ic_rewrite, 1u);
    thvm_atp_add_equation(s_ic, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    AtpStatus st_ic = thvm_atp_step(s_ic);

    CHECK_EQ((int)st_c, (int)st_ic);
    CHECK_EQ(s_c->n_cps,                  s_ic->n_cps);
    CHECK_EQ(s_c->n_rules,                s_ic->n_rules);
    CHECK_EQ(s_c->n_cps_dropped_joinable, s_ic->n_cps_dropped_joinable);

    thvm_atp_free(s_c);
    thvm_atp_free(s_ic);
  }

  TEST_BEGIN("atp/rewrite-ic-parity-on-group-axioms");
  {
    // Full group-axiom saturation under both rewrite paths.
    AtpState *s_c = thvm_atp_init(&DUMMY_CFG, 32);
    thvm_atp_set_goal(s_c, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_c, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s_c, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s_c,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_c = thvm_atp_run(s_c);

    AtpState *s_ic = thvm_atp_init(&DUMMY_CFG, 32);
    s_ic->use_ic_rewrite = 1;
    thvm_atp_set_goal(s_ic, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_ic, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s_ic, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s_ic,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_ic = thvm_atp_run(s_ic);

    CHECK_EQ((int)rst_c, (int)rst_ic);
    CHECK_EQ(s_c->n_rules, s_ic->n_rules);

    thvm_atp_free(s_c);
    thvm_atp_free(s_ic);
  }

  TEST_BEGIN("atp/cp-gen-ic-parity-on-group-axioms");
  {
    // Full group-axiom saturation under both paths.  Final
    // status must agree, n_rules must agree, and the trace
    // length should match (modulo the fact that joinability /
    // queue-subsumption filters reject CPs identically when
    // the C and IC paths produce the same CP terms).
    AtpState *s_c = thvm_atp_init(&DUMMY_CFG, 32);
    thvm_atp_set_goal(s_c, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_c, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s_c, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s_c,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_c = thvm_atp_run(s_c);

    AtpState *s_ic = thvm_atp_init(&DUMMY_CFG, 32);
    s_ic->use_ic_cp_gen = 1;
    thvm_atp_set_goal(s_ic, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_ic, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s_ic, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s_ic,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_ic = thvm_atp_run(s_ic);

    CHECK_EQ((int)rst_c, (int)rst_ic);
    CHECK_EQ(s_c->n_rules, s_ic->n_rules);

    thvm_atp_free(s_c);
    thvm_atp_free(s_ic);
  }

  // === Stage 8.5c: LPO ordering selector =============================

  // === Stage 8.10b: top-K CP peek ====================================

  TEST_BEGIN("atp/peek/empty-queue-returns-zero");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term o_lhs[4], o_rhs[4];
    CHECK_EQ(thvm_atp_peek_top_k(s, 4u, o_lhs, o_rhs), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/peek/k-zero-returns-zero");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_cp_set(s, 0, mk_a(), mk_e());
    s->n_cps = 1;
    Term o_lhs[1], o_rhs[1];
    CHECK_EQ(thvm_atp_peek_top_k(s, 0u, o_lhs, o_rhs), 0u);
    // Queue unchanged.
    CHECK_EQ(s->n_cps, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/peek/singleton");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_cp_set(s, 0, mk_a(), mk_e());
    s->n_cps = 1;
    Term o_lhs[2] = {0, 0}, o_rhs[2] = {0, 0};
    u32 n = thvm_atp_peek_top_k(s, 2u, o_lhs, o_rhs);
    CHECK_EQ(n, 1u);
    CHECK_EQ(term_tag(o_lhs[0]), TAG_CTR);
    CHECK_EQ(term_ext(o_lhs[0]), 4u);   // LAB_a
    CHECK_EQ(s->n_cps, 1u);   // queue unchanged
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/peek/orders-by-priority");
  {
    // Queue 3 CPs of differing sizes; verify peek returns them
    // in priority (size) order without mutating the queue.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    // CP_BIG = (f(f(x, e), e), x), size 5+1=6
    // CP_MID = (f(x, e), x),         size 3+1=4
    // CP_SML = (a, e),               size 1+1=2
    thvm_atp_cp_set(s, 0, mk_f(mk_f(mk_v(VAR_x), mk_e()), mk_e()),
                    mk_v(VAR_x));
    thvm_atp_cp_set(s, 1, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_cp_set(s, 2, mk_a(), mk_e());
    s->n_cps = 3;
    thvm_atp_cp_reheapify(s);  // 7c': hand-built queue -> heap

    Term o_lhs[3] = {0, 0, 0}, o_rhs[3] = {0, 0, 0};
    u32 n = thvm_atp_peek_top_k(s, 3u, o_lhs, o_rhs);
    CHECK_EQ(n, 3u);
    // First peek = CP_SML.
    CHECK_EQ(term_ext(o_lhs[0]), 4u);   // a
    CHECK_EQ(term_ext(o_rhs[0]), 1u);   // e
    // Last peek = CP_BIG (top is f).
    CHECK_EQ(term_ext(o_lhs[2]), 3u);   // f
    CHECK_EQ(term_ctr_n(o_lhs[2]), 2u);
    // Queue unchanged.
    CHECK_EQ(s->n_cps, 3u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/peek/k-greater-than-n-clamps");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_cp_set(s, 0, mk_a(), mk_e());
    thvm_atp_cp_set(s, 1, mk_e(), mk_a());
    s->n_cps = 2;
    thvm_atp_cp_reheapify(s);  // 7c': hand-built queue -> heap
    Term o_lhs[10], o_rhs[10];
    u32 n = thvm_atp_peek_top_k(s, 10u, o_lhs, o_rhs);
    CHECK_EQ(n, 2u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/peek/then-pop-stays-consistent");
  {
    // Peek shows cheapest first; subsequent select_cp should pop
    // the same CP that peek's [0] revealed.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_cp_set(s, 0, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_cp_set(s, 1, mk_a(), mk_e());
    s->n_cps = 2;
    s->cp_trace[0] = 100;
    s->cp_trace[1] = 200;
    thvm_atp_cp_reheapify(s);  // 7c': hand-built queue -> heap

    Term peek_lhs[2], peek_rhs[2];
    u32 n = thvm_atp_peek_top_k(s, 2u, peek_lhs, peek_rhs);
    CHECK_EQ(n, 2u);

    Term pop_lhs = 0, pop_rhs = 0;
    CHECK_EQ((int)thvm_atp_select_cp(s, &pop_lhs, &pop_rhs), 1);
    // Popped CP should match peek[0].
    CHECK_EQ((int)kbo_eq(pop_lhs, peek_lhs[0]), 1);
    CHECK_EQ((int)kbo_eq(pop_rhs, peek_rhs[0]), 1);
    thvm_atp_free(s);
  }

  // === Stage 8.9b: narrowing primitives ==============================

  TEST_BEGIN("atp/narrow/no-rules-returns-zero");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs_in = mk_a(), rhs_in = mk_a();
    Term lhs_out = 0, rhs_out = 0;
    RewriteSubst w = {{0}};
    CHECK_EQ((int)thvm_atp_narrow_step(s, lhs_in, rhs_in,
                                       &lhs_out, &rhs_out, &w), 0);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow/top-position-binds-witness");
  {
    // Rule: f(a) -> b (lhs=f(a), rhs=b).  Goal: f(x) = b with x
    // existential.  Top of f(x) unifies with f(a) -> bind x=a;
    // narrow rewrites lhs to b; rhs stays b.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e());   // not used, but we need
    s->rhs[0] = mk_a();                  // some rule -- replace below
    // Set rule 0 to f(a) = b.  Use mk_e as the "b" placeholder
    // since we don't have a fresh constant; actually mk_a is `a`,
    // mk_e is `e`.  Let's say rule: f(_, e) = a, goal: f(x_, e) = a.
    s->lhs[0] = mk_f(mk_a(), mk_e());   // f(a, e)
    s->rhs[0] = mk_a();                  // -> a
    s->n_rules = 1;

    Term goal_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term goal_rhs = mk_a();

    Term out_lhs = 0, out_rhs = 0;
    RewriteSubst w = {{0}};
    u8 ok = thvm_atp_narrow_step(s, goal_lhs, goal_rhs,
                                 &out_lhs, &out_rhs, &w);
    CHECK_EQ((int)ok, 1);

    // After narrow at top: out_lhs = sigma(rule.rhs) = a;
    //                      out_rhs = sigma(goal_rhs) = a.
    CHECK_EQ(term_tag(out_lhs), TAG_CTR);
    CHECK_EQ(term_ext(out_lhs), 4u);   // LAB_a
    CHECK_EQ(term_tag(out_rhs), TAG_CTR);
    CHECK_EQ(term_ext(out_rhs), 4u);

    // Witness: x bound to a.
    Term wx = w.bindings[VAR_x];
    CHECK_EQ(term_tag(wx), TAG_CTR);
    CHECK_EQ(term_ext(wx), 4u);   // LAB_a
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow/no-unifier-returns-zero");
  {
    // Rule: f(a, e) = a.  Goal: g(_) = a -- top doesn't unify
    // (different head); no sub-positions either (g is unary).
    // After 8.9b's recursive walk the inner FVR position is
    // skipped (FVRs aren't tried), so no narrow step applies.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e());
    s->rhs[0] = mk_a();
    s->n_rules = 1;

    Term goal_lhs = mk_i(mk_v(VAR_x));   // i is a CTR head, but
                                          // doesn't match f
    Term goal_rhs = mk_a();
    Term out_lhs = 0, out_rhs = 0;
    RewriteSubst w = {{0}};
    CHECK_EQ((int)thvm_atp_narrow_step(s, goal_lhs, goal_rhs,
                                       &out_lhs, &out_rhs, &w), 0);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow/get-witness-empty");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ((u64)thvm_atp_get_witness(s, VAR_x), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow/get-witness-out-of-range");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ((u64)thvm_atp_get_witness(s, REWRITE_MAX_VAR), 0u);
    CHECK_EQ((u64)thvm_atp_get_witness(s, REWRITE_MAX_VAR + 7u), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow/get-witness-roundtrip");
  {
    // After a successful narrow, populate s->witness_subst
    // explicitly and verify get_witness reads it back.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->witness_subst.bindings[VAR_x] = mk_a();
    Term wx = thvm_atp_get_witness(s, VAR_x);
    CHECK_EQ(term_tag(wx), TAG_CTR);
    CHECK_EQ(term_ext(wx), 4u);
    thvm_atp_free(s);
  }

  // === Stage 9.1b: multi-witness DFS narrowing =======================

  TEST_BEGIN("atp/narrow_all/no-rules-returns-zero");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    RewriteSubst out[4] = {{{0}}};
    u32 n = thvm_atp_narrow_all(s, mk_a(), mk_e(), 4u, 4u, out);
    CHECK_EQ(n, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow_all/already-equal-emits-empty-witness");
  {
    // lhs == rhs at depth 0: short-circuits to one witness with
    // no bindings (empty acc).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e()); s->rhs[0] = mk_a(); s->n_rules = 1;
    RewriteSubst out[4] = {{{0}}};
    u32 n = thvm_atp_narrow_all(s, mk_a(), mk_a(), 4u, 4u, out);
    CHECK_EQ(n, 1u);
    // Empty acc: every binding slot stays 0.
    for (u32 i = 0; i < REWRITE_MAX_VAR; i++) CHECK_EQ(out[0].bindings[i], 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow_all/depth-zero-no-narrow");
  {
    // Rule applies but max_depth=0 forbids any narrow step.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e()); s->rhs[0] = mk_a(); s->n_rules = 1;
    RewriteSubst out[4] = {{{0}}};
    u32 n = thvm_atp_narrow_all(s, mk_f(mk_v(VAR_x), mk_e()), mk_a(),
                                0u, 4u, out);
    CHECK_EQ(n, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow_all/single-witness-binds-x");
  {
    // Same as 8.9b's narrow_step happy path: rule f(a, e) -> a,
    // goal f(x, e) = a.  DFS finds exactly one witness with x=a.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e()); s->rhs[0] = mk_a(); s->n_rules = 1;
    RewriteSubst out[4] = {{{0}}};
    u32 n = thvm_atp_narrow_all(s, mk_f(mk_v(VAR_x), mk_e()), mk_a(),
                                4u, 4u, out);
    CHECK_EQ(n, 1u);
    Term wx = out[0].bindings[VAR_x];
    CHECK_EQ(term_tag(wx), TAG_CTR);
    CHECK_EQ(term_ext(wx), 4u);   // LAB_a
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow_all/two-rules-two-witnesses");
  {
    // Two distinct rules, both unify with goal at top with
    // different bindings.  Rule 0: f(a, e) -> a (binds x=a).
    // Rule 1: f(e, e) -> a (binds x=e).  Goal: f(x, e) = a.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e()); s->rhs[0] = mk_a();
    s->lhs[1] = mk_f(mk_e(), mk_e()); s->rhs[1] = mk_a();
    s->n_rules = 2;
    RewriteSubst out[8] = {{{0}}};
    u32 n = thvm_atp_narrow_all(s, mk_f(mk_v(VAR_x), mk_e()), mk_a(),
                                4u, 8u, out);
    CHECK(n >= 2u);
    // Collect distinct CTR labels for x across the witnesses; we
    // expect both LAB_a (4) and LAB_e (1) to appear.
    u8 saw_a = 0, saw_e = 0;
    for (u32 i = 0; i < n; i++) {
      Term wx = out[i].bindings[VAR_x];
      if (term_tag(wx) != TAG_CTR) continue;
      if (term_ext(wx) == 4u) saw_a = 1;
      if (term_ext(wx) == 1u) saw_e = 1;
    }
    CHECK_EQ((int)saw_a, 1);
    CHECK_EQ((int)saw_e, 1);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow_all/max-witnesses-caps-count");
  {
    // Same multi-witness setup but cap at 1.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e()); s->rhs[0] = mk_a();
    s->lhs[1] = mk_f(mk_e(), mk_e()); s->rhs[1] = mk_a();
    s->n_rules = 2;
    RewriteSubst out[1] = {{{0}}};
    u32 n = thvm_atp_narrow_all(s, mk_f(mk_v(VAR_x), mk_e()), mk_a(),
                                4u, 1u, out);
    CHECK_EQ(n, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/narrow_all/state-untouched");
  {
    // Verify thvm_atp_narrow_all does not mutate s->witness_subst.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e()); s->rhs[0] = mk_a(); s->n_rules = 1;
    s->witness_subst.bindings[VAR_x] = mk_e();   // pre-existing sentinel
    RewriteSubst out[4] = {{{0}}};
    (void)thvm_atp_narrow_all(s, mk_f(mk_v(VAR_x), mk_e()), mk_a(),
                              4u, 4u, out);
    // Sentinel survives.
    CHECK_EQ(term_tag(s->witness_subst.bindings[VAR_x]), TAG_CTR);
    CHECK_EQ(term_ext(s->witness_subst.bindings[VAR_x]), 1u);   // LAB_e
    thvm_atp_free(s);
  }

  // === Stage 8.9c: existential goal integration ======================

  TEST_BEGIN("atp/exist/default-off");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->goal_existential, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/exist/set-flips-flag");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_a();
    CHECK_EQ((int)thvm_atp_set_goal_existential(s, lhs, rhs), 1);
    CHECK_EQ(s->goal_existential, 1u);
    CHECK_EQ(s->goal_lhs, lhs);
    // Clear via lhs == 0 turns it back off.
    CHECK_EQ((int)thvm_atp_set_goal_existential(s, 0, 0), 1);
    CHECK_EQ(s->goal_existential, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/exist/narrow-proves-with-witness");
  {
    // Pre-install rule f(a, e) -> a.  Existential goal:
    // f(x, e) = a -- find x.  Narrow at top: unify f(x, e) with
    // renamed f(a, e) (renames x -> x'; we have `e` literally),
    // bind x = a.  After narrow: lhs = a, rhs = a.  PROVED.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e());
    s->rhs[0] = mk_a();
    s->n_rules = 1;

    thvm_atp_set_goal_existential(s,
      mk_f(mk_v(VAR_x), mk_e()),
      mk_a());

    AtpStatus st = thvm_atp_goal_check(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);

    Term wx = thvm_atp_get_witness(s, VAR_x);
    CHECK_EQ(term_tag(wx), TAG_CTR);
    CHECK_EQ(term_ext(wx), 4u);   // LAB_a
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/exist/no-narrow-returns-running");
  {
    // Rule f(a, e) -> a.  Existential goal: g(x_) = a -- no
    // narrowing applies (head g != head f at top, no sub-positions
    // unify with a CTR rule LHS).  Returns RUNNING.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_a(), mk_e());
    s->rhs[0] = mk_a();
    s->n_rules = 1;

    thvm_atp_set_goal_existential(s, mk_i(mk_v(VAR_x)), mk_a());
    AtpStatus st = thvm_atp_goal_check(s);
    CHECK_EQ((int)st, (int)ATP_RUNNING);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/exist/already-equal-proves-no-narrow");
  {
    // Existential goal where lhs == rhs structurally; the
    // narrow path should short-circuit before any narrow_step.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term t = mk_a();
    Term t2 = mk_a();
    thvm_atp_set_goal_existential(s, t, t2);
    AtpStatus st = thvm_atp_goal_check(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/lpo-default-off");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK(s->lpo == NULL);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/lpo-orient-prefers-lpo-when-attached");
  {
    // Build a rule (f(x, e) -> x) and orient it under both
    // KBO and LPO; verify both succeed and produce the same
    // rule shape.  Under LPO, f(x, e) > x by subterm dominance
    // (x is a strict subterm of f(x, e)).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    // Construct an LpoConfig with a precedence consistent with
    // DUMMY_CFG's labels (e=1, i=2, f=3, a=4).
    static u32 lpo_prec[5] = {0, 1, 4, 3, 2};
    static const LpoConfig LPO_CFG = {
      .precedence = lpo_prec,
      .n_labels   = 5,
    };
    thvm_atp_set_lpo(s, &LPO_CFG);
    CHECK(s->lpo == &LPO_CFG);

    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(added.count, 1u);
    // Single-direction rule (LPO should give GT, not unfailing).
    CHECK_EQ(s->n_rules, 1u);

    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/lpo-set-clear-roundtrip");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    static u32 prec[5] = {0, 1, 2, 3, 4};
    static const LpoConfig CFG = { .precedence = prec, .n_labels = 5 };
    thvm_atp_set_lpo(s, &CFG);
    CHECK(s->lpo == &CFG);
    thvm_atp_set_lpo(s, NULL);
    CHECK(s->lpo == NULL);
    thvm_atp_free(s);
  }

  // === Stage 8.8: --mix heuristic =====================================

  TEST_BEGIN("atp/mix-heuristic-default-off");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->use_mix_heuristic, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/mix-heuristic-changes-pop-order");
  {
    // Build two CPs:
    //   CP_A: (a, e)         -- size 2; KBO orients (different
    //                            constants, head-precedence wins).
    //   CP_B: (i(x), e)      -- size 3; orients KBO_GT (i is heavier
    //                            in DUMMY_CFG: prec[i]=4 > prec[e]=2,
    //                            so i(x) > e).
    // Both orient cleanly under DUMMY_CFG.  No --mix penalty kicks
    // in for either; --mix and --add agree.  But construct a CP
    // that's KBO_UN: (f(x, y), f(y, x)) -- distinct vars, weights
    // identical, top-symbols equal, lex-arg compare gives UN.
    //
    // Mix-priority on the UN CP: size 7 + penalty 4 = 11.
    // Add-priority on UN CP: size 7.
    // Add a small CP: (a, e), size 2.
    //
    // Without --mix: pop order favors size 2 (CP_AE) -> size 7 (CP_UN).
    // With --mix: same, since CP_AE has lower base priority anyway.
    //
    // To distinguish: build (i(a), e) (size 3, orients GT) and
    // (f(x, y), f(y, x)) (size 7, UN; mix penalty +4 -> 11).  Both
    // heuristics pop the GT one first.
    //
    // Easier test: the priority HELPER directly.  Build a CP that
    // would orient UN under DUMMY_CFG; verify atp_cp_priority
    // returns base+penalty when the flag is set, base when off.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    Term lhs = mk_f(mk_v(VAR_x), mk_v(1u));
    Term rhs = mk_f(mk_v(1u), mk_v(VAR_x));
    // Confirm this pair is KBO_UN under DUMMY_CFG (distinct vars
    // -- domination check fails).
    CHECK_EQ((int)thvm_kbo(lhs, rhs, &DUMMY_CFG), (int)KBO_UN);

    u32 add_prio = atp_cp_priority(s, lhs, rhs);
    s->use_mix_heuristic = 1;
    u32 mix_prio = atp_cp_priority(s, lhs, rhs);
    CHECK(mix_prio > add_prio);
    CHECK_EQ(mix_prio - add_prio, MIX_UNORIENTED_PENALTY);

    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/mix-heuristic-no-penalty-on-clean-orient");
  {
    // f(x, e) > x via KBO_GT on DUMMY_CFG (lhs heavier, dominates).
    // --mix should NOT add a penalty since orientation is clean.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    CHECK_EQ((int)thvm_kbo(lhs, rhs, &DUMMY_CFG), (int)KBO_GT);

    u32 add_prio = atp_cp_priority(s, lhs, rhs);
    s->use_mix_heuristic = 1;
    u32 mix_prio = atp_cp_priority(s, lhs, rhs);
    CHECK_EQ(add_prio, mix_prio);

    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/mix-heuristic-saturation-still-correct");
  {
    // Full group-axiom saturation under --mix.  Outcome must
    // still match the default --add heuristic on this corpus
    // (mix only changes pop ORDER, not soundness; the saturator
    // explores the same closure regardless of order).
    AtpState *s_add = thvm_atp_init(&DUMMY_CFG, 32);
    thvm_atp_set_goal(s_add, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_add, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s_add, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s_add,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_add = thvm_atp_run(s_add);

    AtpState *s_mix = thvm_atp_init(&DUMMY_CFG, 32);
    s_mix->use_mix_heuristic = 1;
    thvm_atp_set_goal(s_mix, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_mix, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s_mix, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s_mix,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_mix = thvm_atp_run(s_mix);

    CHECK_EQ((int)rst_add, (int)rst_mix);
    thvm_atp_free(s_add);
    thvm_atp_free(s_mix);
  }

  // === CP-weight modes: ports of Waldmeister ClasHeuristics =========
  //
  // DUMMY_CFG: weights[e=1]=1, weights[i=2]=0, weights[f=3]=1,
  // weights[a=4]=1, var_weight=1, precedence={_,e:2,i:4,f:3,a:1}.
  // symbol_count counts every node as 1.

  TEST_BEGIN("atp/cp-weight-mode-default-is-gt");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->cp_weight_mode, (u8)ATP_CP_WEIGHT_GT);
    // ADD stays reachable: selecting it reproduces the bare
    // symbol-count sum.
    Term lhs = mk_f(mk_v(VAR_x), mk_e());   // symbol_count 3
    Term rhs = mk_v(VAR_x);                 // symbol_count 1
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_ADD);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 4u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-mode-clamps-out-of-range");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_cp_weight_mode(s, 999u);
    CHECK_EQ(s->cp_weight_mode, (u8)ATP_CP_WEIGHT_ADD);
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_MAX);
    CHECK_EQ(s->cp_weight_mode, (u8)ATP_CP_WEIGHT_MAX);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-maxweight");
  {
    // CH_MaxWeight: max of the two side symbol-counts.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());   // symbol_count 3
    Term rhs = mk_v(VAR_x);                 // symbol_count 1
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_ADD);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 4u);   // add
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_MAX);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 3u);   // max(3,1)
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-ordweight");
  {
    // CH_OrdWeight: KBO-weight sum (CF_Phi_KBO).  i(x) has KBO
    // weight 0 (i) + 1 (x) = 1, e has 1; sum 2.  Its symbol-count
    // sum (the --add value) is 2 (i,x) + 1 (e) = 3 -- so the two
    // modes genuinely diverge here.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_i(mk_v(VAR_x));
    Term rhs = mk_e();
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_ADD);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 3u);   // add: symbol count
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_ORD);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 2u);   // KBO-weight sum
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-gtweight-orients");
  {
    // CH_GtWeight: f(x,e) > x under KBO -> picks the lhs KBO
    // weight (1+1+1 = 3), not the sum (3 + 1 = 4).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    CHECK_EQ((int)thvm_kbo(lhs, rhs, &DUMMY_CFG), (int)KBO_GT);
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_GT);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 3u);   // lhs KBO weight
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-gtweight-unoriented-sums");
  {
    // CH_GtWeight on a KBO_UN pair falls through to the sum.
    // f(x,y) and f(y,x): each KBO weight 1+1+1 = 3 -> sum 6.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_v(1u));
    Term rhs = mk_f(mk_v(1u), mk_v(VAR_x));
    CHECK_EQ((int)thvm_kbo(lhs, rhs, &DUMMY_CFG), (int)KBO_UN);
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_GT);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 6u);   // wl + wr
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-mixweight");
  {
    // CH_MixWeight on the oriented pair f(x,e) > x:
    //   wl = 3, wr = 1, sum = 4, g (GtWeight) = wl = 3.
    //   mix = sum*g + g + sum = 12 + 3 + 4 = 19.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    CHECK_EQ((int)thvm_kbo(lhs, rhs, &DUMMY_CFG), (int)KBO_GT);
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_MIX);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 19u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-mixweight2");
  {
    // CH_MixWeight2 on the same oriented pair:
    //   g = 3, sum = 4 -> mix2 = g*10 + sum = 34.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_MIX2);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 34u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-unif-measure-zero-on-unifiable");
  {
    // CH_Unifikationsmass: f(x,e) and f(a,e) are unifiable
    // (x := a), so the unification measure is 0 -> weight 0.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_f(mk_a(), mk_e());
    CHECK_EQ(atp_unif_measure(lhs, rhs), 0u);
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_UNIF);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-unif-measure-clash");
  {
    // f(a,e) vs f(a,a): top f matches, child (a,a) matches,
    // child (e,a) is a function-symbol clash at depth d=1 -> the
    // measure is 1.  KBO weights are 3 and 3, so the UNIF weight
    // is (3+3)*1 = 6.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_a(), mk_e());
    Term rhs = mk_f(mk_a(), mk_a());
    CHECK_EQ(atp_unif_measure(lhs, rhs), 1u);
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_UNIF);
    CHECK_EQ(atp_cp_priority(s, lhs, rhs), 6u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-weight-modes-change-pop-order");
  {
    // Two CPs, both KBO-oriented:
    //   CP_BIG: f(f(x,e),e) = x   -- symbol_count 5 + 1 = 6
    //   CP_SML: i(x)         = e  -- symbol_count 2 + 1 = 3
    // Under MAX, CP_SML max(2,1)=2 pops before CP_BIG max(5,1)=5.
    // The CPs are seeded via thvm_atp_cp_set + reheapify (m8's
    // packed-byte-string queue API) so the mode is exercised
    // end-to-end through the heap.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_MAX);
    Term big_l = mk_f(mk_f(mk_v(VAR_x), mk_e()), mk_e());
    Term big_r = mk_v(VAR_x);
    Term sml_l = mk_i(mk_v(VAR_x));
    Term sml_r = mk_e();
    thvm_atp_cp_set(s, 0u, big_l, big_r);
    thvm_atp_cp_set(s, 1u, sml_l, sml_r);
    s->n_cps = 2;
    thvm_atp_cp_reheapify(s);
    Term out_l = 0, out_r = 0;
    CHECK(thvm_atp_select_cp(s, &out_l, &out_r));
    // Cheapest first: CP_SML (max weight 2) before CP_BIG (5).
    CHECK(term_tag(out_l) == TAG_CTR && term_ext(out_l) == LAB_i);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/lpo-vs-kbo-parity-on-group-axioms");
  {
    // Run the group-axiom saturation under both KBO and LPO.
    // On these axioms, both orderings are expected to orient
    // the rules the same way (KBO-with-weights and LPO-with-
    // precedence agree on the canonical group orientation),
    // so n_rules at termination should match.  This is the
    // empirical foundation for the bench-numbers-unchanged
    // observation in 8.5d.
    static u32 lpo_prec[5] = {0, 1, 4, 3, 2};   // matches DUMMY_CFG's precedence
    static const LpoConfig LPO_CFG = {
      .precedence = lpo_prec,
      .n_labels   = 5,
    };

    AtpState *s_kbo = thvm_atp_init(&DUMMY_CFG, 32);
    thvm_atp_set_goal(s_kbo, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_kbo, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s_kbo, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s_kbo,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_kbo = thvm_atp_run(s_kbo);

    AtpState *s_lpo = thvm_atp_init(&DUMMY_CFG, 32);
    thvm_atp_set_lpo(s_lpo, &LPO_CFG);
    thvm_atp_set_goal(s_lpo, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_lpo, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s_lpo, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),      mk_e());
    thvm_atp_add_equation(s_lpo,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_lpo = thvm_atp_run(s_lpo);

    // Same final status and rule count -- these axioms are oriented
    // identically by KBO and LPO.
    CHECK_EQ((int)rst_kbo, (int)rst_lpo);
    CHECK_EQ(s_kbo->n_rules, s_lpo->n_rules);

    thvm_atp_free(s_kbo);
    thvm_atp_free(s_lpo);
  }

  // === Stage 9.3: heap checkpoint/reset =============================

  TEST_BEGIN("atp/heap-checkpoint/reads-heap-next");
  {
    u64 c1 = thvm_atp_heap_checkpoint();
    // Allocate a fresh CTR; checkpoint should advance.
    (void)mk_a();
    u64 c2 = thvm_atp_heap_checkpoint();
    CHECK(c2 > c1);
  }

  TEST_BEGIN("atp/heap-reset/pops-back");
  {
    u64 c0 = thvm_atp_heap_checkpoint();
    Term t = mk_f(mk_a(), mk_e());
    (void)t;
    u64 c1 = thvm_atp_heap_checkpoint();
    CHECK(c1 > c0);
    thvm_atp_heap_reset(c0);
    u64 c2 = thvm_atp_heap_checkpoint();
    CHECK_EQ(c2, c0);
  }

  TEST_BEGIN("atp/heap-reset/refuses-to-advance");
  {
    u64 c0 = thvm_atp_heap_checkpoint();
    (void)mk_a();
    u64 c1 = thvm_atp_heap_checkpoint();
    // Calling reset with a checkpoint past the current HEAP_NEXT
    // is a silent no-op; HEAP_NEXT must not move forward.
    thvm_atp_heap_reset(c1 + 1024);
    u64 c2 = thvm_atp_heap_checkpoint();
    CHECK_EQ(c2, c1);
  }

  TEST_BEGIN("atp/heap-reset/joined-cp-reclaims-cells");
  {
    // After running saturation on axioms whose first CP is
    // trivially joinable, the per-step rewrite block in
    // thvm_atp_step pops back via heap_reset.  Compare HEAP_NEXT
    // before vs. after a single step that should join trivially.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 8);
    s->use_ic_rewrite = 1u;
    // Equation a = a -- normalize on both sides is a no-op, kbo_eq
    // matches at the top, the CP is discarded.
    thvm_atp_add_equation(s, mk_a(), mk_a());

    u64 before = thvm_atp_heap_checkpoint();
    AtpStatus st = thvm_atp_step(s);
    u64 after  = thvm_atp_heap_checkpoint();

    CHECK_EQ((int)st, (int)ATP_RUNNING);
    // For the joined-CP case the normalize cells are reclaimed,
    // so HEAP_NEXT does not balloon past `before` by more than a
    // small constant (at most a few cells from select_cp / book-
    // keeping that live above the checkpoint).
    CHECK(after <= before + 8u);
    thvm_atp_free(s);
  }

  // === ATP_CP_CLASSIFY: Waldmeister CP classification ==============
  // The `n_cps_dropped_classified` counter exists in AtpState in
  // both build modes.  With the flag OFF the classifier is compiled
  // out, so the counter must stay 0 even after a full run.

  TEST_BEGIN("atp/classify/counter-zero-when-flag-off");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 8);
    // f(x, e) = x : a clean oriented rule that saturates fast.
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_run(s);
#ifndef ATP_CP_CLASSIFY
    CHECK_EQ(s->n_cps_dropped_classified, 0u);
#endif
    thvm_atp_free(s);
  }

#ifdef ATP_CP_CLASSIFY
  TEST_BEGIN("atp/classify/killer-r-fires-when-cp-reduces-a-rule");
  {
    // R holds an oriented rule  f(a, a) -> a.  The CP  (f(a, a), a)
    // -- the same pair -- oriented big->small reduces that rule's
    // LHS, so KillerR must fire.  An unrelated CP  (i(b), b)  whose
    // LHS matches nothing in R must not.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_orient_and_add(s, mk_f(mk_a(), mk_a()), mk_a());
    CHECK_EQ(s->n_rules, 1u);

    CHECK(atp_clas_killer_r(s, mk_f(mk_a(), mk_a()), mk_a()) == 1u);
    // i(a) -> a : LHS i(a) matches no subterm of f(a,a), so no kill.
    CHECK(atp_clas_killer_r(s, mk_i(mk_a()), mk_a()) == 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/classify/killer-e-needs-unorientable-cp");
  {
    // KillerE only fires for an unorientable CP.  An orientable CP
    // that reduces a rule is a KillerR case, not KillerE.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_orient_and_add(s, mk_f(mk_a(), mk_a()), mk_a());

    // (f(a,a), a) is orientable (KBO_GT) -> KillerE must be 0.
    CHECK(atp_clas_killer_e(s, mk_f(mk_a(), mk_a()), mk_a()) == 0u);
    // (x, y) -- two distinct vars -- is unorientable; but its LHS
    // is a bare variable that matches no rule LHS, so still 0.
    CHECK(atp_clas_killer_e(s, mk_v(VAR_x), mk_v(1u)) == 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/classify/killer-re-is-union-of-r-and-e");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_orient_and_add(s, mk_f(mk_a(), mk_a()), mk_a());
    // KillerRE fires because KillerR fires.
    CHECK(atp_clas_killer_re(s, mk_f(mk_a(), mk_a()), mk_a()) == 1u);
    // Neither R nor E -- no rule LHS is reducible.
    CHECK(atp_clas_killer_re(s, mk_i(mk_a()), mk_a()) == 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/classify/echild-detects-unorientable-parent");
  {
    // Rule 0 oriented (f(a,a) -> a).  Rule 1 born unorientable
    // (x = y -> orient_and_add stores both directions / fallback).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_orient_and_add(s, mk_f(mk_a(), mk_a()), mk_a());
    AtpAddedRange un = thvm_atp_orient_and_add(s, mk_v(VAR_x), mk_v(1u));
    CHECK(un.count >= 1u);
    u32 un_idx = un.first;
    // A CP whose parent un_idx is unorientable -> EChild fires.
    CHECK(atp_clas_echild(s, 0u, un_idx) == 1u);
    // Both parents the oriented rule 0 -> EChild does not fire.
    CHECK(atp_clas_echild(s, 0u, 0u) == 0u);
    // Out-of-range "no parent" indices -> EChild does not fire.
    CHECK(atp_clas_echild(s, s->n_rules, s->n_rules) == 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/classify/classify-cp-drops-rule-subsumed-killer");
  {
    // The CP  (f(a, a), a)  is rule-subsumed by  f(a, a) -> a  AND a
    // KillerRE.  The default config maps KillerRE -> Act_never, and
    // the rule-subsumption soundness guard holds, so atp_classify_cp
    // signals a drop.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_orient_and_add(s, mk_f(mk_a(), mk_a()), mk_a());
    u32 pri = 99u;
    u8 drop = atp_classify_cp(s, mk_f(mk_a(), mk_a()), mk_a(),
                              0u, 0u, &pri);
    CHECK_EQ((int)drop, 1);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/classify/classify-cp-keeps-non-killer");
  {
    // (i(a), a) is neither a killer nor EChild for this R, so the
    // classifier keeps it and leaves the priority unscaled.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_orient_and_add(s, mk_f(mk_a(), mk_a()), mk_a());
    u32 base = atp_cp_priority(s, mk_i(mk_a()), mk_a());
    u32 pri  = base;
    u8 drop  = atp_classify_cp(s, mk_i(mk_a()), mk_a(), 0u, 0u, &pri);
    CHECK_EQ((int)drop, 0);
    CHECK_EQ(pri, base);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/classify/generate-cps-counter-resolves-status");
  {
    // End-to-end through atp_push_cps_traced: run a full saturation
    // and confirm the run still resolves to a normal terminal
    // status with the classifier wired into the CP-insertion path.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 32);
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_i(mk_v(VAR_x)), mk_v(VAR_x)), mk_e());
    AtpStatus st = thvm_atp_run(s);
    CHECK(st == ATP_QUEUE_EMPTY || st == ATP_TIMEOUT ||
          st == ATP_PROVED);
    thvm_atp_free(s);
  }
#endif  // ATP_CP_CLASSIFY

#ifdef ATP_CP_GROUND_JOIN
  // ==================================================================
  // Ground-joinability redundancy criterion (Section F of the spec).
  // These call the static `atp_cp_ground_joinable` directly (same TU
  // via the `#include "../src/thvm.c"` above).  Verdict 1 = provably
  // ground-joinable (would-DELETE); 0 = not shown (would-KEEP).
  // ==================================================================
  {
    enum { GJ_PLUS = 1u };
    static u32 ac_weights   [2] = {0, 1};
    static u32 ac_precedence[2] = {0, 1};
    static const KboConfig AC_CFG = {
      .weights = ac_weights, .precedence = ac_precedence,
      .n_labels = 2, .var_weight = 1,
    };
    #define MK_PLUS(a, b) ({ Term _cs[2] = {(a), (b)}; term_new_ctr(GJ_PLUS, _cs, 2); })

    // F.1: AC critical pair x+(y+z) vs z+(x+y) (gj_spec.md F.1).  This is
    // THE point of the symbolic upgrade: it is GROUND-JOINABLE and MUST be
    // DELETED (verdict 1).  The AC rule set is associativity (oriented),
    // commutativity (both directions), AND its left-commutativity
    // consequence x+(y+z)=y+(x+z) -- exactly the set under which the AC
    // theory is ground-CONFLUENT, so every ground instance of the CP
    // joins.  (Bare {assoc, comm} alone is NOT ground-confluent -- see
    // F.1c below -- so it would be UNSOUND to delete this CP under bare
    // {assoc, comm}; the symbolic test correctly refuses to.  Twee
    // likewise deletes this CP only once R has been completed with the
    // left-comm rule.)  The symbolic >=_C reasoning (no-op `<=` steps
    // under each variable ordering, plus the `=`-case variable
    // unification recursion) is what lets it join here where the previous
    // fresh-ground-constant version could not.
    TEST_BEGIN("atp/ground-join/F1-AC-pair-deletes");
    {
      AtpState *s = thvm_atp_init(&AC_CFG, 64);
      Term x = mk_v(0u), y = mk_v(1u), z = mk_v(2u);
      atp_push_rule(s, MK_PLUS(MK_PLUS(x, y), z), MK_PLUS(x, MK_PLUS(y, z)));
      atp_push_rule(s, MK_PLUS(x, y), MK_PLUS(y, x));
      atp_push_rule(s, MK_PLUS(y, x), MK_PLUS(x, y));
      atp_push_rule(s, MK_PLUS(x, MK_PLUS(y, z)), MK_PLUS(y, MK_PLUS(x, z)));
      atp_push_rule(s, MK_PLUS(y, MK_PLUS(x, z)), MK_PLUS(x, MK_PLUS(y, z)));
      Term cp_l = MK_PLUS(x, MK_PLUS(y, z));
      Term cp_r = MK_PLUS(z, MK_PLUS(x, y));
      CHECK_EQ(atp_cp_ground_joinable(s, cp_l, cp_r), 1);
      thvm_atp_free(s);
    }

    // F.1c: the SAME AC pair under bare {assoc, comm} (no left-comm) -> the
    // ground rewrite system is NOT confluent (e.g. z+(x+y) with x<y<z is a
    // normal form distinct from x+(y+z)), so the CP is NOT ground-joinable
    // by these two rules.  The symbolic test correctly KEEPS it (0).  This
    // is the load-bearing soundness guard for F.1: a test that DELETED
    // here would be unsound (it would have to over-approximate >=_C and
    // fire a comm step z+(x+y)->(x+y)+z that no ground sigma justifies).
    TEST_BEGIN("atp/ground-join/F1c-AC-pair-bare-keeps");
    {
      AtpState *s = thvm_atp_init(&AC_CFG, 64);
      Term x = mk_v(0u), y = mk_v(1u), z = mk_v(2u);
      atp_push_rule(s, MK_PLUS(MK_PLUS(x, y), z), MK_PLUS(x, MK_PLUS(y, z)));
      atp_push_rule(s, MK_PLUS(x, y), MK_PLUS(y, x));
      atp_push_rule(s, MK_PLUS(y, x), MK_PLUS(x, y));
      Term cp_l = MK_PLUS(x, MK_PLUS(y, z));
      Term cp_r = MK_PLUS(z, MK_PLUS(x, y));
      CHECK_EQ(atp_cp_ground_joinable(s, cp_l, cp_r), 0);
      thvm_atp_free(s);
    }

    // F.1d: a second genuinely-ground-joinable PERMUTATIVE CP -> DELETE.
    // Commutative binary f with both rewrite directions; the CP
    // f(x,y) = f(y,x) is its own commutativity instance and is joinable
    // under every ground ordering of x,y (orient f by the order; under
    // x=y both sides are identical).  MUST be DELETED (1).
    TEST_BEGIN("atp/ground-join/F1d-comm-pair-deletes");
    {
      enum { GJ_FC = 2u };
      static u32 wc[3] = {0, 0, 1};
      static u32 pc[3] = {0, 0, 1};
      static const KboConfig CC = {
        .weights = wc, .precedence = pc, .n_labels = 3, .var_weight = 1,
      };
      AtpState *s = thvm_atp_init(&CC, 64);
      Term x = mk_v(0u), y = mk_v(1u);
      #define MK_FC(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(GJ_FC, _c, 2); })
      atp_push_rule(s, MK_FC(x, y), MK_FC(y, x));
      atp_push_rule(s, MK_FC(y, x), MK_FC(x, y));
      CHECK_EQ(atp_cp_ground_joinable(s, MK_FC(x, y), MK_FC(y, x)), 1);
      thvm_atp_free(s);
      #undef MK_FC
    }

    // F.2.A: ground CP b = f(b) (no vars) -> KEEP.
    TEST_BEGIN("atp/ground-join/F2A-b-eq-fb-KEEP");
    {
      enum { GJ_F = 2u, GJ_A = 3u, GJ_B = 4u };
      static u32 w2[5] = {0, 0, 1, 1, 1};
      static u32 p2[5] = {0, 0, 3, 2, 1};        // f > a > b
      static const KboConfig C2 = {
        .weights = w2, .precedence = p2, .n_labels = 5, .var_weight = 1,
      };
      AtpState *s = thvm_atp_init(&C2, 64);
      Term xx = mk_v(0u);
      Term a  = term_new_ctr(GJ_A, NULL, 0);
      Term b  = term_new_ctr(GJ_B, NULL, 0);
      #define MK_F(t) ({ Term _c[1] = {(t)}; term_new_ctr(GJ_F, _c, 1); })
      atp_push_rule(s, MK_F(MK_F(xx)), MK_F(xx));
      atp_push_rule(s, MK_F(a), b);
      CHECK_EQ(atp_cp_ground_joinable(s, b, MK_F(b)), 0);
      thvm_atp_free(s);
      #undef MK_F
    }

    // F.2.B / tie-guard: x = y -> KEEP (must hold for any sound impl).
    TEST_BEGIN("atp/ground-join/F2B-x-eq-y-KEEP");
    {
      enum { GJ_G = 2u, GJ_H = 3u };
      static u32 w3[4] = {0, 0, 1, 1};
      static u32 p3[4] = {0, 0, 2, 3};
      static const KboConfig C3 = {
        .weights = w3, .precedence = p3, .n_labels = 4, .var_weight = 1,
      };
      AtpState *s = thvm_atp_init(&C3, 64);
      Term x = mk_v(0u), y = mk_v(1u);
      #define MK_G(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(GJ_G, _c, 2); })
      #define MK_H(t)    ({ Term _c[1] = {(t)};      term_new_ctr(GJ_H, _c, 1); })
      atp_push_rule(s, MK_G(x, y), MK_G(y, x));
      atp_push_rule(s, MK_G(y, x), MK_G(x, y));
      atp_push_rule(s, MK_H(MK_G(x, y)), x);
      CHECK_EQ(atp_cp_ground_joinable(s, x, y), 0);
      thvm_atp_free(s);
      #undef MK_G
      #undef MK_H
    }

    // F.2.C (KEEP guard): genuinely non-joinable overlap a = c with NO
    // rule relating them.  Two distinct constants, no rewrite, distinct
    // normal forms -> not ground-joinable -> KEEP (0).
    TEST_BEGIN("atp/ground-join/F2C-distinct-consts-KEEP");
    {
      enum { GJ_A2 = 2u, GJ_C2 = 3u, GJ_D2 = 4u };
      static u32 w4[5] = {0, 0, 1, 2, 1};
      static u32 p4[5] = {0, 0, 3, 2, 1};
      static const KboConfig C4 = {
        .weights = w4, .precedence = p4, .n_labels = 5, .var_weight = 1,
      };
      AtpState *s = thvm_atp_init(&C4, 64);
      Term a = term_new_ctr(GJ_A2, NULL, 0);
      Term c = term_new_ctr(GJ_C2, NULL, 0);
      Term d = term_new_ctr(GJ_D2, NULL, 0);
      // an unrelated rule that does not touch a, c
      atp_push_rule(s, d, a);
      CHECK_EQ(atp_cp_ground_joinable(s, a, c), 0);
      thvm_atp_free(s);
    }

    // F.2.D (tie-collision KEEP guard, gj_spec.md F.3): a CP that joins on
    // every STRICT order of its variables but FAILS when two variables
    // collide (are instantiated equal).  d(x,y) -> x and d(x,y) -> y as
    // two oriented-by-order projections of a commutative pairing: the CP
    // x = y is joinable only if x and y collapse, which they need not.
    // A strict-only enumeration (x<y and y<x both give distinct consts
    // that don't rejoin) would already KEEP, but a buggy >=_C that
    // over-approximates could "join" x=y; assert KEEP (0).  (Same shape
    // as F.2.B but isolated to make the tie-coverage requirement explicit.)
    TEST_BEGIN("atp/ground-join/F2D-tie-collision-KEEP");
    {
      enum { GJ_P = 2u, GJ_FST = 3u, GJ_SND = 4u };
      static u32 w5[5] = {0, 0, 1, 1, 1};
      static u32 p5[5] = {0, 0, 2, 3, 4};
      static const KboConfig C5 = {
        .weights = w5, .precedence = p5, .n_labels = 5, .var_weight = 1,
      };
      AtpState *s = thvm_atp_init(&C5, 64);
      Term x = mk_v(0u), y = mk_v(1u);
      #define MK_P(a, b)  ({ Term _c[2] = {(a), (b)}; term_new_ctr(GJ_P, _c, 2); })
      #define MK_FST(t)   ({ Term _c[1] = {(t)};      term_new_ctr(GJ_FST, _c, 1); })
      #define MK_SND(t)   ({ Term _c[1] = {(t)};      term_new_ctr(GJ_SND, _c, 1); })
      atp_push_rule(s, MK_P(x, y), MK_P(y, x));     // commutative pair
      atp_push_rule(s, MK_P(y, x), MK_P(x, y));
      atp_push_rule(s, MK_FST(MK_P(x, y)), x);      // fst(P) -> 1st arg
      atp_push_rule(s, MK_SND(MK_P(x, y)), y);      // snd(P) -> 2nd arg
      // The overlap fst(P(x,y)) vs fst(P(y,x)) gives x = y.
      CHECK_EQ(atp_cp_ground_joinable(s, x, y), 0);
      thvm_atp_free(s);
      #undef MK_P
      #undef MK_FST
      #undef MK_SND
    }

    // F.E2 (fresh-constant weight guard, gj_spec.md E2/F.3): the symbolic
    // test reasons over ALL grounding sigma and never substitutes a fresh
    // constant, so the "fresh constant heavier than a real symbol flips
    // KEEP into DELETE" hazard is structurally absent.  Re-run F.2.A with
    // the would-be-fresh weight inflated and confirm it stays KEEP (0).
    TEST_BEGIN("atp/ground-join/FE2-fresh-weight-stays-KEEP");
    {
      enum { GJ_F2 = 2u, GJ_A3 = 3u, GJ_B3 = 4u };
      static u32 w6[5] = {0, 0, 7, 5, 3};         // inflated weights
      static u32 p6[5] = {0, 0, 3, 2, 1};
      static const KboConfig C6 = {
        .weights = w6, .precedence = p6, .n_labels = 5, .var_weight = 1,
      };
      AtpState *s = thvm_atp_init(&C6, 64);
      Term a  = term_new_ctr(GJ_A3, NULL, 0);
      Term b  = term_new_ctr(GJ_B3, NULL, 0);
      Term xx = mk_v(0u);
      #define MK_F2(t) ({ Term _c[1] = {(t)}; term_new_ctr(GJ_F2, _c, 1); })
      atp_push_rule(s, MK_F2(MK_F2(xx)), MK_F2(xx));
      atp_push_rule(s, MK_F2(a), b);
      CHECK_EQ(atp_cp_ground_joinable(s, b, MK_F2(b)), 0);
      thvm_atp_free(s);
      #undef MK_F2
    }

    // F.diff: differential soundness cross-check.  For the two DELETE
    // verdicts (F.1 AC pair, F.1d comm pair) verify that a large sample of
    // concrete ground instances actually join under independent ground-KBO
    // rewriting.  A contradiction is a soundness bug -> the assert fails.
    TEST_BEGIN("atp/ground-join/Fdiff-AC-delete-is-ground-sound");
    {
      AtpState *s = thvm_atp_init(&AC_CFG, 64);
      Term x = mk_v(0u), y = mk_v(1u), z = mk_v(2u);
      atp_push_rule(s, MK_PLUS(MK_PLUS(x, y), z), MK_PLUS(x, MK_PLUS(y, z)));
      atp_push_rule(s, MK_PLUS(x, y), MK_PLUS(y, x));
      atp_push_rule(s, MK_PLUS(y, x), MK_PLUS(x, y));
      atp_push_rule(s, MK_PLUS(x, MK_PLUS(y, z)), MK_PLUS(y, MK_PLUS(x, z)));
      atp_push_rule(s, MK_PLUS(y, MK_PLUS(x, z)), MK_PLUS(x, MK_PLUS(y, z)));
      Term cp_l = MK_PLUS(x, MK_PLUS(y, z));
      Term cp_r = MK_PLUS(z, MK_PLUS(x, y));
      CHECK_EQ(atp_cp_ground_joinable(s, cp_l, cp_r), 1);     // DELETE
      CHECK_EQ(gjt_differential(s, cp_l, cp_r, AC_CFG.n_labels, 4000), 1);
      thvm_atp_free(s);
    }

    TEST_BEGIN("atp/ground-join/Fdiff-comm-delete-is-ground-sound");
    {
      enum { GJ_FD = 2u };
      static u32 wd[3] = {0, 0, 1};
      static u32 pd[3] = {0, 0, 1};
      static const KboConfig CD = {
        .weights = wd, .precedence = pd, .n_labels = 3, .var_weight = 1,
      };
      AtpState *s = thvm_atp_init(&CD, 64);
      Term x = mk_v(0u), y = mk_v(1u);
      #define MK_FD(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(GJ_FD, _c, 2); })
      atp_push_rule(s, MK_FD(x, y), MK_FD(y, x));
      atp_push_rule(s, MK_FD(y, x), MK_FD(x, y));
      Term cp_l = MK_FD(x, y), cp_r = MK_FD(y, x);
      CHECK_EQ(atp_cp_ground_joinable(s, cp_l, cp_r), 1);     // DELETE
      CHECK_EQ(gjt_differential(s, cp_l, cp_r, CD.n_labels, 4000), 1);
      thvm_atp_free(s);
      #undef MK_FD
    }
    #undef MK_PLUS
  }
#endif  // ATP_CP_GROUND_JOIN

  thvm_free();
  TEST_REPORT();
}
