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

// All-unit-weight variant (the McCune/HigmanNeumann .pr orderings:
// every symbol weight 1, i > f in precedence).  DUMMY_CFG's weight-0
// `i` flips the orientation of rules like f(i(x),i(y)) = i(f(y,x)).
static u32 unit_weights[5] = {0, 1, 1, 1, 1};
static const KboConfig UNIT_CFG = {
  .weights     = unit_weights,
  .precedence  = dummy_precedence,
  .n_labels    = 5,
  .var_weight  = 1,
};

// Test wrapper for atp_cp_queue_subsumed.  These tests populate
// the queue (thvm_atp_cp_set) / s->n_cps directly, so 7d's
// -DATP_FV_INDEX must be resynced before the check: the FV index is
// maintained incrementally on heap push/pop, so a test that pokes
// the arrays directly must reheapify to rebuild it.  Off the flag
// this is a thin pass-through to the array scan.
static int tt_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
#if defined(ATP_FV_INDEX)
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

  TEST_BEGIN("atp/wm-preset-fifo-dimension-off");
  {
    // WM's default proof config (the one wmcli runs) carries no
    // `-pq interleave=` token, so PI_ParseInterleave fails and the
    // CP-queue uses moduloCP=1, thresholdCP=0 (KPVerwaltung.c:1216-1219):
    // CPdimension() == AnzAktivierterRE % 1 < 0 is FALSE always -- WM
    // NEVER takes a FIFO pick.  Under use_wm_intake_order thvm mirrors
    // this: the queue is a pure smallest-weight heap, so the heavy
    // oldest CP is deferred to LAST, never surfaced at a modulo window
    // (contrast atp/select-cp-fifo-interleave, the legacy non-WM path,
    // which DOES surface it at selection 11).  Same fixture as that
    // test; here the FIFO dimension stays off for all 12 selections.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_intake_order(s, 1u);
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
    // All 11 light CPs pop first (pure weight); the heavy oldest CP is
    // last -- the FIFO dimension never fires at selection 11.
    for (u32 i = 0; i < 11; i++) {
      CHECK(thvm_atp_select_cp(s, &lo, &ro));
      CHECK_EQ(term_ext(lo), LAB_a);
    }
    CHECK(thvm_atp_select_cp(s, &lo, &ro));
    CHECK_EQ(term_tag(lo), TAG_CTR);
    CHECK_EQ(term_ext(lo), LAB_f);                 // heavy CP only now
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

#ifdef ATP_ORDERED_REWRITE
  TEST_BEGIN("atp/ordered-rewrite-free-var-instance");
  {
    // WM `RechtsUnfrei` ordered rewriting (GleichungsrichtungPasst,
    // INF/MatchOperationen.c:923): an unorientable equation whose
    // replacement side carries EXTENSION variables fires with the
    // grounded instance (extras -> s->min_const) under the strict
    // KBO_GT gate -- atp_unorient_template.  f(x,x) == f(y,y) is
    // KBO_UN with extras both ways; the subject f(f(v,v), f(v,v))
    // hosts a strictly-decreasing grounded root instance f(c,c)
    // (weight 7 -> 3, ground), which the old vars-contained guard
    // skipped entirely (NF == subject).  Tree and flatterm paths
    // must agree, and the grounded NF is a fixpoint (f(c,c) -> f(c,c)
    // is not a strict decrease).
    static u32 fviw[2] = {0u, 1u};
    static u32 fvip[2] = {0u, 1u};
    static const KboConfig FVI_CFG = {
      .weights = fviw, .precedence = fvip, .n_labels = 2u, .var_weight = 1u,
    };
    #define FVI_F(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(1u, _c, 2); })
    AtpState *s = thvm_atp_init(&FVI_CFG, 64u);
    {
      Term x = mk_v(0u), y = mk_v(1u);
      atp_push_rule(s, FVI_F(x, x), FVI_F(y, y));   // unorientable
    }
    CHECK_EQ(s->n_unorient, 1u);
    Term v    = mk_v(0u);
    Term subj = FVI_F(FVI_F(v, v), FVI_F(v, v));
    Term want = FVI_F(s->min_const, s->min_const);
    Term nf_tree = atp_rewrite_normalize(s, subj, s->lhs, s->rhs,
                                         s->n_rules, 64u);
    CHECK(kbo_eq(nf_tree, want));
    s->use_flatterm = 1u;
    Term nf_flat = atp_rewrite_normalize(s, subj, s->lhs, s->rhs,
                                         s->n_rules, 64u);
    s->use_flatterm = 0u;
    CHECK(kbo_eq(nf_flat, want));
    Term nf2 = atp_rewrite_normalize(s, nf_tree, s->lhs, s->rhs,
                                     s->n_rules, 64u);
    CHECK(kbo_eq(nf2, want));
    thvm_atp_free(s);
    #undef FVI_F
  }

  TEST_BEGIN("atp/generation-doE-false-no-equation-join-at-push");
  {
    // WM per-site NF flags: the generation-time CP treatment
    // (KPBehandelt, `-kg` default "r" -> doE=FALSE) must NOT rewrite
    // with unorientable equations or their grounded instances; the
    // selection-/goal-time flag pair (`-ks` "r:e:s:p" -> doE=TRUE)
    // must.  Same FVI fixture as above: (f(f(v,v),f(v,v)), f(c,c))
    // joins ONLY through the grounded f(x,x)==f(y,y) instance, so
    // push-time joinability says NO and returns the sides
    // equation-unreduced, while the doE normalize joins the pair.
    static u32 fviw[2] = {0u, 1u};
    static u32 fvip[2] = {0u, 1u};
    static const KboConfig FVI_CFG = {
      .weights = fviw, .precedence = fvip, .n_labels = 2u, .var_weight = 1u,
    };
    #define FVI_F(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(1u, _c, 2); })
    AtpState *s = thvm_atp_init(&FVI_CFG, 64u);
    {
      Term x = mk_v(0u), y = mk_v(1u);
      atp_push_rule(s, FVI_F(x, x), FVI_F(y, y));   // unorientable
    }
    CHECK_EQ(s->n_unorient, 1u);
    Term v  = mk_v(0u);
    Term cl = FVI_F(FVI_F(v, v), FVI_F(v, v));
    Term cr = FVI_F(s->min_const, s->min_const);
    Term jl = cl, jr = cr;
    CHECK_EQ((int)atp_cp_trivially_joinable(s, &jl, &jr), 0);
    CHECK(kbo_eq(jl, cl));
    CHECK(kbo_eq(jr, cr));
    Term nl = atp_rewrite_normalize(s, cl, s->lhs, s->rhs, s->n_rules, 64u);
    Term nr = atp_rewrite_normalize(s, cr, s->lhs, s->rhs, s->n_rules, 64u);
    CHECK(kbo_eq(nl, nr));
    thvm_atp_free(s);
    #undef FVI_F
  }

  TEST_BEGIN("atp/redex-priority-rule-before-equation-per-position");
  {
    // WM per-position redex priority (BL_RegelOderGleichungAngewendet,
    // NF/NFBildung.c:503-531): where BOTH an oriented rule and an
    // unorientable equation match the same position, the rule fires.
    // Slot 0 is the commutativity equation f(x,y) == f(y,x) (KBO_UN);
    // slot 1 is the oriented rule f(e,a) -> a.  The subject f(e,a)
    // hosts both at the root -- the equation's forward face passes the
    // strict-decrease gate (e > a in DUMMY_CFG precedence, so
    // f(e,a) > f(a,e)).  Rule-index order would fire the equation
    // first and stall at the f(a,e) fixpoint; the rule tree
    // (Regelbaum) goes first and reaches a.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64u);
    {
      Term x = mk_v(0u), y = mk_v(1u);
      atp_push_rule(s, mk_f(x, y), mk_f(y, x));     // unorientable, slot 0
    }
    atp_push_rule(s, mk_f(mk_e(), mk_a()), mk_a()); // oriented, slot 1
    CHECK_EQ(s->n_rules, 2u);
    CHECK_EQ(s->n_unorient, 1u);
    CHECK_EQ(s->r_orient[0], 0u);
    CHECK_EQ(s->r_orient[1], 1u);
    Term subj = mk_f(mk_e(), mk_a());
    Term want = mk_a();
    // Term-side single-position pick: the rule (slot 1) beats the
    // equation (slot 0).
    u8 fired = 0;
    Term top = atp_ordered_try_top(s, subj, s->lhs, s->rhs, s->n_rules,
                                   &fired);
    CHECK_EQ(fired, 1u);
    CHECK(kbo_eq(top, want));
    // Full Term-side normalize agrees.
    Term nf = atp_rewrite_normalize(s, subj, s->lhs, s->rhs, s->n_rules, 64u);
    CHECK(kbo_eq(nf, want));
#ifdef THVM_ATPFT_NORM
    // FT normalize (find_redex_ft) applies the same per-position
    // priority.
    AtpFtCell *fsubj = ft_from_term((AtpFt *)s->ft_arena_ptr, subj, 0);
    AtpFtCell *fnf   = atp_rewrite_normalize_ft(s, fsubj, 64u);
    CHECK(kbo_eq(ft_to_term(fnf), want));
#endif
    thvm_atp_free(s);
  }
#endif

  TEST_BEGIN("atp/generate-cps-empty-added-no-op");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    AtpAddedRange empty = {0, 0, 0};
    u32 pushed = thvm_atp_generate_cps(s, empty);
    CHECK_EQ(pushed, 0u);
    CHECK_EQ(s->n_cps, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/generate-cps-single-rule-self-overlap");
  {
    // Add one rule via orient_and_add, then generate_cps.  Since R
    // contains only the new rule, the only enumeration is the 1x1
    // self-overlap.  WM never forms a rule's root self-overlap
    // (U1_KPsBildenZuRegel passes the rule itself as the toplevel
    // Ausschluss = exclusion object, Unifikation1.c:1480-1530), and
    // f(x, e)'s only proper non-var position is the constant e,
    // which cannot unify with the f-rooted LHS -- so NOTHING is
    // built: no push, no joinability drop.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(added.count, 1u);
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK_EQ(pushed, 0u);
    CHECK_EQ(s->n_cps, 0u);
    CHECK_EQ(s->n_cps_dropped_joinable, 0u);
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
    AtpAddedRange empty = {0, 0, 0};
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

  TEST_BEGIN("atp/wm-orphan-layout-lazy-pop-eager-sweep-gated");
  {
    // WM's orphan layout (-ocrit; KPVerwaltung.c:535-556 selectNonOrphan,
    // NULL parent = alive): the ONLY orphan mechanism is the lazy at-pop
    // discard -- WM never sweeps the queue when interreduction drops a
    // rule.  thvm's eager interreduce-time sweep (ATP_ORPHAN_KILL) is an
    // extra that changes live-queue composition, so it is runtime-gated:
    // default ON (legacy), Method->"Waldmeister" turns it OFF + lazy ON.
    // Build the same scenario twice -- a queued CP whose parent rule is
    // interreduced away -- and check both layouts.

    // --- Legacy layout (defaults: eager sweep ON, lazy discard OFF) ---
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->use_eager_orphan_sweep, 1u);
    CHECK_EQ(s->use_orphan_murder, 0u);

    // R[0] via the saturation path so it carries a real trace lineage.
    thvm_atp_add_equation(s, mk_f(mk_a(), mk_e()), mk_f(mk_a(), mk_a()));
    thvm_atp_step(s);
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(s->n_cps, 0u);
    u32 doomed = s->r_trace[0];
    // A queued CP parented on the doomed rule.  Raw-NF sides (no f(_,e)
    // redex, not joinable) so only the orphan paths can drop it.
    Term c_l = mk_i(mk_a()), c_r = mk_a();
    u32 tc = atp_trace_push_cp(s, doomed, doomed, c_l, c_r, NULL, 0);
    atp_cp_heap_push(s, c_l, c_r, tc, 0u, 0u);
    CHECK_EQ(s->n_cps, 1u);

    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    CHECK_EQ(thvm_atp_interreduce(s, added), 1u);
#ifdef ATP_ORPHAN_KILL
    // The eager sweep ran: the orphan is gone; only the requeued
    // TRACE_SIMPLIFY equation remains.
    CHECK_EQ(s->n_cps, 1u);
    CHECK(s->cp_trace[0] != tc);
#endif
    thvm_atp_free(s);

    // --- WM layout (lazy at-pop ON, eager sweep OFF) ---
    s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_orphan_murder(s, 1u);
    thvm_atp_set_use_eager_orphan_sweep(s, 0u);

    thvm_atp_add_equation(s, mk_f(mk_a(), mk_e()), mk_f(mk_a(), mk_a()));
    thvm_atp_step(s);
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(s->n_cps, 0u);
    doomed = s->r_trace[0];
    c_l = mk_i(mk_a()); c_r = mk_a();
    tc = atp_trace_push_cp(s, doomed, doomed, c_l, c_r, NULL, 0);
    atp_cp_heap_push(s, c_l, c_r, tc, 0u, 0u);
    CHECK_EQ(s->n_cps, 1u);

    added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    CHECK_EQ(thvm_atp_interreduce(s, added), 1u);
    // No sweep: the orphan is STILL queued next to the requeued victim;
    // the dropped rule's birthing trace id is marked dead instead.
    CHECK_EQ(s->n_cps, 2u);
    CHECK(atp_trace_is_dead(s, doomed));
    CHECK(atp_cp_is_orphan(s, tc));
    // Requeued-victim exemption: the victim's TRACE_SIMPLIFY entry
    // parents on the DEAD rule's trace, yet it is never orphaned
    // (atp_cp_is_orphan only tests TRACE_CP entries -- WM's NULL-parent
    // = alive convention for requeued IR victims).
    u32 t_victim = (s->cp_trace[0] == tc) ? s->cp_trace[1] : s->cp_trace[0];
    CHECK_EQ(term_ext(s->trace[t_victim]), TRACE_SIMPLIFY);
    CHECK_EQ((u32)term_val(term_ctr_at(s->trace[t_victim], 0)), doomed);
    CHECK(!atp_cp_is_orphan(s, t_victim));
    // Drain: the victim pops live; the orphan dies FOR FREE at pop.
    u32 orphans0 = s->n_cps_dropped_orphan;
    Term pl = 0, pr = 0;
    CHECK_EQ(thvm_atp_select_cp(s, &pl, &pr), 1u);
    CHECK_EQ(s->last_popped_trace, t_victim);
    CHECK_EQ(thvm_atp_select_cp(s, &pl, &pr), 0u);
    CHECK_EQ(s->n_cps, 0u);
    CHECK_EQ(s->n_cps_dropped_orphan - orphans0, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-kpbehandelt-raw50-queued-raw-weighed-raw");
  {
    // WM KPBehandelt (INF/KPVerwaltung.c:439-467): a CP at-or-above the
    // lohntSichBehandlung gate (:435-438, combined RAW size >= 50) gets
    // NO generation-time treatment -- it queues RAW, weighs on the RAW
    // pair (recentCPinsert C_Classify :396), and bypasses the
    // auto-MaxWeight stash (WM buries it IN the heap at raw weight);
    // the full normalize + joinability verdict run at selection.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_lazy_normalize(s, 1u);

    // R[0]: f(x, e) -> x via the saturation path (real trace lineage).
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_step(s);
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(s->n_cps, 0u);
    u32 rt = s->r_trace[0];
    // Tight auto-MaxWeight: bound = 2 + 2 * |f(x, e)| = 8, far below the
    // raw CP's 54 nodes -- any treated CP this size would be stashed.
    thvm_atp_set_auto_max_cp_weight(s, 2u);
    atp_auto_maxw_recompute(s);

    // Raw pair: l = i^24(f(a, e)) (27 symbols, f-redex under R[0]),
    // r = i^26(a) (27 symbols).  Combined 54 >= 50; not joinable
    // (l normalizes to i^24(a) != i^26(a)).
    Term l = mk_f(mk_a(), mk_e());
    for (u32 k = 0; k < 24; k++) l = mk_i(l);
    Term r = mk_a();
    for (u32 k = 0; k < 26; k++) r = mk_i(r);
    u64 push_norms0 = s->n_cps_push_normalized;
    CriticalPair cp = {0};
    cp.lhs = l;
    cp.rhs = r;
    CHECK_EQ(atp_push_cps_traced(s, &cp, 1u, rt, rt, 0u, 0u), 1u);

    // Queued RAW in the HEAP (not the stash), untreated (no push
    // normalize ran), sides byte-identical to the raw pair, priority
    // computed on the raw 54-node form.
    CHECK_EQ(s->n_cps, 1u);
    CHECK_EQ(s->n_cp_stash, 0u);
    CHECK_EQ(s->n_cps_push_normalized, push_norms0);
    Term ql = 0, qr = 0;
    acp_unpack(s->cp_packed[0], &ql, &qr);
    CHECK(kbo_eq(ql, l));
    CHECK(kbo_eq(qr, r));
    CHECK_EQ(atp_symbol_count(ql) + atp_symbol_count(qr), 54u);
    CHECK_EQ(s->cp_pri[0], atp_cp_priority_sized(s, l, r, 54u));

    // Pop treatment (KPV_Select -ks): the raw CP is normalized at
    // selection -- the f(a, e) redex reduces away -- then orients into
    // i^26(a) -> i^24(a).
    thvm_atp_step(s);
    CHECK_EQ(s->n_rules, 2u);
    CHECK_EQ(term_ext(s->lhs[1]), LAB_i);
    CHECK_EQ(atp_symbol_count(s->lhs[1]), 27u);
    CHECK_EQ(atp_symbol_count(s->rhs[1]), 25u);
    thvm_atp_free(s);

    // --- <50 class: treated exactly as before (doR-only normalize +
    //     joined-drop at push, queue + weigh on the treated form) ---
    s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_lazy_normalize(s, 1u);
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_step(s);
    rt = s->r_trace[0];
    push_norms0 = s->n_cps_push_normalized;
    u64 joined0 = s->n_cps_dropped_joinable;
    // Joinable small pair: f(a, e) = a joins under R[0] -> push-drop.
    CriticalPair cp_small = {0};
    cp_small.lhs = mk_f(mk_a(), mk_e());
    cp_small.rhs = mk_a();
    atp_push_cps_traced(s, &cp_small, 1u, rt, rt, 0u, 0u);
    CHECK_EQ(s->n_cps, 0u);
    CHECK_EQ(s->n_cps_dropped_joinable - joined0, 1u);
    CHECK_EQ(s->n_cps_push_normalized - push_norms0, 1u);
    // Surviving small pair queues its TREATED form: f(i(a), e) = e
    // reduces to (i(a), e) before it lands in the queue.
    cp_small.lhs = mk_f(mk_i(mk_a()), mk_e());
    cp_small.rhs = mk_e();
    atp_push_cps_traced(s, &cp_small, 1u, rt, rt, 0u, 0u);
    CHECK_EQ(s->n_cps, 1u);
    acp_unpack(s->cp_packed[0], &ql, &qr);
    CHECK(kbo_eq(ql, mk_i(mk_a())));
    CHECK(kbo_eq(qr, mk_e()));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-pop-subsume-drops-unorientable-e-instance");
  {
    // WM -ks "s" stage (KPV_Select, INF/KPVerwaltung.c:667
    // SS_TermpaarSubsummiertVonGM): a popped pair that normalizes
    // UNORIENTABLE and is an instance of an existing unorientable
    // equation -- at the top or down the unique differing-subterm
    // path, in either orientation, one substitution over both sides
    // -- drops before orientation.  E = {f(x,y) = f(y,x)} (KBO_UN:
    // equal weights, var-vs-var first argument), and the proper
    // instance sigma = {x -> i(x), y -> z} stays KBO_UN, so it
    // reaches the stage without joining at the selection normalize.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_pop_subsume(s, 1u);
    CHECK_EQ(s->use_pop_subsume, 1u);
    atp_push_rule(s, mk_f(mk_v(VAR_x), mk_v(1u)), mk_f(mk_v(1u), mk_v(VAR_x)));
    CHECK_EQ(s->n_unorient, 1u);
    CHECK_EQ(s->r_orient[0], 0u);

    // Direct match semantics.  A proper instance (sigma x -> i(x)).
    Term inst_l = mk_f(mk_i(mk_v(VAR_x)), mk_v(2u));
    Term inst_r = mk_f(mk_v(2u), mk_i(mk_v(VAR_x)));
    CHECK_EQ((int)atp_pop_eq_subsumed(s, inst_l, inst_r), 1);
    // Reversed pair: the Antigleichung orientation also subsumes.
    CHECK_EQ((int)atp_pop_eq_subsumed(s, inst_r, inst_l), 1);
    // Position descent: the instance one level down under i(.).
    CHECK_EQ((int)atp_pop_eq_subsumed(s, mk_i(inst_l), mk_i(inst_r)), 1);
    // TWO differing children: SubsumptionBody's context-stripping
    // refuses (NachfolgendeTeiltermeGleich), and the top match fails
    // (the one substitution cannot cover both sides) -- not subsumed.
    CHECK_EQ((int)atp_pop_eq_subsumed(s,
        mk_f(mk_i(mk_v(VAR_x)), mk_v(1u)),
        mk_f(mk_i(mk_v(1u)), mk_v(VAR_x))), 0);
    // Rules never subsume at this stage (WM walks RE_Gleichungsbaum
    // only): an instance of an ORIENTED rule is not dropped here.
    atp_push_rule(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    CHECK_EQ(s->r_orient[s->n_rules - 1u], 1u);
    CHECK_EQ((int)atp_pop_eq_subsumed(s,
        mk_f(mk_i(mk_v(VAR_x)), mk_e()), mk_i(mk_v(VAR_x))), 0);

    // Through thvm_atp_step: the instance pops, normalizes unchanged
    // (the ordered-rewrite gate cannot order it), compares
    // Unvergleichbar, and drops -- no rule added.
    u32 tc = atp_trace_push_cp(s, ATP_TRACE_NONE, ATP_TRACE_NONE,
                               inst_l, inst_r, NULL, 0);
    atp_cp_heap_push(s, inst_l, inst_r, tc, 0u, 0u);
    CHECK_EQ(s->n_cps, 1u);
    u32 rules0 = s->n_rules;
    thvm_atp_step(s);
    CHECK_EQ(s->n_cps_dropped_pop_subsumed, 1u);
    CHECK_EQ(s->n_rules, rules0);
    // Same drop one level down the differing-subterm path.
    Term deep_l = mk_i(inst_l);
    Term deep_r = mk_i(inst_r);
    tc = atp_trace_push_cp(s, ATP_TRACE_NONE, ATP_TRACE_NONE,
                           deep_l, deep_r, NULL, 0);
    atp_cp_heap_push(s, deep_l, deep_r, tc, 0u, 0u);
    thvm_atp_step(s);
    CHECK_EQ(s->n_cps_dropped_pop_subsumed, 2u);
    CHECK_EQ(s->n_rules, rules0);
    // An ORIENTABLE pair is NOT tested -- WM gates the stage on
    // Unvergleichbar -- and orients into a rule as before.  (An
    // orientable proper instance of the equation cannot reach orient:
    // the selection normalize's ordered rewriting joins it, in thvm
    // and WM alike, so a ground non-instance pair is the probe.)
    rules0 = s->n_rules;
    Term ori_l = mk_f(mk_a(), mk_a());
    Term ori_r = mk_a();
    tc = atp_trace_push_cp(s, ATP_TRACE_NONE, ATP_TRACE_NONE,
                           ori_l, ori_r, NULL, 0);
    atp_cp_heap_push(s, ori_l, ori_r, tc, 0u, 0u);
    thvm_atp_step(s);
    CHECK_EQ(s->n_cps_dropped_pop_subsumed, 2u);
    CHECK_EQ(s->n_rules, rules0 + 1u);
    CHECK_EQ(s->r_orient[rules0], 1u);
    thvm_atp_free(s);

    // A NON-subsumed unorientable pair survives to orientation
    // (fresh state so the pop is deterministic).
    s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_pop_subsume(s, 1u);
    atp_push_rule(s, mk_f(mk_v(VAR_x), mk_v(1u)), mk_f(mk_v(1u), mk_v(VAR_x)));
    Term sur_l = mk_f(mk_v(VAR_x), mk_i(mk_v(1u)));
    Term sur_r = mk_f(mk_v(1u), mk_i(mk_v(VAR_x)));
    tc = atp_trace_push_cp(s, ATP_TRACE_NONE, ATP_TRACE_NONE,
                           sur_l, sur_r, NULL, 0);
    atp_cp_heap_push(s, sur_l, sur_r, tc, 0u, 0u);
    u32 unorient0 = s->n_unorient;
    thvm_atp_step(s);
    CHECK_EQ(s->n_cps_dropped_pop_subsumed, 0u);
    CHECK(s->n_unorient > unorient0);
    thvm_atp_free(s);

    // Default OFF: the same instance pop is NOT dropped
    // (byte-identical legacy path) -- it orients into the unfailing
    // equation slots.
    s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->use_pop_subsume, 0u);
    atp_push_rule(s, mk_f(mk_v(VAR_x), mk_v(1u)), mk_f(mk_v(1u), mk_v(VAR_x)));
    Term off_l = mk_f(mk_i(mk_v(VAR_x)), mk_v(2u));
    Term off_r = mk_f(mk_v(2u), mk_i(mk_v(VAR_x)));
    tc = atp_trace_push_cp(s, ATP_TRACE_NONE, ATP_TRACE_NONE,
                           off_l, off_r, NULL, 0);
    atp_cp_heap_push(s, off_l, off_r, tc, 0u, 0u);
    rules0 = s->n_rules;
    thvm_atp_step(s);
    CHECK_EQ(s->n_cps_dropped_pop_subsumed, 0u);
    CHECK(s->n_rules > rules0);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-eset-subsume-destroys-subsumed-e-on-entry");
  {
    // WM E-set subsumption on new-equation entry
    // (GMSubsummierenMitGleichung, INF/Interreduktion.c:251-274,
    // reached from IR_InterreduktionLinks :371-373 BEFORE the new
    // equation enters GM): every existing E-member the new
    // unorientable equation subsumes (SS_TermpaarSubsummiertTermpaar,
    // Subsumption.c:104-110 -- one substitution over both sides,
    // either pattern orientation, context-stripping descent) is
    // removed AND destroyed (FinaleKillprozSubsumption :236-245),
    // twin included -- NO requeue, no CP made.  Twin semantics are
    // N/A in thvm's single-slot storage: WM keeps each equation as
    // Gleichung + Antigleichung twins and kills both; thvm stores
    // one slot per equation under ATP_ORDERED_REWRITE, so one
    // soft-delete covers both directions.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_eset_subsume(s, 1u);
    CHECK_EQ(s->use_eset_subsume, 1u);
    // E0 (will die): a proper instance f(i(x), y) = f(y, i(x)) of the
    // upcoming general equation.  KBO_UN: equal weights, i(x)-vs-y
    // first children incomparable.
    atp_push_rule(s, mk_f(mk_i(mk_v(VAR_x)), mk_v(1u)),
                     mk_f(mk_v(1u), mk_i(mk_v(VAR_x))));
    CHECK_EQ(s->r_orient[0], 0u);
    // E1 (will die via descent): the same instance one level down
    // under i(.) -- subsumed only through SubsumptionBody's
    // context-stripping.
    atp_push_rule(s, mk_i(mk_f(mk_i(mk_v(VAR_x)), mk_v(1u))),
                     mk_i(mk_f(mk_v(1u), mk_i(mk_v(VAR_x)))));
    CHECK_EQ(s->r_orient[1], 0u);
    // E2 (survives): f(x, i(y)) = f(y, i(x)) -- not an instance of
    // commutativity in either orientation, and the descent gate
    // refuses (TWO differing children).
    atp_push_rule(s, mk_f(mk_v(VAR_x), mk_i(mk_v(1u))),
                     mk_f(mk_v(1u), mk_i(mk_v(VAR_x))));
    CHECK_EQ(s->r_orient[2], 0u);
    // R3 (survives): rules never participate -- WM's sweep walks
    // RE_forGleichungenRobust (the Gleichungsmenge) only.  f(x,e) -> x
    // is itself an "instance shape" the matcher would hit if rules
    // were scanned.
    atp_push_rule(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    CHECK_EQ(s->r_orient[3], 1u);

    // The new GENERAL unorientable equation enters: f(x,y) = f(y,x).
    u32 cps0 = s->n_cps;
    u32 trace0 = s->n_trace;
    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_v(1u)), mk_f(mk_v(1u), mk_v(VAR_x)));
    CHECK_EQ(added.count, 1u);
    u32 new_i = added.first;
    // E0 + E1 soft-deleted: dead bit set, sentinel faces, originals in
    // the save slots (proof reconstruction reads them).
    CHECK_EQ(s->n_eqs_dropped_eset_subsumed, 2u);
    CHECK_EQ((u32)s->r_dead[0], 1u);
    CHECK_EQ((u32)s->r_dead[1], 1u);
    CHECK_EQ(term_tag(s->lhs[0]), TAG_FVR);
    CHECK(kbo_eq(s->r_dead_lhs_save[0],
                 mk_f(mk_i(mk_v(VAR_x)), mk_v(1u))));
    // NO requeue, no CP made: the queue and the trace are untouched
    // (WM's FinaleKillprozSubsumption only destroys).
    CHECK_EQ(s->n_cps, cps0);
    CHECK_EQ(s->n_trace, trace0);
    // E2 and the rule R3 survive; the new equation itself is alive
    // (WM sweeps before insertion, so it never subsumes itself).
    CHECK_EQ((u32)s->r_dead[2], 0u);
    CHECK_EQ((u32)s->r_dead[3], 0u);
    CHECK_EQ((u32)s->r_dead[new_i], 0u);
    // Directionality: a MORE SPECIFIC late arrival never kills the
    // general member.  i(f(x,y)) = i(f(y,x)) enters; the live general
    // equation at new_i is not its instance.
    u32 killed0 = s->n_eqs_dropped_eset_subsumed;
    thvm_atp_orient_and_add(s, mk_i(mk_f(mk_v(VAR_x), mk_v(1u))),
                               mk_i(mk_f(mk_v(1u), mk_v(VAR_x))));
    CHECK_EQ(s->n_eqs_dropped_eset_subsumed, killed0);
    CHECK_EQ((u32)s->r_dead[new_i], 0u);
    thvm_atp_free(s);

    // Default OFF: same scenario, the instance member survives the
    // general equation's entry (engine byte-identical).
    s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->use_eset_subsume, 0u);
    atp_push_rule(s, mk_f(mk_i(mk_v(VAR_x)), mk_v(1u)),
                     mk_f(mk_v(1u), mk_i(mk_v(VAR_x))));
    thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_v(1u)), mk_f(mk_v(1u), mk_v(VAR_x)));
    CHECK_EQ(s->n_eqs_dropped_eset_subsumed, 0u);
    CHECK_EQ((u32)s->r_dead[0], 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-demote-equation-victim-requeues-original-sides");
  {
    // Waldmeister KPV_IROpferBehandeln (KPVerwaltung.c:517-518): an
    // interreduction victim re-enters the queue with its ORIGINAL
    // sides, not the slice-reduced form.  Trigger the demotion with
    // an unorientable EQUATION (f(x,a) = f(a,x), KBO_UN under
    // DUMMY_CFG) firing on the rule's LHS under the ordered
    // strict-decrease gate -- and since the drain's KPBehandelt
    // treatment is doR (oriented rules only, `-kg r`), the equation
    // does NOT renormalize the victim at requeue: the sides re-enter
    // intact.  This is the McCune-II `ues 32` shape.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_demote(s, 1u);

    // R[0]: i(f(e,a)) -> e.  The redex f(e,a) rewrites by the
    // equation's l->r direction (sigma x = e; f(e,a) > f(a,e) by the
    // lex child compare under e > a precedence).
    Term victim_lhs = mk_i(mk_f(mk_e(), mk_a()));
    Term victim_rhs = mk_e();
    s->lhs[0] = victim_lhs;
    s->rhs[0] = victim_rhs;
    s->r_orient[0] = 1u;
    s->n_rules = 1;
    u32 n_cps_before = s->n_cps;

    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_a()), mk_f(mk_a(), mk_v(VAR_x)));
    CHECK_EQ(added.count, 1u);
    CHECK_EQ(s->r_orient[added.first], 0u);   // stored unorientable

    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 1u);
    // The victim is BUFFERED, not queued: WM's IR buffer drains only
    // after CP generation (IR_PufferAuslesen is the last queue
    // mutation of the work order).
    CHECK_EQ(s->n_cps, n_cps_before);
    CHECK_EQ(s->n_irv, 1u);
    CHECK(kbo_eq(s->irv_lhs[0], victim_lhs));
    CHECK(kbo_eq(s->irv_rhs[0], victim_rhs));

    u32 trace_before = s->n_trace;
    atp_wm_demote_drain(s);
    CHECK_EQ(s->n_irv, 0u);
    CHECK_EQ((u32)s->n_wm_demote_requeued, 1u);
    CHECK_EQ(s->n_cps, n_cps_before + 1u);
    // The TRACE_SIMPLIFY records the ORIGINAL pair: doR leaves the
    // equation-reducible LHS untouched (the legacy path would have
    // queued the slice-reduced i(f(a,e)) instead).
    Term simp = s->trace[trace_before];
    CHECK_EQ(term_ext(simp), TRACE_SIMPLIFY);
    CHECK(kbo_eq(term_ctr_at(simp, 2),
                 tt_norm_lhs(victim_lhs, victim_rhs)));
    CHECK(kbo_eq(term_ctr_at(simp, 3),
                 tt_norm_rhs(victim_lhs, victim_rhs)));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-demote-drain-discards-joined-victim");
  {
    // WM KPBehandelt (KPVerwaltung.c:443-446): a victim whose sides
    // join after the rules-only renormalize is discarded outright --
    // it never re-enters the queue.  R[0] = f(a,e) -> a collapses
    // under the more-general f(x,e) -> x to the trivial a = a.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_demote(s, 1u);

    s->lhs[0] = mk_f(mk_a(), mk_e());
    s->rhs[0] = mk_a();
    s->r_orient[0] = 1u;
    s->n_rules = 1;
    u32 n_cps_before = s->n_cps;

    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    CHECK_EQ(added.count, 1u);
    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 1u);
    CHECK_EQ(s->n_irv, 1u);

    u32 trace_before = s->n_trace;
    atp_wm_demote_drain(s);
    CHECK_EQ(s->n_irv, 0u);
    CHECK_EQ((u32)s->n_wm_demote_joined, 1u);
    CHECK_EQ((u32)s->n_wm_demote_requeued, 0u);
    CHECK_EQ(s->n_cps, n_cps_before);      // nothing re-queued
    CHECK_EQ(s->n_trace, trace_before);    // no TRACE_SIMPLIFY pushed
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-demote-step-drains-after-cp-gen");
  {
    // Step-level work order (WM ArbeitsAufnahme, Hauptkomponenten.c:
    // 308-331): the victim re-enters the queue AFTER the new fact's
    // CPs, so its FIFO age (cp_seq) is the youngest in the queue.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_f(mk_a(), mk_e()), mk_f(mk_a(), mk_a()));
    thvm_atp_step(s);
    CHECK_EQ(s->n_rules, 1u);

    thvm_atp_set_use_wm_demote(s, 1u);
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_step(s);
    // The step demoted R[0], generated the new rule's CPs, then
    // drained the buffer: the treated victim (a, f(a,a)) is queued.
    CHECK_EQ((u32)s->n_wm_demote_requeued, 1u);
    CHECK_EQ(s->n_irv, 0u);
    CHECK(s->n_cps >= 1u);
    // The youngest queue entry (max cp_seq) is the drained victim --
    // its trace entry is the TRACE_SIMPLIFY the drain pushed.
    u32 young = 0u;
    for (u32 k = 1; k < s->n_cps; k++) {
      if (s->cp_seq[k] > s->cp_seq[young]) young = k;
    }
    CHECK_EQ(term_ext(s->trace[s->cp_trace[young]]), TRACE_SIMPLIFY);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-rhs-interreduce-full-system-in-place-modify");
  {
    // WM RMRechtsInterred under the -irrp default FALSE = "modify rule"
    // (Interreduktion.c:329-360; Parameter.c:337-343): when the NEW
    // object applies anywhere in an oriented rule's RHS, that RHS goes
    // to FULL R+E normal form IN PLACE -- including reductions by OLDER
    // rules the new-object slice cannot see -- with no drop, no requeue
    // and no CP.  R0 = f(a,a) -> a is the full-system-only reducer: the
    // new rule f(e,e) -> f(a,a) steps the victim's RHS i(f(e,e)) to
    // i(f(a,a)), and only the full NF (NF_NormalformRE, doR+doE,
    // NFBildung.h:78) carries it on to i(a).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_rhs_interreduce(s, 1u);
    s->lhs[0] = mk_f(mk_a(), mk_a());
    s->rhs[0] = mk_a();
    s->r_orient[0] = 1u;
    Term victim_lhs = mk_f(mk_f(mk_e(), mk_a()), mk_a());
    s->lhs[1] = victim_lhs;
    s->rhs[1] = mk_i(mk_f(mk_e(), mk_e()));
    s->r_orient[1] = 1u;
    s->n_rules = 2;

    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_e(), mk_e()), mk_f(mk_a(), mk_a()));
    CHECK_EQ(added.count, 1u);
    u32 n_cps_before = s->n_cps;
    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 0u);                  // modify, never delete
    CHECK_EQ(s->n_rules, 3u);
    CHECK(kbo_eq(s->lhs[1], victim_lhs));   // rule stays in its slot
    CHECK(kbo_eq(s->rhs[1], mk_i(mk_a()))); // full-system NF, not the
                                            // slice reduct i(f(a,a))
    CHECK_EQ((u32)s->n_right_reduced, 1u);
    CHECK_EQ(s->n_cps, n_cps_before);       // no requeue, no CP
    // The repointed trace entry is a TRACE_ORIENT carrying the live
    // (l, r') pair, so resolveRule(1) reads the composed rule.
    Term ent = s->trace[s->r_trace[1]];
    CHECK_EQ(term_ext(ent), TRACE_ORIENT);
    CHECK(kbo_eq(term_ctr_at(ent, 2), victim_lhs));
    CHECK(kbo_eq(term_ctr_at(ent, 3), mk_i(mk_a())));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-rhs-interreduce-no-symbol-count-guard");
  {
    // WM commits the NF_NormalformRE result unconditionally -- there is
    // no symbol-count guard in RMRechtsInterred (Interreduktion.c:
    // 341/352 just overwrite the RHS).  f(e,e) -> i(i(i(a))) grows the
    // victim's RHS from 3 to 4 symbols (i has KBO weight 0, so the NF
    // is order-smaller yet symbol-larger).  The legacy slice compose
    // (use_rhs_interreduce OFF) blocks exactly this case.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term victim_lhs = mk_f(mk_f(mk_a(), mk_a()), mk_a());
    Term victim_rhs = mk_f(mk_e(), mk_e());
    s->lhs[0] = victim_lhs;
    s->rhs[0] = victim_rhs;
    s->r_orient[0] = 1u;
    s->n_rules = 1;
    Term grown = mk_i(mk_i(mk_i(mk_a())));
    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_e(), mk_e()), grown);
    CHECK_EQ(added.count, 1u);
    // Default (legacy) mode: the symbol-count guard keeps the compact
    // RHS; nothing is dropped or edited.
    CHECK_EQ(thvm_atp_interreduce(s, added), 0u);
    CHECK(kbo_eq(s->rhs[0], victim_rhs));
    CHECK_EQ((u32)s->n_right_reduced, 0u);
    // WM mode: same interreduction now commits the grown NF in place
    // and still never drops the rule.
    thvm_atp_set_use_rhs_interreduce(s, 1u);
    u32 n_cps_before = s->n_cps;
    CHECK_EQ(thvm_atp_interreduce(s, added), 0u);
    CHECK_EQ(s->n_rules, 2u);
    CHECK(kbo_eq(s->rhs[0], grown));
    CHECK_EQ((u32)s->n_right_reduced, 1u);
    CHECK_EQ(s->n_cps, n_cps_before);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-rhs-interreduce-eset-rhs-face-requeues");
  {
    // The drop-and-requeue half of the RHS check is GMInterred
    // (Interreduktion.c:280-293), which walks every E-member's directed
    // twins: an UNORIENTABLE equation with a reducible RHS face leaves
    // E and re-enters the queue.  Oriented rules never take this path
    // (-irrp default FALSE; scenarios above), so the E-member is the
    // only slot the second interreduce loop may drop.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_rhs_interreduce(s, 1u);
    // E0: f(x, a) = f(y, f(e,e)) -- crosswise variables make the pair
    // KBO-incomparable; only the RHS face holds the f(e,e) redex.
    AtpAddedRange e_added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_a()),
        mk_f(mk_v(1u), mk_f(mk_e(), mk_e())));
    CHECK_EQ(e_added.count, 1u);
    CHECK_EQ(s->r_orient[0], 0u);
    CHECK_EQ(s->n_unorient, 1u);

    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_e(), mk_e()), mk_a());
    CHECK_EQ(added.count, 1u);
    u32 n_cps_before = s->n_cps;
    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 1u);
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(s->r_orient[0], 1u);          // the new rule shifted down
    CHECK_EQ(s->n_unorient, 0u);
    CHECK_EQ(s->n_cps, n_cps_before + 1u); // requeued for re-treatment
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-twin-demote-either-face-original-sides");
  {
    // WM Antigleichung demotion lifecycle (GMInterred, Interreduktion.c:
    // 280-293 + Anwendungsprozedur :175-207): when EITHER face of an
    // unorientable equation becomes reducible by the new object, BOTH
    // twins leave E together (KPV_KillParent marks both, KPVerwaltung.c:
    // 343-351) and exactly ONE pair re-enters the queue -- the
    // DISTINGUISHED face's ORIGINAL sides (KPV_IROpferBehandeln copies
    // the untouched pair, KPVerwaltung.c:517-518; the reverse-face
    // buffer branch :192-199 requeues via the Gegenrichtung, i.e. the
    // same distinguished orientation).  thvm's single unorientable slot
    // covers both twins; under the WM preset gates the victim's ORIGINAL
    // sides land in the IR buffer and drain post-CP-gen.
    //
    // Case 1: stored-RHS face reducible (the GMInterred face thvm's
    // second interreduce loop owns).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_rhs_interreduce(s, 1u);
    thvm_atp_set_use_wm_demote(s, 1u);
    AtpAddedRange e_added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_a()),
        mk_f(mk_v(1u), mk_f(mk_e(), mk_e())));
    CHECK_EQ(e_added.count, 1u);
    CHECK_EQ(s->r_orient[0], 0u);
    Term orig_l = s->lhs[0];   // var-normalized stored = distinguished
    Term orig_r = s->rhs[0];
    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_e(), mk_e()), mk_a());
    CHECK_EQ(added.count, 1u);
    u32 n_cps_before = s->n_cps;
    CHECK_EQ(thvm_atp_interreduce(s, added), 1u);
    CHECK_EQ(s->n_rules, 1u);              // both twins gone = slot gone
    CHECK_EQ(s->n_unorient, 0u);
    CHECK_EQ(s->n_cps, n_cps_before);      // buffered, NOT yet requeued
    CHECK_EQ(s->n_irv, 1u);                // exactly ONE victim
    CHECK(kbo_eq(s->irv_lhs[0], orig_l));  // ORIGINAL sides, not the
    CHECK(kbo_eq(s->irv_rhs[0], orig_r));  // slice reduct
    atp_wm_demote_drain(s);
    CHECK_EQ(s->n_irv, 0u);
    CHECK_EQ(s->n_cps, n_cps_before + 1u); // exactly ONE requeue
    thvm_atp_free(s);

    // Case 2: stored-LHS face reducible (RMLinksInterred's
    // NF_ObjektAnwendbar on the equation's other GM reference) -- same
    // lifecycle through thvm's first interreduce loop.
    s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_rhs_interreduce(s, 1u);
    thvm_atp_set_use_wm_demote(s, 1u);
    e_added = thvm_atp_orient_and_add(
        s, mk_f(mk_f(mk_e(), mk_e()), mk_v(VAR_x)),
        mk_f(mk_v(1u), mk_v(VAR_x)));
    CHECK_EQ(e_added.count, 1u);
    CHECK_EQ(s->r_orient[0], 0u);
    orig_l = s->lhs[0];
    orig_r = s->rhs[0];
    added = thvm_atp_orient_and_add(s, mk_f(mk_e(), mk_e()), mk_a());
    CHECK_EQ(added.count, 1u);
    n_cps_before = s->n_cps;
    CHECK_EQ(thvm_atp_interreduce(s, added), 1u);
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(s->n_unorient, 0u);
    CHECK_EQ(s->n_irv, 1u);
    CHECK(kbo_eq(s->irv_lhs[0], orig_l));
    CHECK(kbo_eq(s->irv_rhs[0], orig_r));
    atp_wm_demote_drain(s);
    CHECK_EQ(s->n_cps, n_cps_before + 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-mono-equation-single-face-cp-generation");
  {
    // WM Monogleichung (RE_ErzeugteGleichung, RUndEVerwaltung.c:435-438):
    // an unorientable equation whose reversed pair var-renumbers to the
    // pair itself indexes ONLY its distinguished face (GleichungEinfuegen
    // :484-491) and skips the reverse-face CP-generation phases C/D/E/G
    // (U1_KPsBildenZuGleichung, Unifikation1.c:1563-1579/:1625).  The
    // self-overlap of f(i(x),i(y)) = f(i(y),i(x)) must therefore yield
    // exactly ONE CP -- the top-level l =? l' of the distinguished face
    // (phase F).  Pre-port thvm emitted 4 (one per face combo), 3 of
    // them exact variants WM never forms.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_unfailing_cp(s, 1u);
    AtpAddedRange a = thvm_atp_orient_and_add(s,
        mk_f(mk_i(mk_v(VAR_x)), mk_i(mk_v(1u))),
        mk_f(mk_i(mk_v(1u)), mk_i(mk_v(VAR_x))));
    CHECK_EQ(a.count, 1u);
    CHECK_EQ(s->r_orient[0], 0u);
    CHECK_EQ((u32)atp_eq_is_mono(s, 0), 1u);
    CriticalPair buf[ATP_CP_BATCH];
    u32 cnt = atp_overlap_ij(s, 0, 0, buf, ATP_CP_BATCH, NULL);
    CHECK_EQ(cnt, 1u);
    thvm_atp_free(s);

    // Stereo control: f(i(x), y) = f(y, i(x)) is NOT a mono equation
    // (the reversed pair renumbers differently), so all four face
    // combos run.  WM's self roots are phases F (l =? l), C (l =? r,
    // ONCE) and G (r =? r); face combo 3's root is C's mirror and is
    // not built, so the self pair yields exactly THREE CPs (no proper
    // position of either side unifies an f-rooted face here).
    s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_unfailing_cp(s, 1u);
    a = thvm_atp_orient_and_add(s,
        mk_f(mk_i(mk_v(VAR_x)), mk_v(1u)),
        mk_f(mk_v(1u), mk_i(mk_v(VAR_x))));
    CHECK_EQ(a.count, 1u);
    CHECK_EQ(s->r_orient[0], 0u);
    CHECK_EQ((u32)atp_eq_is_mono(s, 0), 0u);
    cnt = atp_overlap_ij(s, 0, 0, buf, ATP_CP_BATCH, NULL);
    CHECK_EQ(cnt, 3u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-cp-formation-ordering-gate");
  {
    // WM KPAction ordering gate (Unifikation1.c KPActionGR :1394-1401,
    // KPActionRG :1404-1411, KPActionGG :1414-1421): a CP whose
    // equation parent's step at the peak is strictly UPHILL on the
    // unified instance is discarded at FORMATION (never numbered,
    // weighed, or queued).  The canonical victim is the commutativity
    // MIRROR: overlapping f(a, e) -> a with f(y1, y2) = f(y2, y1) at
    // the root instantiates the comm step as f(a, e) -> f(e, a),
    // strictly uphill in KBO (equal weights, precedence e > a), so WM
    // never forms the mirror CP  f(e, a) # a -- the 16-row
    // AbelianGroup@7 / Boolean@8 alignment-matrix divergence class
    // (corpus shape and(x, not(x)) -> F, mirror via not(x) > x).

    // Mutter test (KPActionRG: rule Vater, comm equation Mutter).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_unfailing_cp(s, 1u);
    AtpAddedRange a = thvm_atp_orient_and_add(s,
        mk_f(mk_v(VAR_x), mk_v(1u)), mk_f(mk_v(1u), mk_v(VAR_x)));
    CHECK_EQ(a.count, 1u);
    CHECK_EQ((u32)atp_eq_is_mono(s, 0), 1u);
    a = thvm_atp_orient_and_add(s, mk_f(mk_a(), mk_e()), mk_a());
    CHECK_EQ(a.count, 1u);
    CHECK_EQ(s->r_orient[1], 1u);
    CriticalPair buf[ATP_CP_BATCH];
    u32 cnt = atp_overlap_ij(s, 1, 0, buf, ATP_CP_BATCH, NULL);
    CHECK_EQ(cnt, 0u);                     // mirror gated (was 1 pre-port)
    thvm_atp_free(s);

    // Vater test (KPActionGR: comm equation Vater, rule Mutter) -- the
    // converse intake order, same mirror, gated by the OUTER
    // (KPLinks vs Ueberlappung) comparison.
    s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_unfailing_cp(s, 1u);
    a = thvm_atp_orient_and_add(s, mk_f(mk_a(), mk_e()), mk_a());
    CHECK_EQ(a.count, 1u);
    a = thvm_atp_orient_and_add(s,
        mk_f(mk_v(VAR_x), mk_v(1u)), mk_f(mk_v(1u), mk_v(VAR_x)));
    CHECK_EQ(a.count, 1u);
    cnt = atp_overlap_ij(s, 1, 0, buf, ATP_CP_BATCH, NULL);
    CHECK_EQ(cnt, 0u);                     // mirror gated (was 1 pre-port)
    thvm_atp_free(s);

    // Incomparable-instance control (WM McCune-II cp 5 analog): comm
    // over f(x, e) -> x instantiates the comm step as
    // f(x, e) -> f(e, x), KBO-INCOMPARABLE (const vs var first
    // argument), so the CP  x # f(e, x)  IS formed -- the gate tests
    // strict uphill only.
    s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_unfailing_cp(s, 1u);
    a = thvm_atp_orient_and_add(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    CHECK_EQ(a.count, 1u);
    CHECK_EQ(s->r_orient[0], 1u);
    a = thvm_atp_orient_and_add(s,
        mk_f(mk_v(VAR_x), mk_v(1u)), mk_f(mk_v(1u), mk_v(VAR_x)));
    CHECK_EQ(a.count, 1u);
    cnt = atp_overlap_ij(s, 1, 0, buf, ATP_CP_BATCH, NULL);
    CHECK_EQ(cnt, 1u);                     // incomparable instance kept
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-queue-subsume-gate-roundtrip");
  {
    // WM has no queue-vs-queue subsumption: recentCPinsert
    // (KPVerwaltung.c:383-417) queues every treated survivor and stamps
    // a fresh w2 = ++CPNr age; SS_TermpaarSubsummiertTermpaar's only
    // set-level caller is the E-set sweep (Interreduktion.c:262).  With
    // use_queue_subsume OFF (the "Waldmeister"* presets), an instance
    // of an already-queued CP must therefore QUEUE; the engine default
    // (ON) drops it -- the historical thvm filter, round-tripped here.
    Term gen_l = mk_f(mk_v(VAR_x), mk_e());
    Term gen_r = mk_i(mk_v(VAR_x));
    Term ins_l = mk_f(mk_a(), mk_e());      // instance x -> a
    Term ins_r = mk_i(mk_a());
    CriticalPair cp;
    cp.peak = 0; cp.pos_len = 0;

    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->use_queue_subsume, 1u);     // engine default ON
    cp.lhs = gen_l; cp.rhs = gen_r;
    CHECK_EQ(atp_push_cps_traced(s, &cp, 1u, ATP_TRACE_NONE,
                                 ATP_TRACE_NONE, 0u, 0u), 1u);
    cp.lhs = ins_l; cp.rhs = ins_r;
    CHECK_EQ(atp_push_cps_traced(s, &cp, 1u, ATP_TRACE_NONE,
                                 ATP_TRACE_NONE, 0u, 0u), 0u);
    CHECK_EQ(s->n_cps_dropped_queue_subsumed, 1u);
    CHECK_EQ(s->n_cps, 1u);
    thvm_atp_free(s);

    s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_queue_subsume(s, 0u);  // WM-exact: filter off
    cp.lhs = gen_l; cp.rhs = gen_r;
    CHECK_EQ(atp_push_cps_traced(s, &cp, 1u, ATP_TRACE_NONE,
                                 ATP_TRACE_NONE, 0u, 0u), 1u);
    cp.lhs = ins_l; cp.rhs = ins_r;
    CHECK_EQ(atp_push_cps_traced(s, &cp, 1u, ATP_TRACE_NONE,
                                 ATP_TRACE_NONE, 0u, 0u), 1u);
    CHECK_EQ(s->n_cps_dropped_queue_subsumed, 0u);
    CHECK_EQ(s->n_cps, 2u);                 // both queued, WM ages intact
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-einsstern-cp-filter");
  {
    // WM -einsstern (EinsSternUeberlappung, Unifikation1.c:1039-1055 via
    // AnEinsSternIn :1028-1036): keep a CP only if its overlap position
    // lies on the "1*" leftmost-argument spine of the overlapped LHS --
    // the path that descends from the root taking the FIRST subterm
    // repeatedly.  In thvm geometry that is pos[d] == 0 for all d.
    //
    // Construct an overlapped LHS with overlap positions both ON and OFF
    // the spine.  rule A: f(a, f(x, e)) -> a.  rule B: f(y, z) -> e.
    // atp_overlap_ij(i, j) walks rule i's LHS positions; the i > j call
    // owns the roots (the converse i < j visit enumerates proper
    // positions only).  So we add B at slot 0, A at slot 1, and call
    // atp_overlap_ij(1, 0) -- A as i walks its own LHS WITH root
    // ownership, yielding:
    //   pos []   -- the root f(a, f(x,e))   (empty path -> ON the spine)
    //   pos [1]  -- the inner f(x, e)       (child index 1 -> OFF spine)
    // (position [0] is the nullary `a`, no f-rooted overlap.)  With the
    // filter OFF both CPs form; with -einsstern ON the [1] CP is dropped
    // and only the root (1*) CP survives.
    AtpState *s = thvm_atp_init(&UNIT_CFG, 100);
    thvm_atp_set_use_unfailing_cp(s, 1u);
    AtpAddedRange a = thvm_atp_orient_and_add(s,
        mk_f(mk_v(VAR_x), mk_v(1u)), mk_e());          // B at slot 0
    CHECK_EQ(a.count, 1u);
    a = thvm_atp_orient_and_add(s,
        mk_f(mk_a(), mk_f(mk_v(VAR_x), mk_e())), mk_a());  // A at slot 1
    CHECK_EQ(a.count, 1u);
    CriticalPair buf[ATP_CP_BATCH];
    // Filter OFF (engine default): both spine + off-spine overlaps form.
    CHECK_EQ(s->use_einsstern, 0u);
    u32 cnt_off = atp_overlap_ij(s, 1, 0, buf, ATP_CP_BATCH, NULL);
    CHECK_EQ(cnt_off, 2u);                  // root [] + inner [1]
    u8 saw_offspine = 0u;
    for (u32 k = 0; k < cnt_off; k++)
        for (u8 d = 0; d < buf[k].pos_len; d++)
            if (buf[k].pos[d] != 0u) saw_offspine = 1u;
    CHECK_EQ((u32)saw_offspine, 1u);        // the [1] CP is present
    // Filter ON: the off-spine inner overlap is gated; only the 1* root.
    thvm_atp_set_use_einsstern(s, 1u);
    u32 cnt_on = atp_overlap_ij(s, 1, 0, buf, ATP_CP_BATCH, NULL);
    CHECK_EQ(cnt_on, 1u);                   // the [1] CP was dropped
    CHECK_EQ(s->n_cps_dropped_einsstern, 1u);
    // Every surviving CP sits on the 1* spine (all-zero position path).
    for (u32 k = 0; k < cnt_on; k++)
        for (u8 d = 0; d < buf[k].pos_len; d++)
            CHECK_EQ((u32)buf[k].pos[d], 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-nusfu-no-overlap-below-skolem");
  {
    // WM -nusfu (NusfUeberlappung, Unifikation1.c:1082-1090 via Nusfu
    // :1063-1079): drop a CP whose overlap position lies physically BELOW
    // a skolem-function symbol on the overlapped LHS.  thvm marks skolem
    // labels explicitly (empty on every ground-goal path, so the filter
    // is inert there); registering one lets the gate fire on a
    // constructed case.
    //
    // rule A: i(f(x, e)) -> a, treating LAB_i as the skolem function.
    // rule B: f(y, z) -> e.  B's LHS unifies with A's LHS at:
    //   pos []   -- the root i(...)        (i is unary, not f-rooted: no
    //                                       overlap; the root symbol is i)
    //   pos [0]  -- the inner f(x, e)      (BELOW the skolem i)
    // With -nusfu OFF the [0] CP forms; with it ON (and LAB_i registered
    // as skolem) the [0] CP is gated.
    AtpState *s = thvm_atp_init(&UNIT_CFG, 100);
    thvm_atp_set_use_unfailing_cp(s, 1u);
    AtpAddedRange a = thvm_atp_orient_and_add(s,
        mk_i(mk_f(mk_v(VAR_x), mk_e())), mk_a());
    CHECK_EQ(a.count, 1u);
    a = thvm_atp_orient_and_add(s, mk_f(mk_v(VAR_x), mk_v(1u)), mk_e());
    CHECK_EQ(a.count, 1u);
    CriticalPair buf[ATP_CP_BATCH];
    // OFF: the below-skolem overlap forms.
    CHECK_EQ(s->use_no_overlap_below_skolem, 0u);
    u32 cnt_off = atp_overlap_ij(s, 0, 1, buf, ATP_CP_BATCH, NULL);
    CHECK(cnt_off >= 1u);
    // ON but registry EMPTY: no-op (inert on the ground-goal corpus).
    thvm_atp_set_use_no_overlap_below_skolem(s, 1u);
    u32 cnt_empty = atp_overlap_ij(s, 0, 1, buf, ATP_CP_BATCH, NULL);
    CHECK_EQ(cnt_empty, cnt_off);           // empty skolem set -> identity
    CHECK_EQ(s->n_cps_dropped_nusfu, 0u);
    // ON + LAB_i registered: the below-skolem overlap is gated.
    thvm_atp_add_skolem_label(s, LAB_i);
    u32 cnt_on = atp_overlap_ij(s, 0, 1, buf, ATP_CP_BATCH, NULL);
    CHECK(cnt_on < cnt_off);                // [0] CP dropped
    CHECK(s->n_cps_dropped_nusfu >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-cp-filters-default-off-byte-identity");
  {
    // All eight WM CP-generation filter knobs default OFF, so a CP batch
    // generated under the engine default is byte-identical whether or not
    // the inert flags are set: einsstern OFF keeps every position,
    // nusfu OFF (or ON with an empty skolem registry) keeps every
    // position, and Reclassify / ReversedCompletion / SUEManagement /
    // CriticalGoalInterreduce / CriticalGoalWeight / BackwardGoalArgue
    // are pure storage (no CP-gen effect on the ground-goal surface).
    AtpState *base = thvm_atp_init(&UNIT_CFG, 100);
    thvm_atp_set_use_unfailing_cp(base, 1u);
    (void)thvm_atp_orient_and_add(base,
        mk_f(mk_a(), mk_f(mk_v(VAR_x), mk_e())), mk_a());
    (void)thvm_atp_orient_and_add(base, mk_f(mk_v(VAR_x), mk_v(1u)), mk_e());
    CriticalPair bufA[ATP_CP_BATCH];
    u32 cntA = atp_overlap_ij(base, 0, 1, bufA, ATP_CP_BATCH, NULL);
    thvm_atp_free(base);

    AtpState *t = thvm_atp_init(&UNIT_CFG, 100);
    thvm_atp_set_use_unfailing_cp(t, 1u);
    // Flip every default-OFF knob ON EXCEPT einsstern (and leave nusfu's
    // registry empty) -- none may perturb CP generation on ground goals.
    thvm_atp_set_use_no_overlap_below_skolem(t, 1u);  // empty registry -> no-op
    thvm_atp_set_use_reclassify(t, 1u);
    thvm_atp_set_use_reversed_completion(t, 1u);
    thvm_atp_set_use_sue_management(t, 1u);
    thvm_atp_set_use_critical_goal_interreduce(t, 1u);
    thvm_atp_set_use_critical_goal_weight(t, 1u);
    thvm_atp_set_use_backward_goal_argue(t, 1u);
    (void)thvm_atp_orient_and_add(t,
        mk_f(mk_a(), mk_f(mk_v(VAR_x), mk_e())), mk_a());
    (void)thvm_atp_orient_and_add(t, mk_f(mk_v(VAR_x), mk_v(1u)), mk_e());
    CriticalPair bufB[ATP_CP_BATCH];
    u32 cntB = atp_overlap_ij(t, 0, 1, bufB, ATP_CP_BATCH, NULL);
    CHECK_EQ(cntB, cntA);                    // count unchanged
    for (u32 k = 0; k < cntA && k < cntB; k++) {
        CHECK(kbo_eq(bufB[k].lhs, bufA[k].lhs));
        CHECK(kbo_eq(bufB[k].rhs, bufA[k].rhs));
    }
    thvm_atp_free(t);
  }

  TEST_BEGIN("atp/wm-mixmost-nf-strategy");
  {
    // WM's DEFAULT normal-form strategy `-nf mixmost` (RUN/Parameter.c
    // :418-419; NF_Normalform = NormalformMixMost, NF/NFBildung.c
    // :837-840 = NormalformZuRegelnOderGleichungenAufNochmal :349-377):
    // after a reduction the SAME position is re-reduced to a local
    // fixpoint and only the ANCESTORS along the path are re-tried --
    // never a rescan from the root.  thvm's legacy walk is WM's
    // "outermost" (BL_NormalformOutermost `goto root`).  On a
    // non-confluent mid-completion R the two reach different NFs and
    // flip the generation-time join verdict -- the McCune
    // EqualityOfInverses cp 1893 exemplar (parents 43, 40), mapped
    // and -> f, not -> i:
    //   r33  i(i(x))           -> x
    //   r34  f(f(x1,i(x2)),x2) -> x1
    //   r41  f(i(x1),i(x2))    -> i(f(x2,x1))
    //   r43  i(f(i(x1),x2))    -> f(i(x2),x1)
    // CP raw  (i(v0), f(i(f(i(i(v1)),v0)),v1)):
    //   outermost: inner r43, then r34 fires AT THE ROOT -> i(v0).
    //              JOINED -- the copy WM queues at w1=62 is dropped.
    //   mixmost:   inner r43, then r41 at the SAME position (local
    //              fixpoint) -> f(i(f(v1,v0)),v1), root irreducible.
    //              NOT joined -- WM's exact queued form.
    AtpState *s = thvm_atp_init(&UNIT_CFG, 100);
    AtpAddedRange a;
    a = thvm_atp_orient_and_add(s, mk_i(mk_i(mk_v(VAR_x))), mk_v(VAR_x));
    CHECK_EQ(a.count, 1u);
    a = thvm_atp_orient_and_add(s,
        mk_f(mk_f(mk_v(VAR_x), mk_i(mk_v(1u))), mk_v(1u)), mk_v(VAR_x));
    CHECK_EQ(a.count, 1u);
    a = thvm_atp_orient_and_add(s,
        mk_f(mk_i(mk_v(VAR_x)), mk_i(mk_v(1u))),
        mk_i(mk_f(mk_v(1u), mk_v(VAR_x))));
    CHECK_EQ(a.count, 1u);
    a = thvm_atp_orient_and_add(s,
        mk_i(mk_f(mk_i(mk_v(VAR_x)), mk_v(1u))),
        mk_f(mk_i(mk_v(1u)), mk_v(VAR_x)));
    CHECK_EQ(a.count, 1u);
    CHECK_EQ(s->n_rules, 4u);
    CHECK_EQ(s->n_unorient, 0u);

    Term cp_a = mk_i(mk_v(VAR_x));
    Term cp_b = mk_f(mk_i(mk_f(mk_i(mk_i(mk_v(1u))), mk_v(VAR_x))),
                     mk_v(1u));
    Term jl = cp_a, jr = cp_b;
    CHECK_EQ((u32)atp_cp_trivially_joinable(s, &jl, &jr), 1u);

    thvm_atp_set_use_wm_mixmost_nf(s, 1u);
    jl = cp_a; jr = cp_b;
    CHECK_EQ((u32)atp_cp_trivially_joinable(s, &jl, &jr), 0u);
    CHECK(kbo_eq(jl, mk_i(mk_v(VAR_x))));
    CHECK(kbo_eq(jr, mk_f(mk_i(mk_f(mk_v(1u), mk_v(VAR_x))), mk_v(1u))));

    thvm_atp_set_use_wm_mixmost_nf(s, 0u);  // roundtrip
    jl = cp_a; jr = cp_b;
    CHECK_EQ((u32)atp_cp_trivially_joinable(s, &jl, &jr), 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-regelbaum-match-order");
  {
    // WM's within-position rule choice (MO_RegelGefunden,
    // INF/MatchOperationen.c:565-651): the Regelbaum DFS tries the
    // exact-symbol edge before the variable edges, so when SEVERAL
    // rules match at one position the most-specific pattern fires --
    // thvm's legacy scan takes the lowest slot.  The HigmanNeumann
    // Associativity cp 597 exemplar (parents 14, 12), fop -> f:
    //   r6   f(x1,f(x2,x2))        -> x1          (slot 0)
    //   r14  f(f(x1,x1),f(x2,x3))  -> f(x3,x2)    (slot 1)
    // CP raw  (f(v0,f(v0,f(v0,v0))), f(f(v0,v0),f(v1,v1))):
    //   slot order: r6 at the right side's root keeps the SHARED
    //               variable (-> f(v0,v0)), the left side also
    //               reduces to f(v0,v0) -- JOINED.
    //   Regelbaum:  r14 wins the root (exact f edge beats r6's var
    //               edge), keeping the sides on DISTINCT variables
    //               (f(v1,v1) vs f(v0,v0)) -- NOT joined, WM's
    //               queued  fop(x1,x1) # fop(x2,x2)  at w1=48.
    AtpState *s = thvm_atp_init(&UNIT_CFG, 100);
    AtpAddedRange a;
    a = thvm_atp_orient_and_add(s,
        mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(1u))), mk_v(VAR_x));
    CHECK_EQ(a.count, 1u);
    a = thvm_atp_orient_and_add(s,
        mk_f(mk_f(mk_v(VAR_x), mk_v(VAR_x)), mk_f(mk_v(1u), mk_v(2u))),
        mk_f(mk_v(2u), mk_v(1u)));
    CHECK_EQ(a.count, 1u);
    CHECK_EQ(s->n_rules, 2u);

    Term cp_a = mk_f(mk_v(VAR_x),
                     mk_f(mk_v(VAR_x), mk_f(mk_v(VAR_x), mk_v(VAR_x))));
    Term cp_b = mk_f(mk_f(mk_v(VAR_x), mk_v(VAR_x)),
                     mk_f(mk_v(1u), mk_v(1u)));
    Term jl = cp_a, jr = cp_b;
    CHECK_EQ((u32)atp_cp_trivially_joinable(s, &jl, &jr), 1u);

    thvm_atp_set_use_wm_mixmost_nf(s, 1u);
    jl = cp_a; jr = cp_b;
    CHECK_EQ((u32)atp_cp_trivially_joinable(s, &jl, &jr), 0u);
    CHECK(kbo_eq(jl, mk_f(mk_v(VAR_x), mk_v(VAR_x))));
    CHECK(kbo_eq(jr, mk_f(mk_v(1u), mk_v(1u))));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-leaf-list");
  {
    // WM leaf-list discipline (DSBaumOperationen.c BlattEinzeigern
    // :365-442): ascending stored-LHS depth; within a depth class the
    // FIRST leaf stays head and later leaves insert immediately after
    // it (head + LIFO); removal promotes the next same-depth leaf
    // (:949-981).  Insert depths [4, 2, 3, 3, 3] as traces 100..104:
    // expected list [101(d2), 102(d3 head), 104, 103, 100(d4)]; after
    // removing the d3 head: [101, 104, 103, 100].
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    Term lhss[5] = {
      mk_i(mk_i(mk_i(mk_a()))),          // d4
      mk_i(mk_a()),                      // d2
      mk_i(mk_i(mk_a())),                // d3
      mk_f(mk_i(mk_a()), mk_a()),        // d3
      mk_f(mk_a(), mk_i(mk_a())),        // d3
    };
    for (u32 k = 0; k < 5u; k++) {
      s->lhs[k] = lhss[k];
      s->rhs[k] = mk_a();
      s->r_orient[k] = 1u;
      s->r_trace[k] = 100u + k;
      s->n_rules++;
      atp_wmo_insert_fact(s, k);
    }
    {
      AtpWmOrder *w = (AtpWmOrder *)s->wmo;
      u32 want[5] = {101u, 102u, 104u, 103u, 100u};
      u32 got = 0;
      for (WmoLeaf *l = w->tree[0].ll_head; l != NULL; l = l->ll_next) {
        CHECK(got < 5u);
        CHECK_EQ(l->n_chain, 1u);
        CHECK_EQ(l->chain[0].trace, want[got]);
        got++;
      }
      CHECK_EQ(got, 5u);
      atp_wmo_remove_trace(s, 102u);     // d3 class head dies
      u32 want2[4] = {101u, 104u, 103u, 100u};
      got = 0;
      for (WmoLeaf *l = w->tree[0].ll_head; l != NULL; l = l->ll_next) {
        CHECK(got < 4u);
        CHECK_EQ(l->chain[0].trace, want2[got]);
        got++;
      }
      CHECK_EQ(got, 4u);
    }
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-ett-leaf-list-rank");
  {
    // Phase-B (eTT) emission order: the new fact's CP batch visits
    // existing partners in LEAF-LIST order (depth ascending), not slot
    // order.  old0 (slot 0) has the DEEPER lhs, old1 (slot 1) the
    // shallower; both contain an i(.)-subterm the new rule's top
    // unifies with.  Legacy slot-major emission would queue old0's CP
    // first; WM order queues old1's first (smaller cp_seq).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    // old0: f(f(i(x), e), e) -> x      (lhs depth 4)
    s->lhs[0] = mk_f(mk_f(mk_i(mk_v(VAR_x)), mk_e()), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->r_orient[0] = 1u; s->r_trace[0] = 200u; s->n_rules++;
    atp_wmo_insert_fact(s, 0u);
    // old1: f(i(y), e) -> y            (lhs depth 3)
    s->lhs[1] = mk_f(mk_i(mk_v(1u)), mk_e());
    s->rhs[1] = mk_v(1u);
    s->r_orient[1] = 1u; s->r_trace[1] = 201u; s->n_rules++;
    atp_wmo_insert_fact(s, 1u);
    // new:  i(f(a, a)) -> a            (top unifies with both i(.)s;
    // no tops overlaps: f(a,a) clashes with both old tops)
    s->lhs[2] = mk_i(mk_f(mk_a(), mk_a()));
    s->rhs[2] = mk_a();
    s->r_orient[2] = 1u; s->r_trace[2] = 202u; s->n_rules++;
    atp_wmo_insert_fact(s, 2u);
    AtpAddedRange added = {2u, 1u, 0u};
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK_EQ(pushed, 2u);
    u32 seq_old0 = 0xffffffffu, seq_old1 = 0xffffffffu;
    for (u32 k = 0; k < s->n_cps; k++) {
      Term tr = s->trace[s->cp_trace[k]];
      u32 pa = (u32)term_val(term_ctr_at(tr, 0));
      Term pae = s->trace[pa];
      // parent_a = the OUTER rule's trace entry; resolve to 200/201 by
      // matching the stored trace index against r_trace.
      (void)pae;
      if (pa == s->r_trace[0]) seq_old0 = s->cp_seq[k];
      if (pa == s->r_trace[1]) seq_old1 = s->cp_seq[k];
    }
    CHECK(seq_old0 != 0xffffffffu);
    CHECK(seq_old1 != 0xffffffffu);
    CHECK(seq_old1 < seq_old0);   // shallow leaf first (WM leaf list)
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-cp-derived-distinguished-face");
  {
    // WM stores a derived unorientable equation's `selRec->lhs` =
    // `KPLinks = MGU(TP_RechteSeite(Vater))` = sigma(outer-rule RHS) as
    // its DISTINGUISHED (indexed) face (Unifikation1.c:916,
    // RUndEVerwaltung.c:407-425); the reverse face runs the late eTT
    // E-phase.  thvm's CP constructor puts sigma(r_i) on cp.rhs and the
    // pop-time normalize keeps that lhs/rhs assignment, so a CP-DERIVED
    // unorientable equation's WM-distinguished face is thvm's stored
    // RHS, not its LHS.  The stored orientation is NOT changed (that
    // would move the CP set via the formation-time KPAction order gate);
    // the wmo mirror records the flip per-trace (dist_rhs) and indexes
    // the WM-distinguished face as face 0 of the equation tree.
    //
    // eq (asymmetric unorientable): lhs = i(f(a,a)), rhs = f(a, i(a)).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    Term eq_lhs = mk_i(mk_f(mk_a(), mk_a()));        // KPRechts
    Term eq_rhs = mk_f(mk_a(), mk_i(mk_a()));        // KPLinks = WM-dist
    s->lhs[0] = eq_lhs;
    s->rhs[0] = eq_rhs;
    s->r_orient[0] = 0u; s->r_trace[0] = 402u; s->n_rules++; s->n_unorient++;
    CHECK_EQ((u32)atp_eq_is_mono(s, 0u), 0u);
    atp_wmo_insert_fact_ex(s, 0u, /*cp_derived=*/1u);
    AtpWmOrder *w = (AtpWmOrder *)s->wmo;
    // The mirror records WM-distinguished = stored RHS for this trace.
    CHECK_EQ((u32)wmo_trace_dist_rhs(w, 402u), 1u);
    // The equation tree indexes the WM-distinguished face (= stored RHS,
    // f(a,i(a))) under WM-face 0, and the reverse face (= stored LHS,
    // i(f(a,a))) under WM-face 1.  Find each leaf's chain face.
    {
      WmoCell rc[WMO_MAX_CELLS], lc[WMO_MAX_CELLS];
      u32 rn = wmo_face_cells(eq_rhs, rc, WMO_MAX_CELLS);
      u32 ln = wmo_face_cells(eq_lhs, lc, WMO_MAX_CELLS);
      u8 dist_face = 0xffu, rev_face = 0xffu;
      for (WmoLeaf *l = w->tree[1].ll_head; l != NULL; l = l->ll_next) {
        u8 is_rhs = (l->key_len == rn);
        u8 is_lhs = (l->key_len == ln);
        for (u32 c = 0; c < l->key_len && (is_rhs || is_lhs); c++) {
          if (is_rhs && !wmo_cell_eq(&l->key[c], &rc[c])) is_rhs = 0u;
          if (is_lhs && !wmo_cell_eq(&l->key[c], &lc[c])) is_lhs = 0u;
        }
        if (is_rhs && l->n_chain > 0u) dist_face = l->chain[0].face;
        if (is_lhs && l->n_chain > 0u) rev_face  = l->chain[0].face;
      }
      CHECK_EQ((u32)dist_face, 0u);   // RHS = distinguished, WM-face 0
      CHECK_EQ((u32)rev_face, 1u);    // LHS = reverse, WM-face 1
    }
    // Intake registration of the SAME asymmetric equation keeps
    // distinguished = stored LHS (LRSortieren surface, no flip).
    s->lhs[1] = mk_i(mk_f(mk_a(), mk_a()));
    s->rhs[1] = mk_f(mk_a(), mk_i(mk_a()));
    s->r_orient[1] = 0u; s->r_trace[1] = 403u; s->n_rules++; s->n_unorient++;
    atp_wmo_insert_fact_ex(s, 1u, /*cp_derived=*/0u);
    CHECK_EQ((u32)wmo_trace_dist_rhs(w, 403u), 0u);
    // Ground-side override: a CP-derived unorientable equation with a
    // GROUND lhs and a variable-bearing rhs gets dist_rhs = 0 -- thvm and
    // WM can orient the same equation from different representative CPs,
    // but WM's first-selected representative lands KPLinks on the ground
    // side (the grounding/absorbing rule's output), so the WM-
    // distinguished face is the ground stored LHS, not the cp.rhs default.
    // (CommRing ZeroIsAbsorbing eq `and(const2,~k1) = and(x,~k1)`:
    // ground `and(const2,~k1)` is distinguished -> full-identity sequence.)
    // Cited: waldmeister INF/RUndEVerwaltung.c:407-425 (links = selRec.lhs
    // = KPLinks), INF/Unifikation1.c:916.
    s->lhs[2] = mk_f(mk_a(), mk_i(mk_a()));          // ground
    s->rhs[2] = mk_f(mk_a(), mk_v(VAR_x));           // variable-bearing
    s->r_orient[2] = 0u; s->r_trace[2] = 404u; s->n_rules++; s->n_unorient++;
    CHECK_EQ((u32)atp_eq_is_mono(s, 2u), 0u);
    atp_wmo_insert_fact_ex(s, 2u, /*cp_derived=*/1u);
    CHECK_EQ((u32)wmo_trace_dist_rhs(w, 404u), 0u);
    // The mirror flip on the OTHER orientation (ground rhs, variable lhs)
    // pins dist_rhs = 1 -- the ground side is the stored RHS.
    s->lhs[3] = mk_f(mk_a(), mk_v(VAR_x));           // variable-bearing
    s->rhs[3] = mk_f(mk_a(), mk_i(mk_a()));          // ground
    s->r_orient[3] = 0u; s->r_trace[3] = 405u; s->n_rules++; s->n_unorient++;
    atp_wmo_insert_fact_ex(s, 3u, /*cp_derived=*/1u);
    CHECK_EQ((u32)wmo_trace_dist_rhs(w, 405u), 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-unorient-equation-leaflist-rank");
  {
    // The NORMALIZE-redex selection (atp_unorient_step_indexed ->
    // atp_ft_unorient_step) chooses among the unorientable equations that
    // match one position.  Legacy thvm fired the lowest-SLOT equation,
    // l->r before r->l; WM consults the Gleichungsbaum
    // (MO_GleichungGefunden, MatchOperationen.c:658-763), reaching the
    // matching leaf in discrimination-tree order then walking that leaf's
    // GleichungsT chain and firing the FIRST order-decreasing member.  On
    // an AC theory several rotation equations share one leaf, so slot
    // order picks a different rotation than WM and bakes a swapped
    // commutative argument order into the resulting rule (Huntington
    // DoubleNegation rule 12 inner-or args vs WM rule 9).  The
    // atp_wmo_eq_leaflist_rank accessor exposes the WM leaf-list rank +
    // within-leaf chain index that the candidate sort re-keys on.
    //
    // Two unorientable equations whose distinguished (lhs) faces are
    // alpha-DISTINCT i(.) patterns of EQUAL depth share the same depth
    // class: the first inserted heads the leaf list, the second inserts
    // immediately after it -- so their leaf-list ranks are 0 and 1.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    // eq0: i(f(x,a)) = f(a,x)   (distinguished lhs = i(f(x,a)), depth 3)
    s->lhs[0] = mk_i(mk_f(mk_v(VAR_x), mk_a()));
    s->rhs[0] = mk_f(mk_a(), mk_v(VAR_x));
    s->r_orient[0] = 0u; s->r_trace[0] = 500u; s->n_rules++; s->n_unorient++;
    atp_wmo_insert_fact_ex(s, 0u, /*cp_derived=*/0u);
    // eq1: i(f(a,y)) = f(y,a)   (distinguished lhs = i(f(a,y)), depth 3)
    s->lhs[1] = mk_i(mk_f(mk_a(), mk_v(1u)));
    s->rhs[1] = mk_f(mk_v(1u), mk_a());
    s->r_orient[1] = 0u; s->r_trace[1] = 501u; s->n_rules++; s->n_unorient++;
    atp_wmo_insert_fact_ex(s, 1u, /*cp_derived=*/0u);
    // Both faces are mono-distinct so dist_rhs = 0; thvm dir 0 (match lhs)
    // = WM-distinguished face 0.  eq0's distinguished face was inserted
    // first (leaf-list head, rank 0); eq1's second (rank 1).
    u32 ll0 = 0xffffu, ch0 = 0xffffu, ll1 = 0xffffu, ch1 = 0xffffu;
    CHECK_EQ((u32)atp_wmo_eq_leaflist_rank(s, 500u, /*thvm_dir=*/0u, &ll0, &ch0), 1u);
    CHECK_EQ((u32)atp_wmo_eq_leaflist_rank(s, 501u, /*thvm_dir=*/0u, &ll1, &ch1), 1u);
    // Distinct leaves (different patterns) -> distinct leaf-list ranks,
    // eq0 (inserted first, same depth class) ahead of eq1.
    CHECK(ll0 < ll1);
    // An unregistered trace yields no rank (caller falls back to slot).
    u32 llx = 0xffffu, chx = 0xffffu;
    CHECK_EQ((u32)atp_wmo_eq_leaflist_rank(s, 999u, 0u, &llx, &chx), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-self-root-face-flip");
  {
    // WM's self-root phases are classified by FACE, not by thvm's raw
    // combo bits: F = l =? l (both WM-DISTINGUISHED faces), C = r =? l
    // (mixed, stereo only), G = r =? r (both WM-REVERSE faces), where l/r
    // are WM's distinguished/reverse sides (U1_KPsBildenZuGleichung
    // F/C/G).  For a CP-derived unorientable equation whose WM-
    // distinguished face is thvm's STORED RHS (dist_rhs=1), thvm's combo-0
    // self-overlap (stored lhs x stored lhs) is WM's reverse self = G, and
    // its combo-3 self-overlap (stored rhs x stored rhs) is WM's
    // distinguished self = F -- the two SWAP.  Keying the self-root phase
    // on raw combo (F=combo0, G=combo3) gives the reverse order and pushes
    // the distinguished-face self-CP to a LATER FIFO age than the reverse
    // one; on Boolean Absorption that mis-aged the and/or partner pair at
    // selection (firstdiv 107).  The rank must instead key on the WM face
    // (combo XOR dist_rhs).
    //
    // eq (asymmetric unorientable): lhs = i(f(a,a)) [reverse], rhs =
    // f(a,i(a)) [WM-distinguished].
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    s->lhs[0] = mk_i(mk_f(mk_a(), mk_a()));
    s->rhs[0] = mk_f(mk_a(), mk_i(mk_a()));
    s->r_orient[0] = 0u; s->r_trace[0] = 500u; s->n_rules++; s->n_unorient++;
    CHECK_EQ((u32)atp_eq_is_mono(s, 0u), 0u);
    atp_wmo_insert_fact_ex(s, 0u, /*cp_derived=*/1u);
    AtpWmOrder *w = (AtpWmOrder *)s->wmo;
    CHECK_EQ((u32)wmo_trace_dist_rhs(w, 500u), 1u);
    // Synthetic self-root CPs (pos_len = 0).  The face-flip means:
    //   combo 0 (thvm lhs x lhs)  -> WM G (reverse self)
    //   combo 3 (thvm rhs x rhs)  -> WM F (distinguished self)
    // so combo-3's key must be SMALLER (F = phase 2 < G = phase 6).
    CriticalPair cp_self;
    cp_self.lhs = s->lhs[0];
    cp_self.rhs = s->rhs[0];
    cp_self.peak = s->lhs[0];
    cp_self.pos_len = 0u;
    u64 key_combo0 = atp_wmo_rank(s, 0u, 0u, 0u, /*combo=*/0u, &cp_self);
    u64 key_combo3 = atp_wmo_rank(s, 0u, 0u, 0u, /*combo=*/3u, &cp_self);
    // phase lives in the top nibble (bits 58..61); F = 2, G = 6.
    CHECK_EQ((u32)((key_combo3 >> 58) & 0xfu), 2u);   // combo 3 -> F
    CHECK_EQ((u32)((key_combo0 >> 58) & 0xfu), 6u);   // combo 0 -> G
    CHECK(key_combo3 < key_combo0);                    // F before G
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-tops-before-ett");
  {
    // Phase segmentation (U1_KPsBildenZuRegel: toplevel phase 2 before
    // the eTT phase 3): a CP from the new fact's own positions (tops)
    // must receive a smaller FIFO age than a CP planted into an OLD
    // fact's proper position (eTT), regardless of slot order.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    // old: f(i(x), e) -> x   (top f(i(x),e); proper subterm i(x))
    s->lhs[0] = mk_f(mk_i(mk_v(VAR_x)), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->r_orient[0] = 1u; s->r_trace[0] = 300u; s->n_rules++;
    atp_wmo_insert_fact(s, 0u);
    // new: i(f(i(a), e)) -> a:
    //   tops: subterm f(i(a),e) unifies the old TOP   -> phase A
    //   eTT:  new top i(...) unifies old's i(x)        -> phase B
    s->lhs[1] = mk_i(mk_f(mk_i(mk_a()), mk_e()));
    s->rhs[1] = mk_a();
    s->r_orient[1] = 1u; s->r_trace[1] = 301u; s->n_rules++;
    atp_wmo_insert_fact(s, 1u);
    AtpAddedRange added = {1u, 1u, 0u};
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK_EQ(pushed, 2u);
    u32 seq_tops = 0xffffffffu, seq_ett = 0xffffffffu;
    for (u32 k = 0; k < s->n_cps; k++) {
      Term tr = s->trace[s->cp_trace[k]];
      u32 pa = (u32)term_val(term_ctr_at(tr, 0));
      if (pa == s->r_trace[1]) seq_tops = s->cp_seq[k];  // outer = new
      if (pa == s->r_trace[0]) seq_ett  = s->cp_seq[k];  // outer = old
    }
    CHECK(seq_tops != 0xffffffffu);
    CHECK(seq_ett != 0xffffffffu);
    CHECK(seq_tops < seq_ett);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-ir-victim-drain-leaf-order");
  {
    // IR-victim re-queue order (WM IR_PufferAuslesen,
    // Interreduktion.c:391): RMLinksInterred buffers each demoted rule via
    // RE_forRegelnRobust = BK_forRegelnRobust over Baum.ErstesBlatt (the
    // rule-tree LEAF LIST in order), the FIFO REPuffer drains in that same
    // order -- so two victims re-enter the queue (and get their late FIFO
    // ages w2 = ++CPNr) in discrimination-tree leaf order, NOT thvm's
    // slot-scan order.  This is the AbsorptionOrAnd @118 commutative-mirror
    // argument-order tie: WM demotes the two commutative-mirror rules in
    // leaf order; thvm's interreduce loop visited slots ascending, which
    // can reverse them.  Set up a d2-LHS rule (trace 500, leaf-list HEAD of
    // its class) and a d4-LHS rule (trace 501, later in the list).  Push
    // them as IR victims in REVERSE leaf order (501 then 500); the drain
    // sort must restore leaf order (500 before 501).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    s->use_wm_demote = 1u;
    s->lhs[0] = mk_i(mk_a());                       // d2
    s->rhs[0] = mk_a();
    s->r_orient[0] = 1u; s->r_trace[0] = 500u; s->n_rules++;
    atp_wmo_insert_fact(s, 0u);
    s->lhs[1] = mk_i(mk_i(mk_i(mk_a())));           // d4
    s->rhs[1] = mk_a();
    s->r_orient[1] = 1u; s->r_trace[1] = 501u; s->n_rules++;
    atp_wmo_insert_fact(s, 1u);
    // Leaf list: the d2 leaf (500) precedes the d4 leaf (501).
    u32 key500 = atp_wmo_victim_drain_key(s, 500u);
    u32 key501 = atp_wmo_victim_drain_key(s, 501u);
    CHECK(key500 < key501);
    // Both are rules (tree 0) -> bit 31 set on each.
    CHECK_EQ(key500 >> 31, 1u);
    CHECK_EQ(key501 >> 31, 1u);
    // Buffer the victims in REVERSE leaf order (501 first), then sort.
    atp_irv_push(s, s->lhs[1], s->rhs[1], s->r_trace[1], key501);
    atp_irv_push(s, s->lhs[0], s->rhs[0], s->r_trace[0], key500);
    CHECK_EQ(s->n_irv, 2u);
    atp_irv_sort_wm_order(s);
    // After the sort the lower-keyed (leaf-earlier, trace 500) victim
    // drains first -> smaller cp_seq / FIFO age, matching WM.
    CHECK_EQ(s->irv_wmo_key[0], key500);
    CHECK_EQ(s->irv_wmo_key[1], key501);
    s->n_irv = 0u;
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-tops-rank-wide-tree");
  {
    // Regression guard for the tops-DFS arrival buffer cap.  WM's CP
    // emission order ranks a new fact's tops overlaps by the discrim-tree
    // ARRIVAL position of the partner leaf (wmo_tops_rank).  The
    // combinator signatures (CombinatorAxioms / Meredith opCenterdot)
    // build very wide trees where one query subterm unifies with several
    // THOUSAND stored leaves; the old fixed 512-leaf arrival buffer
    // silently truncated those scans (SKIToBCKW c2: 2069 truncated
    // ranks), scrambling the equal-weight FIFO age order the selection
    // heap breaks ties on.  Here `N_WIDE` rules f(i^k(a), e) -> a all
    // share the top f(.,.), so a new fact whose subterm is f(i(a), e)
    // unifies every one of them at the top: the partner that arrives past
    // index 512 must still be found (no rank-miss).  With the old cap the
    // late-arriving partner's overlap fell into the 0x3fff fallback
    // bucket and the rank-miss counter spiked.
    const u32 N_WIDE = 700u;
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    atp_ensure_rule_cap(s, N_WIDE + 1u);
    for (u32 k = 0; k < N_WIDE; k++) {
      // Distinct LHS per rule (i-chain depth k): the discrim tree gets
      // N_WIDE separate leaves, all sharing the top f(.,.) so a query
      // f(x, e) arrives at every one of them.
      Term inner = mk_a();
      for (u32 d = 0; d < k; d++) inner = mk_i(inner);
      s->lhs[k] = mk_f(inner, mk_e());
      s->rhs[k] = mk_a();
      s->r_orient[k] = 1u;
      s->r_trace[k] = 1000u + k;
      s->n_rules++;
      atp_wmo_insert_fact(s, k);
    }
    AtpWmOrder *w0 = (AtpWmOrder *)s->wmo;
    u32 misses_before = w0->rank_misses;
    // New fact i(f(x, e)) -> x: its proper subterm f(x, e) (VARIABLE first
    // arg) unifies the shared top of EVERY stored rule f(i^k(a), e), so
    // the tops DFS arrives at all N_WIDE leaves.  Each generates one tops
    // CP; every partner leaf must rank (none fall to the 0x3fff miss
    // fallback) now that the arrival buffer holds all N_WIDE arrivals --
    // with the old 512 cap the leaves past index 512 were dropped and
    // their overlaps spiked the rank-miss counter.
    u32 nf = N_WIDE;
    s->lhs[nf] = mk_i(mk_f(mk_v(VAR_x), mk_e()));
    s->rhs[nf] = mk_v(VAR_x);
    s->r_orient[nf] = 1u;
    s->r_trace[nf] = 1000u + nf;
    s->n_rules++;
    atp_wmo_insert_fact(s, nf);
    AtpAddedRange added = {nf, 1u, 0u};
    (void)thvm_atp_generate_cps(s, added);
    AtpWmOrder *w = (AtpWmOrder *)s->wmo;
    // The wide tree must not produce any new tops rank-miss: every
    // partner (including those arriving past the old 512 cap) was found.
    CHECK_EQ(w->rank_misses, misses_before);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-ir-victim-equation-before-rule");
  {
    // WM IR_InterreduktionLinks runs GMInterred (equation victims) BEFORE
    // RMLinksInterred (rule victims), both into one FIFO puffer -- so an
    // equation victim drains before a rule victim regardless of slot.  The
    // drain key sets bit 31 for rules (tree 0) and clears it for equations
    // (tree 1), so equation keys sort first.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    // slot 0: an orientable rule (lives in the rule tree, tree 0).
    s->lhs[0] = mk_i(mk_a());
    s->rhs[0] = mk_a();
    s->r_orient[0] = 1u; s->r_trace[0] = 600u; s->n_rules++;
    atp_wmo_insert_fact(s, 0u);
    // slot 1: an unorientable equation (lives in the equation tree, tree
    // 1).  Asymmetric so it is non-mono (distinguished face = stored lhs).
    s->lhs[1] = mk_f(mk_i(mk_a()), mk_a());
    s->rhs[1] = mk_f(mk_a(), mk_i(mk_a()));
    s->r_orient[1] = 0u; s->r_trace[1] = 601u; s->n_rules++; s->n_unorient++;
    atp_wmo_insert_fact(s, 1u);
    u32 key_rule = atp_wmo_victim_drain_key(s, 600u);
    u32 key_eq   = atp_wmo_victim_drain_key(s, 601u);
    CHECK_EQ(key_rule >> 31, 1u);   // rule tree -> bit 31 set
    CHECK_EQ(key_eq   >> 31, 0u);   // equation tree -> bit 31 clear
    CHECK(key_eq < key_rule);       // equation victim drains first
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-split-enclosing-exit-newer-first");
  {
    // BooleanAxioms OrAssociativity @300 polarity.  When a SHORTER new
    // leaf splits a LONGER existing leaf at a position whose enclosing
    // function subterm opened at a STRICT ANCESTOR of the split, the new
    // leaf's parallel jump must HEAD that ancestor node's outgoing exit
    // list (the more-general / shorter jump is consulted first) -- not be
    // chained after the old leaf's surviving entry as plain
    // AltesBlattPolieren (DSBaumOperationen.c :523-526) would place it.
    //
    // Build the f(v1, f(v1, ...)) family with f=binary(label 3),
    // i=unary(label 2):
    //   t740 = f(v1, f(v2, v1))            (sibling -> forces a node at the
    //                                       inner-f first arg so the split
    //                                       lands at a strict descendant)
    //   t274 = f(v1, f(v1, f(v2, i(v2))))  (LONG victim, 8 cells)
    //   t768 = f(v1, f(v1, v2))            (SHORT new leaf, 5 cells)
    // Inserting t768 splits t274; the enclosing f(v1, ...) opened two
    // levels up.  The d2 node (root -> f -> v1) must list t768's parallel
    // FIRST.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    Term lhss[3] = {
      mk_f(mk_v(0u), mk_f(mk_v(1u), mk_v(0u))),                       // t740
      mk_f(mk_v(0u), mk_f(mk_v(0u), mk_f(mk_v(1u), mk_i(mk_v(1u))))), // t274
      mk_f(mk_v(0u), mk_f(mk_v(0u), mk_v(1u))),                       // t768
    };
    u32 traces[3] = { 740u, 274u, 768u };
    for (u32 k = 0; k < 3u; k++) {
      s->lhs[k] = lhss[k];
      s->rhs[k] = mk_v(0u);
      s->r_orient[k] = 1u;
      s->r_trace[k] = traces[k];
      s->n_rules++;
      atp_wmo_insert_fact(s, k);
    }
    {
      AtpWmOrder *w = (AtpWmOrder *)s->wmo;
      // navigate root -> f -> v1
      WmoCell cf; cf.sym = 3u; cf.is_var = 0u; cf.arity = 2u;   // f
      WmoCell cv; cv.sym = 1u; cv.is_var = 1u; cv.arity = 0u;   // v1
      u8 il = 0;
      WmoNode *d1 = (WmoNode *)wmo_kid_get(w->tree[0].root, &cf, &il);
      CHECK(d1 != NULL && il == 0u);
      WmoNode *d2 = (WmoNode *)wmo_kid_get(d1, &cv, &il);
      CHECK(d2 != NULL && il == 0u);
      // the HEAD exit of d2 must target the NEW shorter leaf (t768).
      WmoEntry *head = d2->exits;
      CHECK(head != NULL);
      CHECK_EQ(head->ziel_leaf, 1u);
      WmoLeaf *hl = (WmoLeaf *)head->ziel;
      u8 head_is_t768 = 0;
      for (u32 c = 0; c < hl->n_chain; c++) {
        if (hl->chain[c].trace == 768u) head_is_t768 = 1u;
      }
      CHECK_EQ(head_is_t768, 1u);
      // t768's parallel must be strictly ahead of t274's enclosing jump.
      u32 idx = 0, at_768 = 0xffffffffu, at_274 = 0xffffffffu;
      for (WmoEntry *e = d2->exits; e != NULL; e = e->next, idx++) {
        if (!e->ziel_leaf) continue;
        WmoLeaf *l = (WmoLeaf *)e->ziel;
        for (u32 c = 0; c < l->n_chain; c++) {
          if (l->chain[c].trace == 768u) at_768 = idx;
          if (l->chain[c].trace == 274u) at_274 = idx;
        }
      }
      CHECK(at_768 != 0xffffffffu);
      CHECK(at_274 != 0xffffffffu);
      CHECK(at_768 < at_274);   // newer/shorter consulted first
    }
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-split-enclosing-exit-older-first");
  {
    // Opposite polarity (HigmanNeumann @61 family): a LONGER new leaf that
    // splits a SHORTER existing leaf keeps the plain AltesBlattPolieren
    // after-old placement (DSBaumOperationen.c :523-526) -- the older /
    // shorter generic jump is consulted FIRST.  Same three faces as the
    // newer-first test but with t768 (short) inserted as the victim and
    // t274 (long) the splitter, so the discriminator's `old_e2 > e2`
    // clause is false and no head-insert fires.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    Term lhss[3] = {
      mk_f(mk_v(0u), mk_f(mk_v(1u), mk_v(0u))),                       // t740
      mk_f(mk_v(0u), mk_f(mk_v(0u), mk_v(1u))),                       // t768 (short victim)
      mk_f(mk_v(0u), mk_f(mk_v(0u), mk_f(mk_v(1u), mk_i(mk_v(1u))))), // t274 (long splitter)
    };
    u32 traces[3] = { 740u, 768u, 274u };
    for (u32 k = 0; k < 3u; k++) {
      s->lhs[k] = lhss[k];
      s->rhs[k] = mk_v(0u);
      s->r_orient[k] = 1u;
      s->r_trace[k] = traces[k];
      s->n_rules++;
      atp_wmo_insert_fact(s, k);
    }
    {
      AtpWmOrder *w = (AtpWmOrder *)s->wmo;
      WmoCell cf; cf.sym = 3u; cf.is_var = 0u; cf.arity = 2u;
      WmoCell cv; cv.sym = 1u; cv.is_var = 1u; cv.arity = 0u;
      u8 il = 0;
      WmoNode *d1 = (WmoNode *)wmo_kid_get(w->tree[0].root, &cf, &il);
      CHECK(d1 != NULL && il == 0u);
      WmoNode *d2 = (WmoNode *)wmo_kid_get(d1, &cv, &il);
      CHECK(d2 != NULL && il == 0u);
      u32 idx = 0, at_768 = 0xffffffffu, at_274 = 0xffffffffu;
      for (WmoEntry *e = d2->exits; e != NULL; e = e->next, idx++) {
        if (!e->ziel_leaf) continue;
        WmoLeaf *l = (WmoLeaf *)e->ziel;
        for (u32 c = 0; c < l->n_chain; c++) {
          if (l->chain[c].trace == 768u) at_768 = idx;
          if (l->chain[c].trace == 274u) at_274 = idx;
        }
      }
      CHECK(at_768 != 0xffffffffu);
      CHECK(at_274 != 0xffffffffu);
      CHECK(at_768 < at_274);   // older/shorter victim consulted first
    }
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-split-ancestor-jump-no-fresh-duplicate");
  {
    // HuntingtonAxioms DoubleNegation @79.  A strict-ancestor jump (the
    // enclosing function subterm opened ABOVE the leaf-found split point)
    // is shared structurally by the old and new leaf: WM keeps ONE outgoing
    // exit at that ancestor node and AltesBlattPolieren's else-branch
    // (DSBaumOperationen.c :534-560) merely RE-TARGETS it from the old leaf
    // onto the inner chain node, preserving its outgoing-list position
    // (only Sprungeingaenge and Zielknoten change, not Sprungausgaenge).
    // No fresh parallel is built.  thvm must not head-insert a duplicate
    // `anc` jump there; doing so reordered the ancestor node's exit list
    // (prepending the later split's branch ahead of the earlier one) and
    // flipped the tops-DFS arrival order of the partner rules.
    //
    // Build the four order-13 faces in WM insertion order (or = binary
    // label 3, not = unary label 2), all depth 5, splitting pairwise at the
    // same strict-ancestor depth-1 `not(or(...))` subterm:
    //   t3   = or(not(or(not(v0),v1)), not(or(not(v0),not(v1))))  branch B
    //   t65  = or(not(or(v0,not(v1))), not(or(not(v1),not(v0))))  branch A
    //   t79  = or(not(or(not(v0),v1)), not(or(not(v1),not(v0))))  branch B
    //   t100 = or(not(or(v0,not(v1))), not(or(not(v0),not(v1))))  branch A
    // Inserting t79 splits t3 and t100 splits t65, each closing the
    // depth-1 ancestor jump within the new chain.  The WM tops-DFS query
    // or(v0, not(or(not(v1),not(v2)))) (the order-13 LHS first-arg subterm)
    // must arrive partners in WM CPNr order: t79, t3, t65, t100 (branch B
    // before branch A).  A spurious fresh `anc` jump head-inserts branch A
    // ahead of branch B and yields the wrong [t65, t100, t79, t3].
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    Term v0 = mk_v(0u), v1 = mk_v(1u);
    // or = mk_f, not = mk_i
    Term lhss[4] = {
      mk_f(mk_i(mk_f(mk_i(v0), v1)), mk_i(mk_f(mk_i(v0), mk_i(v1)))),  // t3
      mk_f(mk_i(mk_f(v0, mk_i(v1))), mk_i(mk_f(mk_i(v1), mk_i(v0)))),  // t65
      mk_f(mk_i(mk_f(mk_i(v0), v1)), mk_i(mk_f(mk_i(v1), mk_i(v0)))),  // t79
      mk_f(mk_i(mk_f(v0, mk_i(v1))), mk_i(mk_f(mk_i(v0), mk_i(v1)))),  // t100
    };
    u32 traces[4] = { 3u, 65u, 79u, 100u };
    for (u32 k = 0; k < 4u; k++) {
      s->lhs[k] = lhss[k];
      s->rhs[k] = mk_v(0u);
      s->r_orient[k] = 1u;
      s->r_trace[k] = traces[k];
      s->n_rules++;
      atp_wmo_insert_fact(s, k);
    }
    {
      AtpWmOrder *w = (AtpWmOrder *)s->wmo;
      // tops query = or(v0, not(or(not(v1), not(v2)))) with a fresh free
      // variable at the first arg (the order-13 face's enclosing position).
      Term q = mk_f(mk_v(2u),
                    mk_i(mk_f(mk_i(mk_v(0u)), mk_i(mk_v(1u)))));
      u32 a3 = 0, a65 = 0, a79 = 0, a100 = 0, ch = 0;
      CHECK_EQ((u32)wmo_tops_rank(w, 0u, q, 3u, 0u, &a3, &ch), 1u);
      CHECK_EQ((u32)wmo_tops_rank(w, 0u, q, 65u, 0u, &a65, &ch), 1u);
      CHECK_EQ((u32)wmo_tops_rank(w, 0u, q, 79u, 0u, &a79, &ch), 1u);
      CHECK_EQ((u32)wmo_tops_rank(w, 0u, q, 100u, 0u, &a100, &ch), 1u);
      // WM CPNr order: 336(t79) < 337(t3) < 338(t65) < 339(t100).
      CHECK(a79 < a3);
      CHECK(a3 < a65);
      CHECK(a65 < a100);
    }
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-split-ancestor-first-chain-node-parallel");
  {
    // WolframAxioms Commutativity / DoubleNegation @91.  A strict-ancestor
    // in-jump to the split victim whose subterm closes at the FIRST chain
    // node (its PositionNachTeilterm == i+1, the last cell being the
    // leaf-found branch key[i], shared by both leaves) and the GleichPfad
    // chain has interior nodes beyond the first (j > i+1).  WM's
    // AltesBlattPolieren re-targets that survivor onto the chain node
    // (else-branch DSBaumOperationen.c :534-560) AND a FRESH jump to that
    // same chain node is head-inserted at the ancestor (the prepend
    // NeuesBlattEinhaengen makes for the fully closed shorter enclosing
    // subterm), so the more-general jump is consulted first.  Dropping the
    // fresh head jump reorders the tops-DFS arrival of the partner rules
    // (firstdiv 91 instead of the full 497-selection prefix on the Wolfram
    // theorems); firing it on a MINIMAL split (j == i+1) instead regresses
    // CombinatorAxioms SKIToBCKW back to @175.
    //
    // Faces reproducing the @91 pattern (sn@1, i=3, j=6, f = binary label 3,
    // a/e = consts):
    //   branch = f(f(v0,a), v0)        forces the victim to be leaf-found at
    //                                  depth 3 (a sibling at the depth-3 node)
    //   victim = f(f(v0,v1), f(v0,v0)) the f(v0,v1) first arg jumps at depth 1
    //   split  = f(f(v0,v1), f(v0,e))  splits victim; f(v0,v1) closes at the
    //                                  first chain node (e_old == i+1)
    // The depth-1 node's HEAD outgoing exit must be the fresh non-leaf
    // (chain-node) jump for f(v0,v1).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    Term v0 = mk_v(0u), v1 = mk_v(1u);
    Term faces[3] = {
      mk_f(mk_f(v0, mk_a()), v0),                  // branch
      mk_f(mk_f(v0, v1), mk_f(v0, v0)),            // victim
      mk_f(mk_f(v0, v1), mk_f(v0, mk_e())),        // splitter
    };
    for (u32 k = 0; k < 3u; k++) {
      s->lhs[k] = faces[k];
      s->rhs[k] = mk_a();
      s->r_orient[k] = 1u;
      s->r_trace[k] = 800u + k;
      s->n_rules++;
      atp_wmo_insert_fact(s, k);
    }
    {
      AtpWmOrder *w = (AtpWmOrder *)s->wmo;
      // navigate root -> f (the depth-1 node).
      WmoCell cf; cf.sym = 3u; cf.is_var = 0u; cf.arity = 2u;
      u8 il = 0;
      WmoNode *d1 = (WmoNode *)wmo_kid_get(w->tree[0].root, &cf, &il);
      CHECK(d1 != NULL && il == 0u);
      // The HEAD exit is the FRESH non-leaf jump for the f(v0,v1) first arg
      // (sub cells f,v0,v1 -> a chain node).  Without the fresh head jump the
      // head would be the branch leaf or the re-targeted survivor lower down.
      WmoEntry *head = d1->exits;
      CHECK(head != NULL);
      CHECK_EQ(head->ziel_leaf, 0u);          // chain node, not a leaf
      CHECK_EQ(head->sub_len, 3u);            // f(v0,v1) = 3 cells
      CHECK_EQ(head->sub[0].is_var, 0u);      // f
      CHECK_EQ(head->sub[0].sym, 3u);
      CHECK_EQ(head->sub[1].is_var, 1u);      // v0
      CHECK_EQ(head->sub[2].is_var, 1u);      // v1
      // The same chain node is reached by TWO entries (fresh head + the
      // re-targeted survivor): the more-general jump is consulted first.
      u32 to_chain = 0;
      for (WmoEntry *en = d1->exits; en != NULL; en = en->next)
        if (!en->ziel_leaf && en->sub_len == 3u && en->ziel == head->ziel)
          to_chain++;
      CHECK_EQ(to_chain, 2u);
    }
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-emission-order-removal-collapse-exit-head");
  {
    // Removal-avalanche exit-order corner (BO_ObjektEntfernen Schrumpfen,
    // DSBaumOperationen.c :991-1101).  When a one-way branch collapses to
    // a single surviving leaf, that leaf re-hangs at the collapse-target
    // node `up` (:1083) and the short jump reaching it is RE-ISSUED, so it
    // PREPENDS to up's outgoing exit list (RumpfSprungeintragSetzen
    // head-insert :293-295); at every other node the freed-node jump
    // rewires in place (AlleEingehendenSpruengeUmsetzen :814).  This was
    // the residual corner that left mc_ioi CP-emission at 38/41 batches.
    //
    // Build the f-node's exit list [R7, R9, S-subtree-jump] (the S jump is
    // OLD: inserted first, pushed down as R7/R9 prepend ahead of it):
    //   S1 = f(f(a,a), i(a))   S2 = f(f(a,a), i(e))   (collapse to S1)
    //   R7 = f(i(f(a,a)), a)   R9 = f(i(i(a)), a)     (older f-node exits)
    // Removing S2 collapses the f(a,a) jump-target subtree to S1; the
    // f-node is `up`, so S1's rewired jump must move to the exit-list HEAD:
    //   [R7, R9, ->S1] -> [S1, R7, R9].
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_emission_order(s, 1u);
    Term faa = mk_f(mk_a(), mk_a());
    Term lhss[4] = {
      mk_f(mk_f(mk_a(), mk_a()), mk_i(mk_a())),   // S1
      mk_f(mk_f(mk_a(), mk_a()), mk_i(mk_e())),   // S2
      mk_f(mk_i(mk_f(mk_a(), mk_a())), mk_a()),   // R7
      mk_f(mk_i(mk_i(mk_a())), mk_a()),           // R9
    };
    (void)faa;
    for (u32 k = 0; k < 4u; k++) {
      s->lhs[k] = lhss[k];
      s->rhs[k] = mk_a();
      s->r_orient[k] = 1u;
      s->r_trace[k] = 700u + k;   // S1=700 S2=701 R7=702 R9=703
      s->n_rules++;
      atp_wmo_insert_fact(s, k);
    }
    {
      AtpWmOrder *w = (AtpWmOrder *)s->wmo;
      // navigate to the f-node: root kid keyed by the lhs cell 0 (f).
      WmoCell cells[WMO_MAX_CELLS];
      u32 n = wmo_face_cells(lhss[0], cells, WMO_MAX_CELLS);
      CHECK(n > 0u);
      u8 is_leaf = 0;
      WmoNode *fnode =
        (WmoNode *)wmo_kid_get(w->tree[0].root, &cells[0], &is_leaf);
      CHECK(fnode != NULL);
      CHECK_EQ(is_leaf, 0u);
      // before removal the S-subtree jump is the LAST exit (head = R9, R7
      // newest-first; the OLD S jump sits at the tail).
      u32 n_exits = 0;
      for (WmoEntry *e = fnode->exits; e != NULL; e = e->next) n_exits++;
      CHECK(n_exits >= 3u);
      // remove S2 -> collapse; S1's rewired jump must head the list.
      atp_wmo_remove_trace(s, 701u);
      WmoNode *fnode2 =
        (WmoNode *)wmo_kid_get(w->tree[0].root, &cells[0], &is_leaf);
      CHECK(fnode2 != NULL);
      // the head exit must now target S1's leaf (trace 700).
      WmoEntry *head = fnode2->exits;
      CHECK(head != NULL);
      CHECK_EQ(head->ziel_leaf, 1u);
      WmoLeaf *hl = (WmoLeaf *)head->ziel;
      u8 found_s1 = 0;
      for (u32 c = 0; c < hl->n_chain; c++) {
        if (hl->chain[c].trace == 700u) found_s1 = 1u;
      }
      CHECK_EQ(found_s1, 1u);
      // R7 (702) and R9 (703) follow, in their original relative order.
      u32 seen_r7 = 0xffffffffu, seen_r9 = 0xffffffffu, idx = 0;
      for (WmoEntry *e = fnode2->exits; e != NULL; e = e->next, idx++) {
        if (e->ziel_leaf) {
          WmoLeaf *l = (WmoLeaf *)e->ziel;
          for (u32 c = 0; c < l->n_chain; c++) {
            if (l->chain[c].trace == 702u) seen_r7 = idx;
            if (l->chain[c].trace == 703u) seen_r9 = idx;
          }
        }
      }
      CHECK(seen_r7 != 0xffffffffu);
      CHECK(seen_r9 != 0xffffffffu);
      CHECK(seen_r7 < seen_r9);   // R7 before R9 (head-prepend kept it)
      CHECK_EQ(idx >= 3u ? 1u : 0u, 1u);
      CHECK(seen_r7 > 0u);        // S1 is strictly ahead of R7
    }
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-intake-canonical-sort-pops-first");
  {
    // WM loader intake (wm_intake.c): the initial axiom set pops in
    // SpezNormierung canonical-sort order, before any derived CP,
    // regardless of computed weight.  Theory:
    //   A1 (input first):  i(x) = x                   (light)
    //   A2 (input second): f(f(f(x,e),e),e) = x       (heavy)
    // Stats: occ_eq f=3, e=3, i=1; canonical symbol order = e (arity
    // 0) < f < i (occ DESC, then arity ASC) -> pos e=0, f=1, i=2.
    // LRSortieren swaps BOTH (var rhs < non-var lhs), so the equation
    // sort compares lhs x vs x (Gleich, equal var ranks) then rhs
    // i(x) vs f(f(f(x,e),e),e): head positions i=2 > f=1 -> A2 is
    // Kleiner and pops FIRST despite being the heavier pair.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_intake_order(s, 1u);
    CHECK(thvm_atp_add_equation(s, mk_i(mk_v(VAR_x)), mk_v(VAR_x)));
    CHECK(thvm_atp_add_equation(
        s, mk_f(mk_f(mk_f(mk_v(VAR_x), mk_e()), mk_e()), mk_e()),
        mk_v(VAR_x)));
    thvm_atp_step(s);
    CHECK_EQ(s->wm_intake_done, 1u);
    CHECK_EQ(s->use_initial_ultimate, 1u);   // initial=ultimate stamp
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(term_ext(s->lhs[0]), LAB_f);    // A2 first (canonical sort)
    // Step 2: the remaining axiom (ultimate) pops before every CP
    // derived from A2's self-overlaps -- WM's MIN_INT floor.
    thvm_atp_step(s);
    CHECK_EQ(s->n_rules, 2u);
    CHECK_EQ(term_ext(s->lhs[1]), LAB_i);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-intake-order-off-roundtrip");
  {
    // Flag OFF (engine default): the same axiom set pops by computed
    // weight -- the light i(x) = x first.  Byte-identical legacy
    // intake; the canonicalization never runs.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ(s->use_wm_intake_order, 0u);
    CHECK(thvm_atp_add_equation(s, mk_i(mk_v(VAR_x)), mk_v(VAR_x)));
    CHECK(thvm_atp_add_equation(
        s, mk_f(mk_f(mk_f(mk_v(VAR_x), mk_e()), mk_e()), mk_e()),
        mk_v(VAR_x)));
    thvm_atp_step(s);
    CHECK_EQ(s->wm_intake_done, 0u);
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(term_ext(s->lhs[0]), LAB_i);    // weight order, A1 first
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/wm-intake-ultimate-fifo-not-weight");
  {
    // Within the ultimate class the heap key is cp_seq alone (WM
    // stamps every Act_ultimate CP w1 = INT32_MIN, so (w1, w2)
    // degenerates to FIFO).  Same axioms in canonical order already
    // (A2 heavy first by intake restamp); a weight-ordered ultimate
    // class would pop the light one first -- assert it does not.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_wm_intake_order(s, 1u);
    // Input order = canonical order (heavy first): the restamp is the
    // identity permutation, so a pri-ordered comparator would still
    // pop the LIGHT axiom first.  FIFO-within-ultimate keeps input.
    CHECK(thvm_atp_add_equation(
        s, mk_f(mk_f(mk_f(mk_v(VAR_x), mk_e()), mk_e()), mk_e()),
        mk_v(VAR_x)));
    CHECK(thvm_atp_add_equation(s, mk_i(mk_v(VAR_x)), mk_v(VAR_x)));
    thvm_atp_step(s);
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(term_ext(s->lhs[0]), LAB_f);
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

  TEST_BEGIN("atp/goal-check-single-goal-latches-joined-mask");
  {
    // The single-conjecture path mirrors its join into the multi-goal
    // bookkeeping: set_goal is set_goals(n=1), so a PROVED check
    // latches bit 0.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_e()), mk_a());
    CHECK_EQ(s->n_goals, 1u);
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_PROVED);
    CHECK_EQ((unsigned)s->goals_joined_mask, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-multi-both-join-proves");
  {
    // Two-goal conjunction, both joinable under f(x, e) -> x:
    //   g0: f(a, e) == a    g1: f(e, e) == e
    // One goal_check joins both, sets mask 0b11, returns PROVED.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;
    Term gl[2] = { mk_f(mk_a(), mk_e()), mk_f(mk_e(), mk_e()) };
    Term gr[2] = { mk_a(),               mk_e()               };
    CHECK_EQ((int)thvm_atp_set_goals(s, gl, gr, 2), 1);
    CHECK_EQ(s->n_goals, 2u);
    CHECK_EQ((unsigned)s->goals_joined_mask, 0u);
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_PROVED);
    CHECK_EQ((unsigned)s->goals_joined_mask, 3u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-multi-partial-join-still-running");
  {
    // g0 joins (f(a, e) -> a), g1 (a == e) cannot: the joined bit
    // latches for g0 only, the check stays RUNNING, and the
    // single-goal alias re-points at the unjoined conjunct so the
    // goal-directed heuristics steer toward it.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;
    Term gl[2] = { mk_f(mk_a(), mk_e()), mk_a() };
    Term gr[2] = { mk_a(),               mk_e() };
    CHECK_EQ((int)thvm_atp_set_goals(s, gl, gr, 2), 1);
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_RUNNING);
    CHECK_EQ((unsigned)s->goals_joined_mask, 1u);
    CHECK(kbo_eq(s->goal_lhs, gl[1]));
    CHECK(kbo_eq(s->goal_rhs, gr[1]));
    // Re-checking is idempotent: the latched bit stays, still RUNNING.
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_RUNNING);
    CHECK_EQ((unsigned)s->goals_joined_mask, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-multi-order-independent");
  {
    // The same conjunct set in the opposite order: the verdicts are
    // order-independent (mask bits track positions, the unjoined
    // conjunct is the one that cannot close either way).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;
    Term gl[2] = { mk_a(), mk_f(mk_a(), mk_e()) };
    Term gr[2] = { mk_e(), mk_a()               };
    CHECK_EQ((int)thvm_atp_set_goals(s, gl, gr, 2), 1);
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_RUNNING);
    CHECK_EQ((unsigned)s->goals_joined_mask, 2u);   // bit 1 = the joinable goal
    // The alias stays on g0 -- the first (and only) unjoined conjunct.
    CHECK(kbo_eq(s->goal_lhs, gl[0]));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/run-multi-goal-both-orders-prove");
  {
    // End-to-end: axiom f(x, e) = x, conjunction of two joinable
    // goals, run to PROVED in both conjunct orders.
    for (int rev = 0; rev < 2; rev++) {
      AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
      Term g0l = mk_f(mk_a(), mk_e()),               g0r = mk_a();
      Term g1l = mk_f(mk_f(mk_a(), mk_e()), mk_e()), g1r = mk_a();
      Term gl[2], gr[2];
      gl[0] = rev ? g1l : g0l;  gr[0] = rev ? g1r : g0r;
      gl[1] = rev ? g0l : g1l;  gr[1] = rev ? g0r : g1r;
      CHECK_EQ((int)thvm_atp_set_goals(s, gl, gr, 2), 1);
      thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
      CHECK_EQ((int)thvm_atp_run(s), (int)ATP_PROVED);
      CHECK_EQ((unsigned)s->goals_joined_mask, 3u);
      thvm_atp_free(s);
    }
  }

#ifdef ATP_MNF
  TEST_BEGIN("atp/run-multi-goal-mnf-only-conjuncts-prove-on-empty-queue");
  {
    // Pre-oriented KBO-INCREASING ground rules (e -> a -> c8): the
    // single-NF goal check cannot close any conjunct (ordered
    // rewriting refuses the increasing direction), only the MNF front
    // search joins them -- and the CP queue is empty from step one.
    // goal_check must therefore give EVERY unjoined conjunct an MNF
    // budget within ONE call: with one join per call the run dies
    // QUEUE_EMPTY right after the first conjunct closes (the
    // regression the multi-goal MNF loop fixes).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_mnf(s, 1);
    Term c1 = mk_e();                     // label 1
    Term c2 = mk_a();                     // label 4
    Term c3 = term_new_ctr(8u, NULL, 0);  // label 8
    thvm_atp_install_oriented_rule(s, c1, c2);
    thvm_atp_install_oriented_rule(s, c2, c3);
    Term gl[2] = { c1, c1 };
    Term gr[2] = { c3, c2 };
    CHECK_EQ((int)thvm_atp_set_goals(s, gl, gr, 2), 1);
    CHECK_EQ((int)thvm_atp_run(s), (int)ATP_PROVED);
    CHECK_EQ((unsigned)s->goals_joined_mask, 3u);
    thvm_atp_free(s);
  }
#endif

  TEST_BEGIN("atp/set-goals-clear-and-cap");
  {
    // n == 0 clears back to completion mode; n > ATP_MAX_GOALS is
    // rejected with the state untouched.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term gl[1] = { mk_a() };
    Term gr[1] = { mk_e() };
    CHECK_EQ((int)thvm_atp_set_goals(s, gl, gr, 1), 1);
    CHECK_EQ(s->n_goals, 1u);
    CHECK(s->goal_lhs != 0u);
    CHECK_EQ((int)thvm_atp_set_goals(s, NULL, NULL, 0), 1);
    CHECK_EQ(s->n_goals, 0u);
    CHECK_EQ((unsigned)s->goal_lhs, 0u);
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_RUNNING);
    CHECK_EQ((int)thvm_atp_set_goals(s, gl, gr, ATP_MAX_GOALS + 1u), 0);
    CHECK_EQ(s->n_goals, 0u);
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

  TEST_BEGIN("atp/implicit-cp-push-queues-trace-backed-descriptor");
  {
    // Deferred-CP (`implicit_pair`) push side: same two-rule setup as
    // the trace-parents test above (the cross-overlap CP `(e, a)`
    // survives the filters), but with use_implicit_cp on the CP must
    // land as a 20-byte descriptor -- cp_packed[i] == NULL, tag bit
    // set, parents cached, push-time priority cached -- and its raw
    // terms must be readable off the slot's TRACE_CP entry (the
    // trace-backed contract atp_cp_implicit_materialize reads from).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_implicit_cp(s, 1u);

    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   s->lhs[0], s->rhs[0]);
    s->n_rules = 1;

    Term lhs1 = mk_f(mk_v(VAR_x), mk_e());
    Term rhs1 = mk_a();
    AtpAddedRange added = thvm_atp_orient_and_add(s, lhs1, rhs1);
    CHECK_EQ(added.count, 1u);
    s->r_trace[1] = atp_trace_push(s, TRACE_ORIENT, s->r_trace[0],
                                   ATP_TRACE_NONE,
                                   s->lhs[1], s->rhs[1]);

    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK(pushed >= 1u);
    CHECK(s->n_cps >= 1u);
    CHECK(s->n_cps_implicit >= 1u);
    CHECK(s->cp_implicit != NULL);
    CHECK(s->cp_is_implicit != NULL);

    u32 n_tagged = 0;
    for (u32 i = 0; i < s->n_cps; i++) {
      u8 tagged = atp_cp_slot_implicit(s, i);
      // Tag-bit invariant: implicit <=> no packed bytes.
      CHECK_EQ(tagged, s->cp_packed[i] == NULL ? 1u : 0u);
      if (!tagged) continue;
      n_tagged++;
      // Descriptor: two real cached parents, push-time priority/weight.
      CHECK(s->cp_implicit[i].parent_a_trace_id != ATP_TRACE_NONE);
      CHECK(s->cp_implicit[i].parent_b_trace_id != ATP_TRACE_NONE);
      CHECK_EQ(s->cp_implicit[i].priority, s->cp_pri[i]);
      CHECK(s->cp_implicit[i].weight >= 2u);
      // Trace-backed materialization contract: the slot's TRACE_CP
      // entry holds the raw unified terms as children 2/3.
      u32 t = s->cp_trace[i];
      CHECK(t != ATP_TRACE_NONE);
      CHECK(t < s->n_trace);
      Term te = s->trace[t];
      CHECK_EQ(term_ext(te), TRACE_CP);
      CHECK(term_ctr_at(te, 2) != 0u);
      CHECK(term_ctr_at(te, 3) != 0u);
    }
    CHECK(n_tagged >= 1u);
    CHECK_EQ(n_tagged, s->n_cps_implicit);

    // Queue-subsumption deliberately skips the implicit passive set
    // (WM has no queue-vs-queue subsumption -- SS_TermpaarSubsummiert-
    // VonGM matches only the ACTIVE set), so the implicit run sees
    // ZERO queue-subsumed drops.  An eager twin of the same setup
    // builds the identical CP set: the cross pair's single root
    // overlap is owned by the (new, old) visit (WM TT/eTT root
    // discipline), so neither run has a symmetric second copy for the
    // filter to drop and the two paths agree exactly.
    CHECK_EQ(s->n_cps_dropped_queue_subsumed, 0u);
    AtpState *e = thvm_atp_init(&DUMMY_CFG, 100);
    e->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    e->rhs[0] = mk_v(VAR_x);
    e->r_trace[0] = atp_trace_push(e, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   e->lhs[0], e->rhs[0]);
    e->n_rules = 1;
    AtpAddedRange eadded = thvm_atp_orient_and_add(
        e, mk_f(mk_v(VAR_x), mk_e()), mk_a());
    e->r_trace[1] = atp_trace_push(e, TRACE_ORIENT, e->r_trace[0],
                                   ATP_TRACE_NONE,
                                   e->lhs[1], e->rhs[1]);
    u32 epushed = thvm_atp_generate_cps(e, eadded);
    CHECK(epushed >= 1u);
    CHECK_EQ(e->n_cps_dropped_queue_subsumed, 0u);
    CHECK_EQ(epushed, pushed);
    CHECK_EQ(e->n_cps_implicit, 0u);
    CHECK(e->cp_is_implicit == NULL);

    thvm_atp_free(e);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/implicit-cp-select-materializes-from-trace");
  {
    // Deferred-CP (`implicit_pair`) select side: same two-rule setup as
    // the push test above (every surviving CP queues as a descriptor),
    // then pop through thvm_atp_select_cp.  The materialized pair must
    // be the slot's TRACE_CP children 2/3 -- pointer-identical, the
    // zero-copy contract -- and the pop must keep the queue invariants
    // (tag bit travels with the backfill, vacated slot cleared).
    // Finally, with orphan murder on and a parent marked dead, the
    // remaining implicit CPs must be discarded at pop (WM
    // selectNonOrphan over the implicit passive set).
    // TWO old rules whose LHS roots both unify with the new rule's
    // f(x, e) -- each cross pair contributes its one root CP (owned
    // by the (new, old) visit per the WM TT/eTT root discipline), so
    // two descriptors queue and one survives the first pop for the
    // orphan-drain check below.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_implicit_cp(s, 1u);

    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   s->lhs[0], s->rhs[0]);
    s->lhs[1] = mk_f(mk_i(mk_v(VAR_x)), mk_e());
    s->rhs[1] = mk_i(mk_a());
    s->r_trace[1] = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   s->lhs[1], s->rhs[1]);
    s->n_rules = 2;

    AtpAddedRange added = thvm_atp_orient_and_add(
        s, mk_f(mk_v(VAR_x), mk_e()), mk_a());
    s->r_trace[2] = atp_trace_push(s, TRACE_ORIENT, s->r_trace[0],
                                   ATP_TRACE_NONE,
                                   s->lhs[2], s->rhs[2]);
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK(pushed >= 2u);
    CHECK_EQ(s->n_cps, pushed);            // no queue-subsume drops
    CHECK_EQ(s->n_cps_implicit, pushed);   // every CP deferred

    // Pop one.  cp_select_count == 0 takes the heap-min branch (j = 0),
    // an implicit slot.
    CHECK(atp_cp_slot_implicit(s, 0) == 1u);
    u32 n_before = s->n_cps;
    Term pl = 0, pr = 0;
    CHECK_EQ(thvm_atp_select_cp(s, &pl, &pr), 1u);
    CHECK_EQ(s->n_cps, n_before - 1u);
    CHECK(pl != 0u);
    CHECK(pr != 0u);
    // Zero-copy: the popped pair IS the trace entry's raw children.
    u32 pt = s->last_popped_trace;
    CHECK(pt != ATP_TRACE_NONE);
    CHECK(pt < s->n_trace);
    Term pte = s->trace[pt];
    CHECK_EQ(term_ext(pte), TRACE_CP);
    CHECK_EQ(pl, term_ctr_at(pte, 2));
    CHECK_EQ(pr, term_ctr_at(pte, 3));
    // Queue invariants after backfill: tag bit <=> NULL packed bytes
    // for every live slot; the vacated tail slot is fully cleared.
    for (u32 i = 0; i < s->n_cps; i++) {
      CHECK_EQ(atp_cp_slot_implicit(s, i),
               s->cp_packed[i] == NULL ? 1u : 0u);
    }
    CHECK(atp_cp_slot_implicit(s, s->n_cps) == 0u);
    CHECK(s->cp_packed[s->n_cps] == NULL);

    // Orphan discard at pop: kill the orient rule's trace id -- every
    // queued CP names it as a parent -- and re-select.  The whole
    // remaining implicit queue must drain as orphans.
    CHECK(s->n_cps >= 1u);
    thvm_atp_set_use_orphan_murder(s, 1u);
    atp_trace_mark_dead(s, s->r_trace[2]);
    Term ol = 0, orr = 0;
    CHECK_EQ(thvm_atp_select_cp(s, &ol, &orr), 0u);
    CHECK_EQ(s->n_cps, 0u);
    CHECK(s->n_cps_dropped_orphan >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/implicit-cp-end-to-end-proves-group-goal");
  {
    // Flag-ON end-to-end: prove the LEFT inverse f(i(a), a) == e from
    // right-id / right-inv / assoc.  Unlike the headline right-inverse
    // goal (closed by an axiom before any derived CP is selected),
    // this one needs genuine completion -- derived rules born from
    // rule-x-rule CPs that queue through the deferred descriptor lane
    // and materialize at selection.  Trajectory may differ from the
    // eager run (no queue-vs-queue subsumption on the implicit passive
    // set; pop-normalize starts from the raw overlap) -- the gate is
    // proof validity, not byte parity.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 200);
    thvm_atp_set_use_implicit_cp(s, 1u);
    thvm_atp_set_goal(s,
                      mk_f(mk_i(mk_a()), mk_a()),
                      mk_e());
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),           mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))), mk_e());
    thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);
    CHECK(s->n_cps_implicit >= 1u);   // the deferred lane was exercised
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/implicit-cp-ir-sweep-materialize-then-normalize");
  {
    // Deferred-CP IR-sweep lifecycle (atp_cp_set_interreduce routes
    // implicit slots through the same WM AP_generic materialize-then-
    // normalize the packed survivors get).  Three implicit slots, one
    // rule R = { f(e, x) -> x }:
    //   A joined:    raw (f(e, a), a)            -> normalizes to (a, a)
    //                -> dropped (tag bit cleared, nothing freed).
    //   B reduced:   raw (f(e, i(a)), i(i(a)))   -> (i(a), i(i(a)))
    //                -> becomes EAGER (packed reduced pair, bit clear).
    //   C unchanged: raw (i(a), a) already in NF -> stays implicit,
    //                NF-witness cookie stamped, push-time keys carried.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_implicit_cp(s, 1u);

    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   s->lhs[0], s->rhs[0]);
    s->n_rules = 1;

    Term a_l  = mk_f(mk_e(), mk_a());
    Term a_r  = mk_a();
    Term b_l  = mk_f(mk_e(), mk_i(mk_a()));
    Term b_r  = mk_i(mk_i(mk_a()));
    Term c_l  = mk_i(mk_a());
    Term c_r  = mk_a();
    u32 ta = atp_trace_push_cp(s, s->r_trace[0], s->r_trace[0],
                               a_l, a_r, NULL, 0);
    u32 tb = atp_trace_push_cp(s, s->r_trace[0], s->r_trace[0],
                               b_l, b_r, NULL, 0);
    u32 tc = atp_trace_push_cp(s, s->r_trace[0], s->r_trace[0],
                               c_l, c_r, NULL, 0);
    CHECK_EQ(atp_cp_implicit_push(s, a_l, a_r, s->r_trace[0], s->r_trace[0],
                                  ta, 0u), 1u);
    CHECK_EQ(atp_cp_implicit_push(s, b_l, b_r, s->r_trace[0], s->r_trace[0],
                                  tb, 0u), 1u);
    CHECK_EQ(atp_cp_implicit_push(s, c_l, c_r, s->r_trace[0], s->r_trace[0],
                                  tc, 0u), 1u);
    CHECK_EQ(s->n_cps, 3u);
    CHECK_EQ(s->n_cps_implicit, 3u);

    u32 deleted0    = s->n_cp_set_ir_deleted;
    u32 reweighted0 = s->n_cp_set_ir_reweighted;
    atp_cp_set_interreduce(s);

    CHECK_EQ(s->n_cps, 2u);
    CHECK_EQ(s->n_cp_set_ir_deleted - deleted0, 1u);       // A dropped
    CHECK_EQ(s->n_cp_set_ir_reweighted - reweighted0, 1u); // B eagerified
    u32 n_impl = 0, n_eager = 0;
    for (u32 i = 0; i < s->n_cps; i++) {
      // Tag-bit invariant survives the sweep compaction.
      CHECK_EQ(atp_cp_slot_implicit(s, i),
               s->cp_packed[i] == NULL ? 1u : 0u);
      if (atp_cp_slot_implicit(s, i)) {
        n_impl++;
        // C: still trace-backed -- materializes the SAME raw pair
        // (pointer-identical trace children), cookie stamped with the
        // current rule-set revision, push-time keys carried (the
        // descriptor cache stays the coherent heap key).
        CHECK_EQ(s->cp_trace[i], tc);
        Term ml = 0, mr = 0;
        atp_cp_slot_read(s, i, &ml, &mr);
        CHECK_EQ(ml, term_ctr_at(s->trace[tc], 2));
        CHECK_EQ(mr, term_ctr_at(s->trace[tc], 3));
        CHECK_EQ(s->cp_last_norm_r_revision[i], s->r_revision);
        CHECK_EQ(s->cp_implicit[i].priority, s->cp_pri[i]);
      } else {
        n_eager++;
        // B: packed reduced pair (i(a), i(i(a))), provenance kept.
        CHECK_EQ(s->cp_trace[i], tb);
        Term el = 0, er = 0;
        atp_cp_slot_read(s, i, &el, &er);
        CHECK(kbo_eq(el, mk_i(mk_a())));
        CHECK(kbo_eq(er, mk_i(mk_i(mk_a()))));
        // Reweighted at the commit site exactly as the packed path
        // does (atp_cp_commit_priorities is pure in (s, l, r)).
        CHECK_EQ(s->cp_pri[i], atp_cp_priority(s, el, er));
      }
    }
    CHECK_EQ(n_impl, 1u);
    CHECK_EQ(n_eager, 1u);
    // The vacated tail slot is fully cleared.
    CHECK(atp_cp_slot_implicit(s, s->n_cps) == 0u);
    CHECK(s->cp_packed[s->n_cps] == NULL);

    // Second sweep: C's cookie matches r_revision (no rule was added
    // in between) so it skips the normalize and stays implicit; B is
    // already in NF so nothing is dropped.  (B may still count as
    // reweighted on the FT build: packed slots witness "unchanged" by
    // POINTER identity and ft_to_term always rebuilds -- the repack is
    // structurally a no-op.)
    u32 deleted1 = s->n_cp_set_ir_deleted;
    atp_cp_set_interreduce(s);
    CHECK_EQ(s->n_cps, 2u);
    CHECK_EQ(s->n_cp_set_ir_deleted, deleted1);
    u32 n_impl2 = 0;
    for (u32 i = 0; i < s->n_cps; i++) {
      if (atp_cp_slot_implicit(s, i)) n_impl2++;
    }
    CHECK_EQ(n_impl2, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/implicit-cp-ir-sweep-drops-orphans");
  {
    // Eager Waisenmord inside the IR sweep covers the implicit passive
    // set: an implicit slot whose TRACE_CP names a dead parent is
    // dropped by the sweep's orphan branch (descriptor bit cleared, no
    // free -- packed is NULL), without paying the materialize+normalize.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_implicit_cp(s, 1u);
    thvm_atp_set_use_orphan_murder(s, 1u);

    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   s->lhs[0], s->rhs[0]);
    s->n_rules = 1;
    // A second trace entry to play the dead parent (no rule attached;
    // the orphan test only reads trace liveness).
    u32 dead_parent = atp_trace_push(s, TRACE_ORIENT, s->r_trace[0],
                                     ATP_TRACE_NONE,
                                     mk_f(mk_v(VAR_x), mk_e()), mk_a());

    // X: orphan (parent_a dead).  Y: live parents.  Both raw-NF, so any
    // drop is attributable to the orphan branch alone.
    Term x_l = mk_i(mk_a()),        x_r = mk_a();
    Term y_l = mk_i(mk_i(mk_a())),  y_r = mk_a();
    u32 tx = atp_trace_push_cp(s, dead_parent, s->r_trace[0],
                               x_l, x_r, NULL, 0);
    u32 ty = atp_trace_push_cp(s, s->r_trace[0], s->r_trace[0],
                               y_l, y_r, NULL, 0);
    CHECK_EQ(atp_cp_implicit_push(s, x_l, x_r, dead_parent, s->r_trace[0],
                                  tx, 0u), 1u);
    CHECK_EQ(atp_cp_implicit_push(s, y_l, y_r, s->r_trace[0], s->r_trace[0],
                                  ty, 0u), 1u);
    CHECK_EQ(s->n_cps, 2u);

    atp_trace_mark_dead(s, dead_parent);
    u32 orphans0 = s->n_cps_dropped_orphan;
    u32 deleted0 = s->n_cp_set_ir_deleted;
    atp_cp_set_interreduce(s);

    CHECK_EQ(s->n_cps, 1u);
    CHECK_EQ(s->n_cps_dropped_orphan - orphans0, 1u);
    CHECK_EQ(s->n_cp_set_ir_deleted - deleted0, 1u);
    // The survivor is Y, still implicit, still trace-backed.
    CHECK_EQ(atp_cp_slot_implicit(s, 0), 1u);
    CHECK_EQ(s->cp_trace[0], ty);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-set-ir-sweep-preserves-insertion-age");
  {
    // WM's AP_generic reweight only recomputes w1; the FIFO age w2 is
    // never changed (C_ReClassify, CLAS/NewClassification.c:399-406
    // "w2 wird nicht geaendert" / w2 is not changed).  The sweep must
    // keep every survivor's cp_seq -- including across the compaction
    // over a dropped slot and through the repack of a reduced CP.
    // R = { f(e, x) -> x }; three packed CPs:
    //   joiner:  (f(e, a), a)            -> (a, a), dropped
    //   reducer: (f(e, i(a)), i(i(a)))   -> repacked reduced
    //   nf:      (i(a), a)               -> untouched
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   s->lhs[0], s->rhs[0]);
    s->n_rules = 1;

    thvm_atp_cp_set(s, 0, mk_f(mk_e(), mk_a()), mk_a());
    thvm_atp_cp_set(s, 1, mk_f(mk_e(), mk_i(mk_a())), mk_i(mk_i(mk_a())));
    thvm_atp_cp_set(s, 2, mk_i(mk_a()), mk_a());
    for (u32 i = 0; i < 3; i++) s->cp_trace[i] = ATP_TRACE_NONE;
    s->n_cps = 3;
    thvm_atp_cp_reheapify(s);

    // Record each CP's set-time age (the slots may have permuted in the
    // heapify; ages travel with the CP).
    u32 seq_reducer = 0, seq_nf = 0, seen = 0;
    for (u32 i = 0; i < s->n_cps; i++) {
      Term l = 0, r = 0;
      atp_cp_slot_read(s, i, &l, &r);
      if (kbo_eq(l, mk_f(mk_e(), mk_i(mk_a())))) {
        seq_reducer = s->cp_seq[i]; seen |= 1u;
      } else if (kbo_eq(l, mk_i(mk_a()))) {
        seq_nf = s->cp_seq[i]; seen |= 2u;
      }
    }
    CHECK_EQ(seen, 3u);
    CHECK(seq_reducer != seq_nf);

    atp_cp_set_interreduce(s);
    CHECK_EQ(s->n_cps, 2u);
    seen = 0;
    for (u32 i = 0; i < s->n_cps; i++) {
      Term l = 0, r = 0;
      atp_cp_slot_read(s, i, &l, &r);
      if (kbo_eq(r, mk_i(mk_i(mk_a())))) {
        // The reduced survivor keeps its set-time age despite the
        // repack + reweight.
        CHECK(kbo_eq(l, mk_i(mk_a())));
        CHECK_EQ(s->cp_seq[i], seq_reducer); seen |= 1u;
      } else if (kbo_eq(r, mk_a())) {
        // The NF survivor keeps its age across the compaction shift
        // over the dropped joiner's slot.
        CHECK_EQ(s->cp_seq[i], seq_nf); seen |= 2u;
      }
    }
    CHECK_EQ(seen, 3u);

    // A reheapify after the sweep (the orphan-kill path's trailing
    // call) must also leave the ages untouched.
    thvm_atp_cp_reheapify(s);
    seen = 0;
    for (u32 i = 0; i < s->n_cps; i++) {
      Term l = 0, r = 0;
      atp_cp_slot_read(s, i, &l, &r);
      if (kbo_eq(r, mk_i(mk_i(mk_a())))) {
        CHECK_EQ(s->cp_seq[i], seq_reducer); seen |= 1u;
      } else if (kbo_eq(r, mk_a())) {
        CHECK_EQ(s->cp_seq[i], seq_nf); seen |= 2u;
      }
    }
    CHECK_EQ(seen, 3u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/implicit-cp-over-bound-falls-back-to-packed-stash");
  {
    // Auto-MaxWeight routing for the deferred lane (the push-site seam
    // in atp_cp_push_batch): the overflow stash is packed-only, so
    // atp_cp_implicit_push signals an over-bound CP by returning 0 and
    // the caller's eager fallback (atp_cp_heap_push) re-tests the bound
    // and parks the CP on the stash as packed bytes.  The force-drain
    // then re-admits it as an ordinary EAGER slot -- an over-bound CP
    // is never implicit at any point of its life, and never lost.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_use_implicit_cp(s, 1u);
    thvm_atp_set_auto_max_cp_weight(s, 1u);

    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = atp_trace_push(s, TRACE_AXIOM,
                                   ATP_TRACE_NONE, ATP_TRACE_NONE,
                                   s->lhs[0], s->rhs[0]);
    s->n_rules = 1;
    // bound = base + slope * deepest-rule-LHS = 1 + 2 * 3 = 7.
    atp_auto_maxw_recompute(s);
    CHECK_EQ(atp_auto_maxw_bound(s), 7u);

    // 8-node CP (over bound): f(f(a, a), f(a, a)) = a.
    Term big_l = mk_f(mk_f(mk_a(), mk_a()), mk_f(mk_a(), mk_a()));
    Term big_r = mk_a();
    u32 tb = atp_trace_push_cp(s, s->r_trace[0], s->r_trace[0],
                               big_l, big_r, NULL, 0);
    // The push-site seam: implicit push refuses (returns 0), eager
    // fallback stashes.
    CHECK_EQ(atp_cp_implicit_push(s, big_l, big_r, s->r_trace[0],
                                  s->r_trace[0], tb, 0u), 0u);
    atp_cp_heap_push(s, big_l, big_r, tb, 0u, 0u);
    CHECK_EQ(s->n_cps, 0u);
    CHECK_EQ(s->n_cp_stash, 1u);
    CHECK_EQ(s->n_cps_implicit, 0u);

    // An under-bound CP still defers: (i(a), a) is 3 nodes.
    Term small_l = mk_i(mk_a());
    Term small_r = mk_a();
    u32 ts = atp_trace_push_cp(s, s->r_trace[0], s->r_trace[0],
                               small_l, small_r, NULL, 0);
    CHECK_EQ(atp_cp_implicit_push(s, small_l, small_r, s->r_trace[0],
                                  s->r_trace[0], ts, 0u), 1u);
    CHECK_EQ(s->n_cps, 1u);
    CHECK_EQ(s->n_cps_implicit, 1u);
    CHECK_EQ(atp_cp_slot_implicit(s, 0), 1u);

    // Pop the small implicit CP -- the live queue empties; the next
    // select force-drains the stash (the auto-MaxWeight completeness
    // lever) and pops the big CP, which re-entered as an EAGER packed
    // slot.  An over-bound CP is thus never implicit at any point of
    // its life, and never lost.
    Term pl = 0, pr = 0;
    CHECK_EQ(thvm_atp_select_cp(s, &pl, &pr), 1u);
    CHECK(kbo_eq(pl, small_l));
    CHECK_EQ(s->n_cps, 0u);
    Term bl = 0, br = 0;
    CHECK_EQ(thvm_atp_select_cp(s, &bl, &br), 1u);
    CHECK_EQ(s->n_cp_stash, 0u);
    CHECK_EQ(s->n_cps, 0u);
    CHECK(kbo_eq(bl, big_l));
    CHECK(kbo_eq(br, big_r));
    CHECK_EQ(s->n_cps_implicit, 1u);   // the big CP never deferred
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
    // A rule's root self-overlap is never built (WM Ausschluss; see
    // atp/generate-cps-single-rule-self-overlap), so the trivially-
    // joinable drop is exercised on the next-simplest shape: two
    // VARIANT rules.  The cross pair's root overlap -- owned by the
    // (new, old) visit -- unifies the two LHS copies and yields a CP
    // whose sides are the same variable; the filter drops it and
    // bumps n_cps_dropped_joinable.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[0] = mk_v(VAR_x);
    s->r_trace[0] = ATP_TRACE_NONE;
    s->lhs[1] = mk_f(mk_e(), mk_v(VAR_x));
    s->rhs[1] = mk_v(VAR_x);
    s->r_trace[1] = ATP_TRACE_NONE;
    s->n_rules = 2;

    AtpAddedRange added = {1, 1, 0};
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

    AtpAddedRange added = {1, 1, 0};
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK(pushed >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-joinability-filter-counter-on-saturation");
  {
    // Full group-axiom saturation: many assoc-driven cross-overlap
    // CPs are trivially joinable.  The goal is a NON-theorem so the
    // run exhausts its step budget instead of closing before the
    // deeper interactions fire; the dropped counter must be > 0.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_a()), mk_e());
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

    AtpAddedRange added = {0, 1, 0};
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

    // atp_push_cps_traced routes queue subsumption through the FV
    // index; resync it from the hand-populated cp_packed[]/n_cps.
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

  TEST_BEGIN("atp/auto-maxweight-preserves-completeness");
  {
    // The automatic growing CP-weight bound (Waldmeister MaxWeight, but
    // never lossy) defers over-bound critical pairs onto an overflow
    // stash instead of dropping them; the stash is force-drained when
    // the active queue empties.  With a TIGHT bound (base 1) most CPs
    // are deferred -- so the group inverse goal proves ONLY IF the
    // stash + drain re-admit them.  Same verdict as the unbounded
    // engine: the bound changes search order, never reachability.
    AtpState *s_unb = thvm_atp_init(&DUMMY_CFG, 64);
    thvm_atp_set_goal(s_unb, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_unb, mk_f(mk_v(VAR_x), mk_e()),            mk_v(VAR_x));
    thvm_atp_add_equation(s_unb, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))), mk_e());
    thvm_atp_add_equation(s_unb,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_unb = thvm_atp_run(s_unb);

    AtpState *s_aut = thvm_atp_init(&DUMMY_CFG, 64);
    thvm_atp_set_auto_max_cp_weight(s_aut, 1u);    // tightest bound
    thvm_atp_set_goal(s_aut, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    thvm_atp_add_equation(s_aut, mk_f(mk_v(VAR_x), mk_e()),            mk_v(VAR_x));
    thvm_atp_add_equation(s_aut, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))), mk_e());
    thvm_atp_add_equation(s_aut,
                          mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    AtpStatus rst_aut = thvm_atp_run(s_aut);

    // Both must reach the same verdict; the unbounded run proves it,
    // so the auto-bounded run must prove it too (completeness).
    CHECK_EQ((int)rst_unb, (int)ATP_PROVED);
    CHECK_EQ((int)rst_aut, (int)rst_unb);
    thvm_atp_free(s_unb);
    thvm_atp_free(s_aut);
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

  // ==================================================================
  // WM backward ground-joinability sterilization (inventory row H:
  // RueckwaertsGrundzusammenfuehrbarkeit, INF/Hauptkomponenten.c:
  // 260-306; -gj default OFF, RUN/Parameter.c:317).  These call the
  // static fact-level halves directly (same TU).
  // ==================================================================
  {
    // Signature: f/1 (label 2, w1), h/2 (label 3, w1), g2/2 (label 4,
    // w1), c/0 (label 5, w1).
    enum { BG_F = 2u, BG_H = 3u, BG_G2 = 4u, BG_C = 5u };
    static u32 bgw[6] = {0, 0, 1, 1, 1, 1};
    static u32 bgp[6] = {0, 0, 4, 3, 2, 1};
    static const KboConfig BG_CFG = {
      .weights = bgw, .precedence = bgp, .n_labels = 6, .var_weight = 1,
    };
    #define MK_BF(t)    ({ Term _c[1] = {(t)};      term_new_ctr(BG_F,  _c, 1); })
    #define MK_BH(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(BG_H,  _c, 2); })
    #define MK_BG2(a, b)({ Term _c[2] = {(a), (b)}; term_new_ctr(BG_G2, _c, 2); })

    // H.1: a queued fact that BECOMES ground-joinable after a new rule
    // arrives is sterilized by the backward walk -- status JOINABLE,
    // counter ticked, queued CP orphaned (KPV_KillParent analog), but
    // the fact STAYS live in R (GZ_ZSFB_BEHALTEN=1 keep-for-rewriting).
    TEST_BEGIN("atp/bwd-ground-join/became-joinable-sterilized");
    {
      AtpState *s = thvm_atp_init(&BG_CFG, 64);
      CHECK_EQ(s->use_bwd_ground_join, 0u);            // WM -gj default OFF
      CHECK_EQ(s->gj_exclude, ATP_GJ_NO_EXCLUDE);
      Term x = mk_v(0u), y = mk_v(1u);
      // victim (slot 0): h(f(x), y) -> h(x, y); 2 distinct vars.
      atp_push_rule(s, MK_BH(MK_BF(x), y), MK_BH(x, y));
      // Self-exclusion guard: in isolation the victim must NOT prove
      // joinable through itself (WM DarfNichtReduzieren).
      CHECK_EQ(atp_gj_fact_test(s, 0u), ATP_GJ_ST_FAILED);
      // New fact (slot 1): f(x) -> x -- now every ground instance of
      // the victim joins through it (a non-root step, no Dreieck gate).
      atp_push_rule(s, MK_BF(x), x);
      // Trace plumbing: victim rule + a queued CP descending from it.
      u32 t0  = atp_trace_push(s, TRACE_ORIENT, ATP_TRACE_NONE,
                               ATP_TRACE_NONE, s->lhs[0], s->rhs[0]);
      u32 t1  = atp_trace_push(s, TRACE_ORIENT, ATP_TRACE_NONE,
                               ATP_TRACE_NONE, s->lhs[1], s->rhs[1]);
      s->r_trace[0] = t0;
      s->r_trace[1] = t1;
      u32 tcp = atp_trace_push(s, TRACE_CP, t0, t1, MK_BH(x, y), MK_BH(y, x));
      CHECK_EQ(atp_cp_is_orphan(s, tcp), 0);
      thvm_atp_set_use_bwd_ground_join(s, 1u);
      CHECK_EQ(s->use_bwd_ground_join, 1u);
      // The walk skips the just-added range [1, 2) = the new fact.
      atp_bwd_ground_join_walk(s, 1u, 2u);
      CHECK_EQ(s->r_gj_status[0], ATP_GJ_ST_JOINABLE);
      CHECK_EQ(s->n_facts_bwd_ground_joinable, 1u);
      CHECK_EQ(atp_cp_is_orphan(s, tcp), 1);           // children orphaned
      CHECK_EQ(s->r_dead[0], 0u);                      // fact kept in R
      CHECK_EQ(s->r_gj_status[1], ATP_GJ_ST_UNKNOWN);  // Neues skipped
      // Walk state restored: the forward CP-drop path stays unscoped.
      CHECK_EQ(s->gj_exclude, ATP_GJ_NO_EXCLUDE);
      CHECK_EQ(s->gj_protect_l, (Term)0);
      CHECK_EQ(s->gj_protect_r, (Term)0);
      thvm_atp_free(s);
    }

    // H.2: a non-ground-joinable fact SURVIVES the walk -- status
    // FAILED (retestable), no sterilize, no orphaning.
    TEST_BEGIN("atp/bwd-ground-join/non-joinable-survives");
    {
      AtpState *s = thvm_atp_init(&BG_CFG, 64);
      Term x = mk_v(0u), y = mk_v(1u);
      Term c = term_new_ctr(BG_C, NULL, 0);
      // victim (slot 0): g2(x, y) -> c; nothing reduces g2(x, y).
      atp_push_rule(s, MK_BG2(x, y), c);
      atp_push_rule(s, MK_BF(x), x);                   // the new fact
      u32 t0 = atp_trace_push(s, TRACE_ORIENT, ATP_TRACE_NONE,
                              ATP_TRACE_NONE, s->lhs[0], s->rhs[0]);
      s->r_trace[0] = t0;
      thvm_atp_set_use_bwd_ground_join(s, 1u);
      atp_bwd_ground_join_walk(s, 1u, 2u);
      CHECK_EQ(s->r_gj_status[0], ATP_GJ_ST_FAILED);
      CHECK_EQ(s->n_facts_bwd_ground_joinable, 0u);
      CHECK_EQ(atp_trace_is_dead(s, t0), 0);
      thvm_atp_free(s);
    }

    // H.3: sticky classifications.  Commutativity is GZ_wertvoll
    // (PROTECT_3_PERMS) -- never sterilized even though provably
    // ground-joinable (F.1d).  <= 1 distinct var / ground facts are
    // GZ_aussichtslos.
    TEST_BEGIN("atp/bwd-ground-join/valuable-and-hopeless-sticky");
    {
      AtpState *s = thvm_atp_init(&BG_CFG, 64);
      Term x = mk_v(0u), y = mk_v(1u);
      Term c = term_new_ctr(BG_C, NULL, 0);
      atp_push_rule(s, MK_BH(x, y), MK_BH(y, x));      // comm (unorientable)
      atp_push_rule(s, MK_BF(x), x);                   // 1 distinct var
      atp_push_rule(s, MK_BF(c), c);                   // ground
      CHECK_EQ(atp_gj_fact_test(s, 0u), ATP_GJ_ST_VALUABLE);
      CHECK_EQ(atp_gj_fact_test(s, 1u), ATP_GJ_ST_HOPELESS);
      CHECK_EQ(atp_gj_fact_test(s, 2u), ATP_GJ_ST_HOPELESS);
      // The walk never sterilizes them: statuses persist, counter 0.
      thvm_atp_set_use_bwd_ground_join(s, 1u);
      atp_bwd_ground_join_walk(s, 3u, 3u);             // no skip range
      CHECK_EQ(s->n_facts_bwd_ground_joinable, 0u);
      CHECK_EQ(s->r_gj_status[0], ATP_GJ_ST_VALUABLE);
      CHECK_EQ(s->r_gj_status[1], ATP_GJ_ST_HOPELESS);
      CHECK_EQ(s->r_gj_status[2], ATP_GJ_ST_HOPELESS);
      thvm_atp_free(s);
    }

    // H.4: Weggefiltert (Unifikation1.c:967-972) -- a sterilized fact
    // forms no CP as either parent.  g2(f(x), y) -> y overlapped with
    // f(c) -> c yields a genuine CP; after marking the parent sterile
    // the SAME overlap yields none.
    TEST_BEGIN("atp/bwd-ground-join/sterile-parent-forms-no-cp");
    {
      AtpState *s = thvm_atp_init(&BG_CFG, 64);
      Term x = mk_v(0u), y = mk_v(1u);
      Term c = term_new_ctr(BG_C, NULL, 0);
      atp_push_rule(s, MK_BG2(MK_BF(x), y), y);
      atp_push_rule(s, MK_BF(MK_BF(c)), c);
      CriticalPair buf[ATP_CP_BATCH];
      u32 before = atp_gen_one(s, 0u, 1u, buf);
      CHECK(before > 0u);
      s->r_gj_status[0] = ATP_GJ_ST_JOINABLE;
      CHECK_EQ(atp_gen_one(s, 0u, 1u, buf), 0u);       // sterile as i
      CHECK_EQ(atp_gen_one(s, 1u, 0u, buf), 0u);       // sterile as j
      thvm_atp_free(s);
    }

    // H.5: Dreieck root protection (Grundzusammenfuehrung.c:235-240 /
    // NFBildung.c:767-779).  A victim whose ONLY join path needs a
    // ROOT step by an EQUALLY-general rule stays FAILED (the gate
    // blocks it); a victim joinable through a STRICTLY-more-general
    // root rule -- the classic instance-redundancy shape -- is
    // sterilized (the gate admits proper encompassment).
    TEST_BEGIN("atp/bwd-ground-join/dreieck-root-protection");
    {
      // p/1 (label 2, w1), r2/1 (label 3, w2), h2/2 (label 4, w3),
      // g4/2 (label 5, w3), g5w/2 (label 6, w2), f2/1 (label 7, w1).
      enum { DK_P = 2u, DK_R2 = 3u, DK_H2 = 4u, DK_G4 = 5u,
             DK_G5W = 6u, DK_F2 = 7u };
      static u32 dkw[8] = {0, 0, 1, 2, 3, 3, 2, 1};
      static u32 dkp[8] = {0, 0, 6, 5, 4, 3, 2, 1};
      static const KboConfig DK_CFG = {
        .weights = dkw, .precedence = dkp, .n_labels = 8, .var_weight = 1,
      };
      #define MK_P1(t)     ({ Term _c[1] = {(t)};      term_new_ctr(DK_P,   _c, 1); })
      #define MK_R2(t)     ({ Term _c[1] = {(t)};      term_new_ctr(DK_R2,  _c, 1); })
      #define MK_H2(a, b)  ({ Term _c[2] = {(a), (b)}; term_new_ctr(DK_H2,  _c, 2); })
      #define MK_G4(a, b)  ({ Term _c[2] = {(a), (b)}; term_new_ctr(DK_G4,  _c, 2); })
      #define MK_G5W(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(DK_G5W, _c, 2); })
      #define MK_F2T(t)    ({ Term _c[1] = {(t)};      term_new_ctr(DK_F2,  _c, 1); })
      AtpState *s = thvm_atp_init(&DK_CFG, 64);
      Term x = mk_v(0u), y = mk_v(1u);
      // BLOCKED case.  victim (slot 0): h2(x, y) -> p(x).  Joins only
      // via slot 1's EQUALLY-general h2(x, y) -> r2(x) at the ROOT
      // (then r2(x) -> p(x)); the anchor h2(x, y) is not strictly
      // encompassed by h2(x, y), so the gate refuses -> FAILED.
      atp_push_rule(s, MK_H2(x, y), MK_P1(x));
      atp_push_rule(s, MK_H2(x, y), MK_R2(x));
      atp_push_rule(s, MK_R2(x), MK_P1(x));
      CHECK_EQ(atp_gj_fact_test(s, 0u), ATP_GJ_ST_FAILED);
      // ADMITTED case.  victim (slot 3): g4(f2(x), y) -> g5w(f2(x), y)
      // is an INSTANCE of slot 4's g4(u, v) -> g5w(u, v); the general
      // rule properly encompasses the anchor g4(f2(x), y), so the gate
      // admits the root step and the victim joins -> JOINABLE.
      atp_push_rule(s, MK_G4(MK_F2T(x), y), MK_G5W(MK_F2T(x), y));
      atp_push_rule(s, MK_G4(x, y), MK_G5W(x, y));
      CHECK_EQ(atp_gj_fact_test(s, 3u), ATP_GJ_ST_JOINABLE);
      thvm_atp_free(s);
      #undef MK_P1
      #undef MK_R2
      #undef MK_H2
      #undef MK_G4
      #undef MK_G5W
      #undef MK_F2T
    }

    // H.6: end-to-end through thvm_atp_step.  Phase 1 saturates the
    // victim h(f(x,y)) = h(f(y,x)) alone -- it is IRREDUCIBLE (ordered
    // rewriting cannot apply commutativity over bare variables, so
    // interreduction can never drop it: the exact residue class row H
    // exists for) and not yet ground-joinable.  Phase 2 feeds
    // commutativity f(x,y) = f(y,x); at ITS step's tail the backward
    // walk re-tests the victim against the extended system, finds every
    // ground instance joins through comm under the instance order, and
    // STERILIZES it: status JOINABLE, parent trace marked dead (queued
    // CPs orphaned), fact kept live in R/E.  With the flag OFF
    // (default = WM -gj default) the same sequence marks nothing.
    TEST_BEGIN("atp/bwd-ground-join/e2e-step-walk-sterilizes");
    {
      enum { E2_F = 2u, E2_H = 3u };
      static u32 e2w[4] = {0, 0, 1, 1};
      static u32 e2p[4] = {0, 0, 2, 1};
      static const KboConfig E2_CFG = {
        .weights = e2w, .precedence = e2p, .n_labels = 4, .var_weight = 1,
      };
      #define MK_EF(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(E2_F, _c, 2); })
      #define MK_EH(t)    ({ Term _c[1] = {(t)};      term_new_ctr(E2_H, _c, 1); })
      for (u32 flag = 0u; flag <= 1u; flag++) {
        AtpState *s = thvm_atp_init(&E2_CFG, 200);
        s->cp_weight_mode = ATP_CP_WEIGHT_ADD;
        thvm_atp_set_use_bwd_ground_join(s, (u8)flag);
        Term x = mk_v(0u), y = mk_v(1u);
        // Phase 1: the victim saturates alone (forward test at birth:
        // FAILED -- comm is not in the system yet).
        thvm_atp_add_equation(s, MK_EH(MK_EF(x, y)), MK_EH(MK_EF(y, x)));
        thvm_atp_run(s);
        u32 vic = ATP_RULE_NONE;
        for (u32 i = 0; i < s->n_rules; i++) {
          if (!s->r_dead[i] && term_tag(s->lhs[i]) == TAG_CTR &&
              term_ext(s->lhs[i]) == E2_H) {
            vic = i;
            break;
          }
        }
        CHECK(vic != ATP_RULE_NONE);
        CHECK_EQ(s->r_gj_status[vic],
                 flag ? ATP_GJ_ST_FAILED : ATP_GJ_ST_UNKNOWN);
        // Phase 2: commutativity arrives; its step's tail walk re-tests
        // the victim against the extended system.
        thvm_atp_add_equation(s, MK_EF(x, y), MK_EF(y, x));
        thvm_atp_run(s);
        // Re-locate the victim: a phase-2 interreduce drop would have
        // compacted the slots (status/trace ride along).
        vic = ATP_RULE_NONE;
        for (u32 i = 0; i < s->n_rules; i++) {
          if (!s->r_dead[i] && term_tag(s->lhs[i]) == TAG_CTR &&
              term_ext(s->lhs[i]) == E2_H) {
            vic = i;
            break;
          }
        }
        CHECK(vic != ATP_RULE_NONE);
        if (flag) {
          CHECK_EQ(s->r_gj_status[vic], ATP_GJ_ST_JOINABLE);
          CHECK(s->n_facts_bwd_ground_joinable >= 1u);
          CHECK_EQ(s->r_dead[vic], 0u);          // kept in R/E for rewriting
          CHECK_EQ(atp_trace_is_dead(s, s->r_trace[vic]), 1);
        } else {
          // Default gating: the walk never ran, nothing is marked.
          CHECK_EQ(s->r_gj_status[vic], ATP_GJ_ST_UNKNOWN);
          CHECK_EQ(s->n_facts_bwd_ground_joinable, 0u);
          CHECK_EQ(atp_trace_is_dead(s, s->r_trace[vic]), 0);
        }
        thvm_atp_free(s);
      }
      #undef MK_EF
      #undef MK_EH
    }

    #undef MK_BF
    #undef MK_BH
    #undef MK_BG2
  }
#endif  // ATP_CP_GROUND_JOIN

#ifdef ATP_FLATTERM_DIFF
  // === flatterm mixed-normalizer differential ========================
  // For many random subjects against several mixed (orientable +
  // unorientable) rule sets, the opt-in flatterm mixed path
  // (use_flatterm=1) must produce the SAME normal form as the default
  // tree mixed path (use_flatterm=0).  The flatterm path is a speed
  // optimization, not a semantics change.
  {
    // One binary symbol `f` (label 1) over which both orientable and
    // unorientable equations live; KBO with f-weight 1.
    static u32 ftw[2] = {0u, 1u};
    static u32 ftp[2] = {0u, 1u};
    static const KboConfig FT_CFG = {
      .weights = ftw, .precedence = ftp, .n_labels = 2u, .var_weight = 1u,
    };
    #define FT_F(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(1u, _c, 2); })

    // Build a couple of mixed rule sets.  set 0: a commutativity-style
    // unorientable equation f(x,y)=f(y,x) PLUS an orientable collapse
    // f(f(x,y),z) -> f(x,z) and f(x,x) -> x.  set 1: associativity-ish.
    AtpState *sa = thvm_atp_init(&FT_CFG, 256u);
    {
      Term x = mk_v(0u), y = mk_v(1u), z = mk_v(2u);
      atp_push_rule(sa, FT_F(x, y), FT_F(y, x));               // unorientable
      atp_push_rule(sa, FT_F(FT_F(x, y), z), FT_F(x, z));      // orientable
      atp_push_rule(sa, FT_F(x, x), x);                        // orientable
    }
    AtpState *sb = thvm_atp_init(&FT_CFG, 256u);
    {
      Term x = mk_v(0u), y = mk_v(1u), z = mk_v(2u);
      atp_push_rule(sb, FT_F(FT_F(x, y), z), FT_F(x, FT_F(y, z))); // orient
      atp_push_rule(sb, FT_F(x, y), FT_F(y, x));                   // unorient
      atp_push_rule(sb, FT_F(FT_F(x, x), x), x);                   // orient
    }
    // set 2: TWO unorientable equations whose LHS faces overlap (both
    // f(_, _) heads), so several unorientable faces are candidates at one
    // position -- exercises the indexed unorientable path's candidate
    // priority (rule asc, l->r before r->l) against the linear scan.  An
    // index that mis-orders r->l-of-rule-i vs l->r-of-rule-(i+1) diverges
    // here even though every set-0/1 position has at most one such face.
    AtpState *sc = thvm_atp_init(&FT_CFG, 256u);
    {
      Term x = mk_v(0u), y = mk_v(1u), z = mk_v(2u);
      atp_push_rule(sc, FT_F(x, y), FT_F(y, x));                   // unorient
      atp_push_rule(sc, FT_F(FT_F(x, y), z), FT_F(FT_F(y, x), z)); // unorient
      atp_push_rule(sc, FT_F(x, FT_F(y, z)), FT_F(y, FT_F(x, z))); // unorient
      atp_push_rule(sc, FT_F(x, x), x);                            // orient
    }

    // Deterministic random subject generator: variables fv(0..2) and
    // binary f, capped depth so terms stay under the flat cap.
    u32 rng = 0x1234567u;
    #define FT_RND() (rng = rng * 1103515245u + 12345u, rng >> 8)
    u32 mism = 0u, total = 0u;
    u32 resume_mism = 0u;     // flatterm resume-ON vs resume-OFF normal form
    for (u32 t = 0; t < 4000u; t++) {
      AtpState *s = (t % 3u == 0u) ? sc : ((t & 1u) ? sb : sa);
      // Build a random subject bottom-up: start with `budget` random
      // variable leaves over fv(0..2), then repeatedly combine two pool
      // entries under f until one term remains.
      u32  budget = 3u + (FT_RND() % 8u);
      Term subj;
      {
        Term pool[32]; u32 np = 0u;
        for (u32 k = 0; k < budget && np < 32u; k++) {
          pool[np++] = mk_v(FT_RND() % 3u);
        }
        while (np > 1u) {
          u32 i = FT_RND() % np;
          u32 j = FT_RND() % np;
          if (i == j) j = (j + 1u) % np;
          Term f = FT_F(pool[i], pool[j]);
          u32 lo = i < j ? i : j, hi = i < j ? j : i;
          pool[hi] = pool[np - 1u]; np--;
          pool[lo] = pool[np - 1u]; np--;
          pool[np++] = f;
        }
        subj = pool[0];
      }
      total++;
      u64 cp = thvm_atp_heap_checkpoint();
      s->use_flatterm = 0u;
      Term nf_tree = atp_rewrite_normalize(s, subj, s->lhs, s->rhs,
                                           s->n_rules, 4096u);
      s->use_flatterm = 1u;
      Term nf_flat = atp_rewrite_normalize(s, subj, s->lhs, s->rhs,
                                           s->n_rules, 4096u);
      // Resume-ON (the default) vs resume-OFF unorientable scan: the
      // incremental-resume watermark must not change the normal form.
      s->ft_unorient_resume = 0u;
      Term nf_noresume = atp_rewrite_normalize(s, subj, s->lhs, s->rhs,
                                               s->n_rules, 4096u);
      s->ft_unorient_resume = 1u;
      s->use_flatterm = 0u;
      if (!kbo_eq(nf_flat, nf_noresume)) {
        resume_mism++;
        if (resume_mism <= 3u) {
          char a[512], b[512], c[512];
          atp_pretty_term(subj, a, sizeof a);
          atp_pretty_term(nf_flat, b, sizeof b);
          atp_pretty_term(nf_noresume, c, sizeof c);
          fprintf(stderr, "FLATTERM RESUME DIFF: subj=%s on=%s off=%s\n",
                  a, b, c);
        }
      }
      if (!kbo_eq(nf_tree, nf_flat)) {
        mism++;
        if (mism <= 3u) {
          char a[512], b[512], c[512];
          atp_pretty_term(subj, a, sizeof a);
          atp_pretty_term(nf_tree, b, sizeof b);
          atp_pretty_term(nf_flat, c, sizeof c);
          fprintf(stderr, "FLATTERM DIFF: subj=%s tree=%s flat=%s\n", a, b, c);
        }
      }
      thvm_atp_heap_reset(cp);
      // heap_reset recycles cell integers across subjects; the persistent
      // KBO weight memo is keyed by cell integer, so a recycled integer
      // would alias a stale entry.  The live engine bumps the epoch on GC;
      // this differential never GCs, so invalidate explicitly here.
      thvm_kbo_invalidate();
    }
    TEST_BEGIN("atp/flatterm-mixed-normalize-differential");
    {
      CHECK_EQ(mism, 0u);
      CHECK_EQ(resume_mism, 0u);
      CHECK(total >= 4000u);
    }
    thvm_atp_free(sa);
    thvm_atp_free(sb);
    thvm_atp_free(sc);
    #undef FT_F
    #undef FT_RND
  }

  // === DEEP flatterm differential (saturation-grown rule set) =========
  // The shallow random differential above never builds a rule set deep
  // enough to expose the splice/scan-interleaving staleness that the live
  // AndAssociativity-over-Sheffer workload hits at a few hundred rules:
  // an in-place atp_ri_splice leaves an ANCESTOR's cached flat[] tree cell
  // stale (only child cells + subsz/flatsym are updated), so a subsequent
  // orientable index query that binds a rule var to that interior position
  // -- or a heap_reset that recycles a loc the KBO weight memo still keys
  // -- reads stale state and the flatterm NF diverges from the tree NF.
  //
  // This block runs the ACTUAL andassoc saturation with use_flatterm=1.
  // Under ATP_FLATTERM_DIFF the dispatch in atp_rewrite_normalize_ordered
  // runs BOTH the flat and tree normalizers on every live goal-check /
  // critical-pair reduction and bumps g_atp_ft_diff_mism on any flat-NF !=
  // tree-NF -- the REAL deep critical-pair subjects the engine reduces, the
  // exact class the random battery cannot construct.
  {
    extern u32 g_atp_ft_diff_mism;
    g_atp_ft_diff_mism = 0u;
    // One binary symbol nand (label 1) + ground constants p/q/r
    // (labels 2/3/4).  Precedence nand(1) highest, constants p>q>r below --
    // matches the bench's andassoc config (the orientation order that
    // derives the self-overlapping rule set the divergence lives in).
    static u32 shw[5] = {0u, 1u, 1u, 1u, 1u};
    static u32 shp[5] = {0u, 4u, 3u, 2u, 1u};
    static const KboConfig SH_CFG = {
      .weights = shw, .precedence = shp, .n_labels = 5u, .var_weight = 1u,
    };
    #define SH_ND(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(1u, _c, 2); })
    #define SH_K(lbl)   term_new_ctr((lbl), NULL, 0)
    // One engine, use_flatterm=1, driving the AndAssociativity goal (which
    // does NOT close on this axiom -- saturation runs to the rule cap, so
    // goal-check + critical-pair reductions deepen R to 200-500 rules).
    // Under ATP_FLATTERM_DIFF the dispatch runs BOTH the flat and tree
    // normalizers on every live normalize, bumps g_atp_ft_diff_mism on any
    // flat-NF != tree-NF, and RETURNS the tree result so the saturation
    // stays on the proven trajectory (a buggy flat NF would otherwise steer
    // R onto a different path that dodges its own later divergences).  This
    // reproduces the exact live divergence the in-engine self-check aborts
    // on -- the splice/scan staleness on deep goal-check / CP subjects --
    // as a CHECKable g_atp_ft_diff_mism == 0; no offline random subject
    // battery reconstructs those internal critical pairs.
    AtpState *s = thvm_atp_init(&SH_CFG, 4096u);
    s->use_flatterm = 1u;
    {
      Term a = mk_v(0u), b = mk_v(1u), c = mk_v(2u);
      Term ax = SH_ND(SH_ND(SH_ND(a, b), c),
                      SH_ND(a, SH_ND(SH_ND(a, c), a)));
      thvm_atp_add_equation(s, ax, c);
      Term p = SH_K(2u), q = SH_K(3u), rr = SH_K(4u);
      Term qr = SH_ND(SH_ND(q, rr), SH_ND(q, rr));      // And(q,r)
      Term pq = SH_ND(SH_ND(p, q), SH_ND(p, q));        // And(p,q)
      Term gl = SH_ND(SH_ND(p, qr), SH_ND(p, qr));      // And(p, And(q,r))
      Term gr = SH_ND(SH_ND(pq, rr), SH_ND(pq, rr));    // And(And(p,q), r)
      thvm_atp_set_goal(s, gl, gr);
    }
    u32 max_rules = 0u;
    for (u32 i = 0; i < 6000u && s->n_rules < 560u; i++) {
      if (thvm_atp_step(s) != ATP_RUNNING) break;
      if (s->n_rules > max_rules) max_rules = s->n_rules;
    }
    TEST_BEGIN("atp/flatterm-deep-saturation-differential");
    {
      // Every live goal-check / critical-pair reduction over this deep,
      // self-overlapping R (~350 rules -- well past the ~200-rule depth
      // where the staleness first bit) self-checked flat-NF == tree-NF
      // in-dispatch.  Zero divergence -- the splice/scan staleness is gone
      // on the deep workload.  (The exhaustive deep guard is the
      // ATP_FLATTERM_SELFCHECK build: `make ATP_FLATTERM_SELFCHECK=1
      // bin/test_atp_wolfram_bench && THVM_ATP_FLATTERM=1
      // ./bin/test_atp_wolfram_bench andassoc 5000 240` -- zero mismatches.)
      CHECK_EQ(g_atp_ft_diff_mism, 0u);    // flat NF == tree NF on every live normalize
      CHECK(max_rules >= 300u);            // genuinely deep rule set exercised
    }
    thvm_atp_free(s);
    #undef SH_ND
    #undef SH_K
  }
#endif  // ATP_FLATTERM_DIFF

  // === atp/precedence: Waldmeister structural precedence generator ====
  //
  // atp_auto_precedence (src/atp/precedence.c) ports Waldmeister's
  // Praezedenzgenerator: it walks the axiom set, tags each operator's
  // algebraic properties (commutative / associative / inverse / unit /
  // distributes), and emits a numeric precedence.  These tests pin the
  // generated ordering against the canonical group precedence that real
  // Waldmeister .pr files hand-specify (i > f > e > a), then confirm the
  // output is consumable by the live thvm_kbo ordering.
  {
    // Group axioms over {e:0, i:1, f:2} with constant a (LAB_e=1, LAB_i=2,
    // LAB_f=3, LAB_a=4; n_labels=5).
    Term ax_l[3], ax_r[3];
    // assoc: f(f(x0,x1),x2) = f(x0,f(x1,x2))
    ax_l[0] = mk_f(mk_f(mk_v(0u), mk_v(1u)), mk_v(2u));
    ax_r[0] = mk_f(mk_v(0u), mk_f(mk_v(1u), mk_v(2u)));
    // left identity: f(e,x0) = x0
    ax_l[1] = mk_f(mk_e(), mk_v(0u));
    ax_r[1] = mk_v(0u);
    // left inverse: f(i(x0),x0) = e
    ax_l[2] = mk_f(mk_i(mk_v(0u)), mk_v(0u));
    ax_r[2] = mk_e();

    u32 prec[5] = {0u};
    u32 n_seen = atp_auto_precedence(ax_l, ax_r, 3u, 5u, prec);

    TEST_BEGIN("atp/auto-precedence/group-order");
    {
      // Four symbols seen (e, i, f, a -- a only via the structural
      // pattern? no: a never appears in these axioms, so only e,i,f).
      CHECK_EQ(n_seen, 3u);
      // Ranks are 1..n ascending by structural score; the inverse symbol
      // sits highest, the binary product next, the unit constant lowest.
      CHECK_EQ(prec[LAB_i], 3u);   // inverse symbol -- top
      CHECK_EQ(prec[LAB_f], 2u);   // binary product -- middle
      CHECK_EQ(prec[LAB_e], 1u);   // unit constant -- bottom of the seen set
      CHECK_EQ(prec[LAB_a], 0u);   // unseen -> sentinel rank 0
    }

    TEST_BEGIN("atp/auto-precedence/orients-group-kbo");
    {
      // Feed the generated precedence into a unit-weight KBO config and
      // confirm every group axiom orients left-to-right (lhs > rhs) -- the
      // convergent completion direction.  This proves the structural
      // generator's output is directly consumable by the live ordering.
      static u32 gw[5] = {0u, 1u, 1u, 1u, 1u};
      KboConfig gcfg = {
        .weights = gw, .precedence = prec, .n_labels = 5u, .var_weight = 1u,
      };
      CHECK_EQ((int)thvm_kbo(ax_l[0], ax_r[0], &gcfg), (int)KBO_GT);  // assoc
      CHECK_EQ((int)thvm_kbo(ax_l[1], ax_r[1], &gcfg), (int)KBO_GT);  // left-id
      CHECK_EQ((int)thvm_kbo(ax_l[2], ax_r[2], &gcfg), (int)KBO_GT);  // left-inv
    }
  }

  TEST_BEGIN("atp/auto-precedence/ring-distributor-and-ac");
  {
    // Ring-flavoured axioms over {plus:5, times:6}: + is AC, * distributes
    // over +.  The distributor must outrank what it distributes over
    // (Sinai Ring precedence "*+"), and an AC operator is demoted within
    // its arity band so a non-AC binary operator at the same arity wins.
    #define LAB_plus  5u
    #define LAB_times 6u
    #define LAB_g     7u            // a plain (non-AC) binary operator
    #define MK2(lab, x, y) ({ Term _c[2] = {(x), (y)}; term_new_ctr((lab), _c, 2); })
    #define MK_PL(x, y) MK2(LAB_plus, (x), (y))
    #define MK_TI(x, y) MK2(LAB_times, (x), (y))
    #define MK_G(x, y)  MK2(LAB_g, (x), (y))

    Term rl[5], rr[5];
    // + commutative: +(x0,x1) = +(x1,x0)
    rl[0] = MK_PL(mk_v(0u), mk_v(1u));  rr[0] = MK_PL(mk_v(1u), mk_v(0u));
    // + associative: +(+(x0,x1),x2) = +(x0,+(x1,x2))
    rl[1] = MK_PL(MK_PL(mk_v(0u), mk_v(1u)), mk_v(2u));
    rr[1] = MK_PL(mk_v(0u), MK_PL(mk_v(1u), mk_v(2u)));
    // left distributivity of * over +: *(x0,+(x1,x2)) = +(*(x0,x1),*(x0,x2))
    rl[2] = MK_TI(mk_v(0u), MK_PL(mk_v(1u), mk_v(2u)));
    rr[2] = MK_PL(MK_TI(mk_v(0u), mk_v(1u)), MK_TI(mk_v(0u), mk_v(2u)));
    // g commutative only (not associative) -> not AC: g(x0,x1) = g(x1,x0)
    rl[3] = MK_G(mk_v(0u), mk_v(1u));   rr[3] = MK_G(mk_v(1u), mk_v(0u));

    u32 prec[8] = {0u};
    u32 n_seen = atp_auto_precedence(rl, rr, 4u, 8u, prec);
    CHECK_EQ(n_seen, 3u);                       // plus, times, g
    CHECK(prec[LAB_times] > prec[LAB_plus]);     // distributor above distributee
    CHECK(prec[LAB_g]     > prec[LAB_plus]);     // non-AC binop above AC binop (same arity)

    #undef MK2
    #undef MK_PL
    #undef MK_TI
    #undef MK_G
    #undef LAB_plus
    #undef LAB_times
    #undef LAB_g
  }

#ifdef THVM_ATP_AC
  TEST_BEGIN("atp/perm-subsume/wm-ac-dispensable-flattens-n-ary");
  {
    // Port of WM GZ_ACVerzichtbar (INF/Grundzusammenfuehrung.c:137): the
    // AC-permutation-redundancy filter that closes the Huntington/Boolean/
    // Ring/Meredith over-formation.  With the perm-subsume mask carrying
    // an AC operator (or = label 4), atp_cp_perm_subsumed must:
    //   * DROP n-ary associativity permutations the binary swap misses
    //     (or(a,or(b,or(c,d))) = or(b,or(c,or(d,a))) -- same multiset),
    //   * DROP a permutation under a non-AC head (not(or(a,b))=not(or(b,a))),
    //   * KEEP the generating commutativity axiom (or(a,b)=or(b,a)),
    //   * KEEP the generating associativity axiom,
    //   * KEEP a genuinely different equation (or(a,b)=a),
    //   * be INERT (return 0) on a non-AC top when the mask is the
    //     dedicated perm-subsume mask, not the engine AC mask.
    #define LAB_or  4u
    #define LAB_not 3u
    #define MK_OR(x, y) ({ Term _c[2] = {(x), (y)}; term_new_ctr(LAB_or, _c, 2); })
    #define MK_NOT(x)   ({ Term _c1[1] = {(x)}; term_new_ctr(LAB_not, _c1, 1); })

    u64 prev_mask = g_atp_perm_subsume_mask;
    thvm_atp_set_perm_subsume_mask(1ull << LAB_or);   // mark `or` AC

    // 4-ary AC perm: or(v0,or(v1,or(v2,v3))) = or(v1,or(v2,or(v3,v0)))
    Term l4 = MK_OR(mk_v(0u), MK_OR(mk_v(1u), MK_OR(mk_v(2u), mk_v(3u))));
    Term r4 = MK_OR(mk_v(1u), MK_OR(mk_v(2u), MK_OR(mk_v(3u), mk_v(0u))));
    CHECK_EQ(atp_cp_perm_subsumed(l4, r4), 1u);       // DROP (the prior gap)

    // 3-ary AC perm that is NOT a generating form: or(v0,or(v1,v2)) =
    // or(v0,or(v2,v1)) -- v0 stays leading, only the inner pair swaps.
    // Same multiset {v0,v1,v2}, but none of WM's extended-commutativity
    // C' shapes (which all rotate v0 out of the lead), so it is dropped.
    Term l3 = MK_OR(mk_v(0u), MK_OR(mk_v(1u), mk_v(2u)));
    Term r3 = MK_OR(mk_v(0u), MK_OR(mk_v(2u), mk_v(1u)));
    CHECK_EQ(atp_cp_perm_subsumed(l3, r3), 1u);       // DROP

    // The extended-commutativity C' generating form IS kept (WM keeps
    // it in E): or(v0,or(v1,v2)) = or(v1,or(v0,v2)) is one of the four
    // TO_IstErweiterteKommutativitaet rotations.
    Term lec = MK_OR(mk_v(0u), MK_OR(mk_v(1u), mk_v(2u)));
    Term rec = MK_OR(mk_v(1u), MK_OR(mk_v(0u), mk_v(2u)));
    CHECK_EQ(atp_cp_perm_subsumed(lec, rec), 0u);     // KEEP (ext-comm C')

    // permutation under a NON-AC head: not(or(v0,v1)) = not(or(v1,v0))
    Term ln = MK_NOT(MK_OR(mk_v(0u), mk_v(1u)));
    Term rn = MK_NOT(MK_OR(mk_v(1u), mk_v(0u)));
    CHECK_EQ(atp_cp_perm_subsumed(ln, rn), 1u);       // DROP (non-AC top)

    // KEEP the generating commutativity axiom: or(v0,v1) = or(v1,v0)
    Term lc = MK_OR(mk_v(0u), mk_v(1u));
    Term rc = MK_OR(mk_v(1u), mk_v(0u));
    CHECK_EQ(atp_cp_perm_subsumed(lc, rc), 0u);       // KEEP (it IS comm)

    // KEEP the generating associativity axiom:
    //   or(or(v0,v1),v2) = or(v0,or(v1,v2))
    Term la = MK_OR(MK_OR(mk_v(0u), mk_v(1u)), mk_v(2u));
    Term ra = MK_OR(mk_v(0u), MK_OR(mk_v(1u), mk_v(2u)));
    CHECK_EQ(atp_cp_perm_subsumed(la, ra), 0u);       // KEEP (it IS assoc)

    // KEEP a genuinely distinct equation (NOT AC-equal): or(v0,v1) = v0
    Term ld = MK_OR(mk_v(0u), mk_v(1u));
    Term rd = mk_v(0u);
    CHECK_EQ(atp_cp_perm_subsumed(ld, rd), 0u);       // KEEP (not AC-equal)

    // DROP a REPEATED-VARIABLE AC perm: or(v0,or(v0,v1)) = or(v0,or(v1,v0)).
    // Both sides flatten to the multiset {v0,v0,v1}; the top is AC; and it
    // is NOT a generating axiom (the C' rotations require three DISTINCT
    // vars, so atp_gj_perm_valuable rejects the repeated-var shape).  This
    // is the Huntington DoubleNegation @58 thvm-only fork that WM's
    // GZ_ACVerzichtbar drops at selection.
    Term lrv = MK_OR(mk_v(0u), MK_OR(mk_v(0u), mk_v(1u)));
    Term rrv = MK_OR(mk_v(0u), MK_OR(mk_v(1u), mk_v(0u)));
    CHECK_EQ(atp_cp_perm_subsumed(lrv, rrv), 1u);     // DROP (repeated-var AC perm)

    // INERT when the mask does NOT include the operator: clearing the
    // mask routes to the binary-swap fallback, which cannot flatten the
    // 4-ary perm -> keeps it (the pre-port behaviour).
    thvm_atp_set_perm_subsume_mask(0ull);
    CHECK_EQ(atp_cp_perm_subsumed(l4, r4), 0u);       // binary fallback misses it

    thvm_atp_set_perm_subsume_mask(prev_mask);        // restore engine state

    #undef MK_OR
    #undef MK_NOT
    #undef LAB_or
    #undef LAB_not
  }
#endif

  TEST_BEGIN("atp/auto-precedence/mccune-arity-fallback");
  {
    // McCune's single-axiom group/Sheffer-style equation uses {and:arity2,
    // not:arity1}; no Sinai pattern matches its dense form, so the
    // structural detection sees no commutativity/associativity/inverse/
    // identity/distributivity.  Praezedenzgenerator must then fall
    // through to pure Fuchs arity scoring -- which ranks `and` (arity 2)
    // above `not` (arity 1).  This is the same ordering Vampire reaches
    // via `to=lpo:sp=arity` in its cracking config for
    // McCuneAxioms/EqualityOfInverses; thvm's WL `wl_precedence[i]=i+1`
    // bridge default is label-index-arbitrary, so auto-prec genuinely
    // changes the ordering once wired into the WL surface.
    //
    // Axiom (TPTP): and(X0, not(and(X1, and(and(and(X2, not(X2)),
    //                                          not(and(X3, X1))), X0)))) = X3
    #define LAB_AND 10u
    #define LAB_NOT 11u
    #define MK_AND(a, b) ({ Term _c[2] = {(a), (b)}; term_new_ctr(LAB_AND, _c, 2); })
    #define MK_NOT(a)    ({ Term _c[1] = {(a)};      term_new_ctr(LAB_NOT, _c, 1); })

    Term x0 = mk_v(0u), x1 = mk_v(1u), x2 = mk_v(2u), x3 = mk_v(3u);
    Term inner = MK_AND(MK_AND(x2, MK_NOT(x2)), MK_NOT(MK_AND(x3, x1)));
    Term mid   = MK_AND(inner, x0);
    Term outer = MK_AND(x1, mid);
    Term lhs   = MK_AND(x0, MK_NOT(outer));
    Term rhs   = x3;

    u32 prec[12] = {0u};
    u32 n_seen   = atp_auto_precedence(&lhs, &rhs, 1u, 12u, prec);
    CHECK_EQ(n_seen, 2u);              // only `and` and `not` appear
    CHECK_EQ(prec[LAB_AND], 2u);       // arity-2 -> top of the seen set
    CHECK_EQ(prec[LAB_NOT], 1u);       // arity-1 -> next
    // Sanity: the auto-derived ordering DIFFERS from the trivial label-
    // index default (which would give prec[10]=11, prec[11]=12 -> NOT
    // outranks AND, the inverse direction).
    CHECK(prec[LAB_AND] > prec[LAB_NOT]);

    #undef MK_AND
    #undef MK_NOT
    #undef LAB_AND
    #undef LAB_NOT
  }

  TEST_BEGIN("atp/occurrence-precedence/rare-outranks-common");
  {
    // Vampire `sp=occurrence` / E `-G InvFreqRank`: rank by ASCENDING
    // occurrence count.  Build axioms with four distinct counts so a
    // unique rank lands on each symbol.
    //   f(e, a) = a                          -- e=1, f=1, a=2
    //   f(a, i(a)) = a                       -- f=1, a=3, i=1
    //   f(a, f(i(a), a)) = a                 -- f=2, a=4, i=1
    // Cumulative CTR counts: e=1, i=2, f=4, a=9.
    // Ascending: e(1) < i(2) < f(4) < a(9).
    // Ranks (rare = high): e -> 4, i -> 3, f -> 2, a -> 1.
    Term oa_l[3], oa_r[3];
    oa_l[0] = mk_f(mk_e(), mk_a());          oa_r[0] = mk_a();
    oa_l[1] = mk_f(mk_a(), mk_i(mk_a()));    oa_r[1] = mk_a();
    oa_l[2] = mk_f(mk_a(), mk_f(mk_i(mk_a()), mk_a()));
    oa_r[2] = mk_a();

    u32 prec[5] = {0u};
    u32 n_seen = atp_occurrence_precedence(oa_l, oa_r, 3u, 5u, prec);
    CHECK_EQ(n_seen, 4u);              // e, i, f, a all appear
    CHECK_EQ(prec[LAB_e], 4u);         // count 1 -> top (rarest)
    CHECK_EQ(prec[LAB_i], 3u);         // count 2
    CHECK_EQ(prec[LAB_f], 2u);         // count 4
    CHECK_EQ(prec[LAB_a], 1u);         // count 9 -> bottom
    // Distinguishes from auto_precedence on the same axioms (which
    // would key off arity, not frequency).
    CHECK(prec[LAB_e] > prec[LAB_a]);
  }

  thvm_free();
  TEST_REPORT();
}
