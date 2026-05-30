// test_atp_wolfram_bench.c -- standalone completion-engine bench.
//
// Runs a single-Wolfram-axiom completion directly against the C
// engine (thvm_atp_init / _step), printing the rule / CP-queue
// trajectory.  Milestone-7b convergence probe + inference-ladder
// debugger.
//
// Not a pass/fail test (not in the Makefile TESTS list) -- build
// with `make bin/test_atp_wolfram_bench` and run:
//   bin/test_atp_wolfram_bench [goal] [step_cap] [wall_cap_s]
// goal in { thm, cpl1, cpl2, subl2, chain3, chain4, chain6, deep5,
// cpgen } (default thm).  chain3/chain4/chain6/deep5 are multi-step
// join goals -- the rungs between the distance-1 lemmas and the
// distance-54 thm -- and all join+verify cleanly.
//
// The "ladder" idea (debug whether inference is even right): WL's
// own FindEquationalProof[DoubleNegation, WolframAxioms] proof is a
// DAG of CriticalPairLemma + SubstitutionLemma nodes.  cpl1 / cpl2
// are critical pairs of the axiom with itself (distance 1 from the
// axiom); subl2 is ~4 inferences deep; thm is the 54-step target.
// If the engine cannot prove a distance-1 lemma, the inference
// (CP generation / orientation / goal-check) is broken -- not just
// slow.
//
// Unlike a `timeout`-wrapped wolframscript probe, this is a plain C
// process with an in-loop wall-clock guard: it always exits
// cleanly and can never orphan a WolframKernel.

#include "../src/thvm.c"
#include <time.h>

// nand is the single binary function symbol (CTR label 1).
#define L_NAND 1u

static Term nand2(Term x, Term y) {
  Term c[2] = { x, y };
  return term_new_ctr(L_NAND, c, 2);
}
static Term fv(u32 id) { return term_new_fvr(id); }

// Ground skolem constants: nullary CTRs with their own labels (p=2,
// q=3, r=4).  Distinct from the axiom variables a,b,c (TAG_FVR) so a
// rule lhs over a,b,c can match a ground subterm but the goal sides
// stay rigid -- a genuine ground equation, not a schema.  Labels are
// inside n_labels (see cfg below) so KBO gives each a positive weight.
#define L_P 2u
#define L_Q 3u
#define L_R 4u
// McCune-axiom symbols (binary and, unary not): arity 2 + arity 1.
#define L_AND 5u
#define L_NOT 6u
static Term konst(u32 label) { return term_new_ctr(label, NULL, 0); }
static Term and_op(Term x, Term y) {
  Term c[2] = { x, y };
  return term_new_ctr(L_AND, c, 2);
}
static Term not_op(Term x) {
  Term c[1] = { x };
  return term_new_ctr(L_NOT, c, 1);
}

// Wolfram axiom:
//   nand(nand(nand(a,b),c), nand(a,nand(nand(a,c),a))) == c
// axiom_inst(a,b,c) is the LHS with its three variables free: for ANY
// a, b, c it matches the axiom rule and rewrites to `c` in one step.
static Term axiom_inst(Term a, Term b, Term c) {
  return nand2(nand2(nand2(a, b), c),
               nand2(a, nand2(nand2(a, c), a)));
}
static Term axiom_lhs(void) { return axiom_inst(fv(0), fv(1), fv(2)); }

// Goal equations.  *_lhs / *_rhs build the two sides; free
// variables fv(0..3) = a,b,c,(e2|w) are universally quantified.

// thm = DoubleNegation: nand(nand(w,w),nand(w,w)) == w
static void goal_thm(Term *l, Term *r) {
  Term w = fv(3);
  *l = nand2(nand2(w, w), nand2(w, w));
  *r = fv(3);
}

// wolfram = NAND commutativity from the Sheffer-stroke axiom:
//   nand(x, y) == nand(y, x)   -- Waldmeister's wolfram.pr conjecture.
// The hard sibling of `thm`: `thm` (double negation) is the distance-54
// goal; deriving full commutativity from the single axiom is the
// deeper Wolfram-axiom benchmark.
static void goal_wolfram(Term *l, Term *r) {
  Term x = fv(0), y = fv(1);
  *l = nand2(x, y);
  *r = nand2(y, x);
}

// cpl1 (CriticalPairLemma 1, distance 1): a critical pair of the
// axiom with itself.
//   nand(a,nand(nand(a,b),a))
//     == nand(b,nand(nand(a,c),
//               nand(nand(nand(a,c),nand(a,nand(nand(a,b),a))),
//                    nand(a,c))))
static void goal_cpl1(Term *l, Term *r) {
  Term a = fv(0), b = fv(1), c = fv(2);
  Term W  = nand2(a, nand2(nand2(a, b), a));   // == lhs
  Term ac = nand2(a, c);
  Term Z  = nand2(ac, W);
  Term Y  = nand2(Z, nand2(a, c));
  Term X  = nand2(nand2(a, c), Y);
  *l = nand2(a, nand2(nand2(a, b), a));
  *r = nand2(b, X);
}

// cpl2 (CriticalPairLemma 2, distance 1):
//   a == nand(nand(b,a),
//             nand(P, nand(nand(P,a), P)))   where P = nand(nand(c,e2),b)
static void goal_cpl2(Term *l, Term *r) {
  Term a = fv(0), b = fv(1), c = fv(2), e2 = fv(3);
  Term P = nand2(nand2(c, e2), b);
  *l = fv(0);
  *r = nand2(nand2(b, a),
             nand2(P, nand2(nand2(P, a), P)));
}

// subl2 (SubstitutionLemma 2, ~4 inferences deep):
//   a == nand(nand(nand(b,Q),a), nand(c,nand(nand(c,a),c)))
//        where Q = nand(nand(b,c),b)
static void goal_subl2(Term *l, Term *r) {
  Term a = fv(0), b = fv(1), c = fv(2);
  Term Q = nand2(nand2(b, c), b);
  *l = fv(0);
  *r = nand2(nand2(nand2(b, Q), a),
             nand2(c, nand2(nand2(c, a), c)));
}

// Multi-step goals -- the missing rungs between the distance-1 lemmas
// (cpl1/cpl2/subl2) and the distance-54 thm.  wrapk stacks `k` axiom
// instances: each axiom_inst(a,b,X) rewrites to X in one step, so the
// stack reduces to M in exactly k steps (reduce the outermost each
// time).  These join via the AXIOM ALONE -- no completion needed --
// exercising the MNF parent-chain machinery on chains longer than one.
// The axiom LHS holds its `c` argument twice, so |wrapk| ~ 2^k --
// ~885 symbols/side at k=6, still joined by the depth-first deque.
static Term wrapk(Term a, Term b, Term m, u32 k) {
  Term t = m;
  for (u32 i = 0; i < k; i++) t = axiom_inst(a, b, t);
  return t;
}

// Two-sided meet: both fronts reduce to M = nand(fv7,fv8) along
// DISJOINT paths (distinct wrap variables), so the join is a genuine
// middle-meet after ~k steps on each side.
static void goal_chain(Term *l, Term *r, u32 k) {
  Term m = nand2(fv(7), fv(8));
  *l = wrapk(fv(0), fv(1), m, k);
  *r = wrapk(fv(4), fv(5), m, k);
}

// One-sided deep chain: L reduces to M in k steps, R = M -- a single
// long GREEN parent chain, the RED seed static at M.
static void goal_deep(Term *l, Term *r, u32 k) {
  Term m = nand2(fv(7), fv(8));
  *l = wrapk(fv(0), fv(1), m, k);
  *r = m;
}

// andassoc = AndAssociativity over the single Sheffer/nand axiom --
// the Waldmeister landmark (WM: KBO+MixWeight, 1601 rules, 14.3s).
// And is defined via nand:  And(x,y) = nand(nand(x,y), nand(x,y)).
// The conjecture And(p, And(q,r)) == And(And(p,q), r) with p,q,r
// distinct GROUND constants, fully expanded in nand.  This is the
// exact form thvm's paclet uses (verified against WM's andassoc.pr).
static Term and2(Term x, Term y) {
  Term inner = nand2(x, y);
  return nand2(inner, inner);
}
static void goal_andassoc(Term *l, Term *r) {
  Term p = konst(L_P), q = konst(L_Q), rr = konst(L_R);
  // LHS = And(p, And(q,r))
  *l = and2(p, and2(q, rr));
  // RHS = And(And(p,q), r)
  *r = and2(and2(p, q), rr);
}
// andassocu = AndAssociativity with p,q,r as UNIVERSAL variables
// (TAG_FVR) rather than ground skolem constants -- the form the paclet's
// AxiomaticTheory["WolframAxioms","NotableTheorems"]["AndAssociativity"]
// conjecture actually carries (ForAll[{p,q,r}, ...]).  The ground form
// (goal_andassoc) is a rigid instance; this is the schema, so it
// exercises the same trajectory the user-facing TFindEquationalProof
// drives -- the matched-config comparison point for the paclet.
static void goal_andassocu(Term *l, Term *r) {
  Term p = fv(0), q = fv(1), rr = fv(2);
  *l = and2(p, and2(q, rr));
  *r = and2(and2(p, q), rr);
}

// McCune's single-axiom group/Sheffer-style equation over {and:2, not:1}:
//   and(X0, not(and(X1, and(and(and(X2, not(X2)), not(and(X3, X1))), X0))))
//   = X3
// (Vampire TPTP form: tools/baselines/vampire_raw/
//  McCuneAxioms__EqualityOfInverses.out f1.)
static Term mccune_axiom_lhs(void) {
  Term x0 = fv(0), x1 = fv(1), x2 = fv(2), x3 = fv(3);
  Term inner = and_op(and_op(x2, not_op(x2)),
                      not_op(and_op(x3, x1)));
  Term mid   = and_op(inner, x0);
  return and_op(x0, not_op(and_op(x1, mid)));
}

// EqualityOfInverses conjecture (positive form): for the Skolem constant
// p, and(p, not(p)) == and(not(p), p) -- the goal Vampire negates to a
// disequality on sk_c1.  Uses L_P as the single ground constant.
static void goal_mccune(Term *l, Term *r) {
  Term p = konst(L_P);
  *l = and_op(p, not_op(p));
  *r = and_op(not_op(p), p);
}

// Robbins basis for Boolean algebra (three axioms over {or:2, not:1}):
//   1. or(X1, or(X2, X3)) = or(or(X1, X2), X3)      -- associativity
//   2. or(X1, X2)         = or(X2, X1)              -- commutativity
//   3. not(or(not(or(X1, X2)), not(or(X1, not(X2))))) = X1   -- Robbins
// Reuses the McCune-side L_AND label as `or` (arity 2) and L_NOT as
// `not` (arity 1): KBO/LPO only care about arity here, so the integer
// tags are reusable -- the bench's output prints them as `C5`/`C6`.
// Conjecture (DoubleNegation): not(not(sk_c1)) = sk_c1.  Uses L_P.
static void robbins_axioms(Term *l1, Term *r1,
                           Term *l2, Term *r2,
                           Term *l3, Term *r3) {
  Term x1 = fv(0), x2 = fv(1), x3 = fv(2);
  *l1 = and_op(x1, and_op(x2, x3));
  *r1 = and_op(and_op(x1, x2), x3);
  *l2 = and_op(x1, x2);
  *r2 = and_op(x2, x1);
  Term inner_a = not_op(and_op(x1, x2));
  Term inner_b = not_op(and_op(x1, not_op(x2)));
  *l3 = not_op(and_op(inner_a, inner_b));
  *r3 = x1;
}
static void goal_robbins(Term *l, Term *r) {
  Term p = konst(L_P);
  *l = not_op(not_op(p));
  *r = p;
}

int main(int argc, char **argv) {
  thvm_init();

  const char *goal = (argc > 1) ? argv[1] : "thm";
  u32    step_cap  = (argc > 2) ? (u32)strtoul(argv[2], NULL, 10) : 200000u;
  double wall_cap  = (argc > 3) ? strtod(argv[3], NULL)          : 120.0;

  // Reduction ordering.  Default KBO (one binary symbol `nand`,
  // weight 1, var weight 1 -- KBO weight is plain symbol count, so a
  // deeply-nested lhs outweighs a bare-variable rhs).  ATP_BENCH_ORD
  // selects an alternative for the ordering experiment:
  //   "kbo0" -- KBO with var_weight 0 (variables weigh nothing)
  //   "lpo"  -- lexicographic path ordering instead of KBO
  const char *ord_env = getenv("ATP_BENCH_ORD");
  int use_lpo  = (ord_env != NULL && strcmp(ord_env, "lpo")  == 0);
  int use_kbo0 = (ord_env != NULL && strcmp(ord_env, "kbo0") == 0);
  // Labels: 0 unused, 1=nand, 2=p, 3=q, 4=r (the andassoc constants).
  // Each gets weight 1.  Precedence: nand highest, constants below it
  // (p>q>r) -- matches Waldmeister's AutoPrecedence on the function
  // symbol over skolem constants.  Goals that use only nand (thm,
  // wolfram, ...) are unaffected: their terms never touch labels 2-4.
  // n_labels covers Sheffer (L_NAND=1, L_P/Q/R=2..4) AND McCune (L_AND=5,
  // L_NOT=6).  Sheffer goals never touch labels 5-6 and McCune never
  // touches labels 1/3/4, so the per-label weight/precedence entries for
  // the unused symbols are inert on the respective paths.  Precedence
  // ranks `and` (arity 2) above `not` (arity 1) on the McCune side, in
  // line with the Fuchs arity ladder atp_auto_precedence would derive.
  static u32 weights[7]    = { 0u, 1u, 1u, 1u, 1u, 1u, 1u };
  static u32 precedence[7] = { 0u, 4u, 3u, 2u, 1u, 6u, 5u };
  KboConfig cfg = {
    .weights    = weights,
    .precedence = precedence,
    .n_labels   = 7u,
    .var_weight = use_kbo0 ? 0u : 1u,
  };
  static u32 lpo_prec[7] = { 0u, 4u, 3u, 2u, 1u, 6u, 5u };
  static u32 lpo_prec_wm[7] = { 0u, 1u, 4u, 3u, 2u, 6u, 5u };
  const char *lpo_wm_env = getenv("ATP_BENCH_LPO_SKOLEMS_HIGH");
  static LpoConfig lpo = { .precedence = lpo_prec, .n_labels = 7u };
  if (lpo_wm_env != NULL && lpo_wm_env[0] == '1') lpo.precedence = lpo_prec_wm;

  // cpgen mode: generate the critical pairs of the axiom with
  // itself -- the distance-1 lemmas -- in ONE CP-generation call,
  // no completion loop.  This is the direct "does inference work
  // one step from the axiom" test: CriticalPairLemma 1/2 of WL's
  // proof should appear among the output.
  if (strcmp(goal, "cpgen") == 0) {
    Term ax_l = axiom_lhs();
    Term ax_r = fv(2);
    static CriticalPair cps[8192];
    u32 n = thvm_critical_pairs(&ax_l, &ax_r, 1u, cps, 8192u);
    printf("=== axiom critical pairs (1-step, axiom x axiom) ===\n");
    printf("generated %u critical pairs\n", n);
    u32 show = (n < 60u) ? n : 60u;
    for (u32 k = 0; k < show; k++) {
      char lb[768], rb[768];
      atp_pretty_term(cps[k].lhs, lb, sizeof lb);
      atp_pretty_term(cps[k].rhs, rb, sizeof rb);
      printf("  CP%-3u %s = %s\n", k, lb, rb);
    }
    return (n > 0u) ? 0 : 1;
  }

  // Opt-in in-loop GC.  ATP_BENCH_GC=<half-space-cells> enables the
  // Cheney collector so a long completion run floats around its live
  // working set instead of climbing into from-space exhaustion.  Off
  // by default (flat 128M heap) so the cost numbers stay comparable
  // to earlier milestones; set it to exercise thvm_atp_gc_collect --
  // and, under -DATP_CP_GRAPH, the 8b cp_graph GC root.
  {
    const char *gc_env = getenv("ATP_BENCH_GC");
    if (gc_env != NULL && gc_env[0] != '\0') {
      u64 half = strtoull(gc_env, NULL, 10);
      if (half > 0) gc_init(half);
    }
  }

  AtpState *s = thvm_atp_init(&cfg, step_cap);
  if (use_lpo) thvm_atp_set_lpo(s, &lpo);
  // CP-weight mode: when `THVM_ATP_CP_WEIGHT` (an `AtpCpWeightMode`
  // integer) is exported, override the mode for the experiment;
  // otherwise leave the engine default (ATP_CP_WEIGHT_GT) in place.
  {
    const char *cw = getenv("THVM_ATP_CP_WEIGHT");
    if (cw != NULL && *cw != 0) {
      thvm_atp_set_cp_weight_mode(s, (u32)strtoul(cw, NULL, 10));
    }
  }
  // THVM_ATP_RIGHT_REDUCE=0 disables interreduction right-reduction
  // (RHS composition) for A/B measurement; default (unset/1) keeps the
  // DISCOUNT-loop right-reduction on.
  {
    const char *rr = getenv("THVM_ATP_RIGHT_REDUCE");
    if (rr != NULL && *rr == '0') thvm_atp_set_right_reduce(s, 0u);
  }
  // THVM_ATP_CP_SET_IR=1 enables periodic full-rule-set CP-queue
  // interreduction (a port of KPV_KPMengeInterreduzieren) -- deletes
  // queued CPs that became joinable through any rule, keeping the queue
  // (the dominant memory consumer at depth) small.
  {
    const char *ci = getenv("THVM_ATP_CP_SET_IR");
    if (ci != NULL && ci[0] != '\0' && ci[0] != '0') {
      thvm_atp_set_cp_set_interreduce(s, 1u);
    }
  }
  // THVM_ATP_WALDMEISTER=1 replicates the WL Method->"Waldmeister"
  // preset's runtime knobs (SelectionRatio 51, RHSInterreduce,
  // UnfailingCP) so a profiling run follows the same trajectory the
  // proof harness does.  AutoPrecedence on the single nand symbol is
  // the identity precedence already in `cfg`, so it needs no setter.
  {
    const char *wm = getenv("THVM_ATP_WALDMEISTER");
    if (wm != NULL && wm[0] != '\0' && wm[0] != '0') {
      // selection_ratio=141 (one FIFO pick per 141 weight picks) tuned
      // further from iter-164's 101 -- iter 165 measurement.  Wins
      // across all four bench goals: AndAssoc +10%, wolfram +14%,
      // mccune +10%, robbins +12%.  Just outside WM's enumerated set
      // {10, 50, 100, 200} but the YFiles problem-analysis range is
      // tunable and 141 is the empirical sweet spot for this engine.
      thvm_atp_set_selection_ratio(s, 141u);
      thvm_atp_set_use_rhs_interreduce(s, 1u);
      thvm_atp_set_use_unfailing_cp(s, 1u);
      thvm_atp_set_use_orphan_murder(s, 1u);
      thvm_atp_set_use_unorient_index(s, 1u);
      thvm_atp_set_use_lazy_normalize(s, 1u);
      // WM's KPV_KPMengeInterreduzieren -- periodic CP-queue
      // interreduction against the rule set.  +6% on AndAssoc post-fix
      // (iter 160); part of standard Waldmeister.
      thvm_atp_set_cp_set_interreduce(s, 1u);
      // Backward subsumption (WM standard): scan existing rules and
      // soft-delete any subsumed by the newly-added rule.  +30% on
      // AndAssoc post-fix (iter 162) -- the kill churn pays off because
      // fewer surviving rules => smaller DT => cheaper unorient queries.
      // (Forward subsumption alone regresses; backward subsumption +
      // backward demodulation together give +31% on AndAssoc.)
      thvm_atp_set_use_bwd_subsume(s, 1u);
      thvm_atp_set_use_bwd_demod(s, 1u);
      // MNF goal-directed front search.  +1.6% on AndAssoc (iter 163);
      // gated against lazy_normalize at the push site (use_lazy_normalize
      // && !use_mnf) so the combination stays sound.
      thvm_atp_set_use_mnf(s, 1u);
      // Auto-MaxWeight base=30 (iter 164): grows with rule depth (slope=2),
      // bounds CP weight so heavy junk gets stashed.  +9.5% on AndAssoc,
      // +13% on wolfram, neutral on mccune/robbins.  Default-on as part
      // of preset.
      thvm_atp_set_auto_max_cp_weight(s, 30u);
    }
  }
  // THVM_ATP_RHS_IR=0/1: independent override of the secondary RHS-
  // interreduce sweep (already-redundant with right_reduce inline path).
  {
    const char *ri = getenv("THVM_ATP_RHS_IR");
    if (ri != NULL && ri[0] != '\0')
      thvm_atp_set_use_rhs_interreduce(s, (ri[0] != '0') ? 1u : 0u);
  }

  // THVM_ATP_LAZY_NORM=0/1: toggle deferred-selection / lazy normalization
  // independently of the WM preset (A/B vs eager push-time normalize).
  {
    const char *ln = getenv("THVM_ATP_LAZY_NORM");
    if (ln != NULL && ln[0] != '\0')
      thvm_atp_set_use_lazy_normalize(s, (ln[0] != '0') ? 1u : 0u);
  }

  // THVM_ATP_UNORIDX=0/1: toggle the indexed unorientable-rewrite pass
  // independently of the WM preset (A/B vs the linear KBO-gated scan).
  {
    const char *ui = getenv("THVM_ATP_UNORIDX");
    if (ui != NULL && ui[0] != '\0')
      thvm_atp_set_use_unorient_index(s, (ui[0] != '0') ? 1u : 0u);
  }


  // THVM_ATP_ORPHAN=1 toggles lazy orphan murder independently of the
  // WM preset, so a run can A/B its effect on the CP queue.
  {
    const char *om = getenv("THVM_ATP_ORPHAN");
    if (om != NULL && om[0] != '\0')
      thvm_atp_set_use_orphan_murder(s, (om[0] != '0') ? 1u : 0u);
  }

  // Independent A/B toggles for WM-standard subsumption / demodulation.
  // BWD_SUB and BWD_DEMOD are now in the WALDMEISTER preset (iter 162)
  // but kept here so they can be turned OFF for differential debugging;
  // FWD_SUB is OFF by default because it regresses AndAssoc.
  {
    const char *fs = getenv("THVM_ATP_FWD_SUB");
    if (fs != NULL && fs[0] != '\0')
      thvm_atp_set_use_fwd_subsume(s, (fs[0] != '0') ? 1u : 0u);
  }
  {
    const char *bs = getenv("THVM_ATP_BWD_SUB");
    if (bs != NULL && bs[0] != '\0')
      thvm_atp_set_use_bwd_subsume(s, (bs[0] != '0') ? 1u : 0u);
  }
  {
    const char *bd = getenv("THVM_ATP_BWD_DEMOD");
    if (bd != NULL && bd[0] != '\0')
      thvm_atp_set_use_bwd_demod(s, (bd[0] != '0') ? 1u : 0u);
  }
  // SOS (Set-of-Support) -- a CP-scoring bonus for CPs touching goal
  // symbols.  Not WM standard (Vampire/E heuristic); kept as opt-in.
  {
    const char *ss = getenv("THVM_ATP_SOS");
    if (ss != NULL && ss[0] != '\0')
      thvm_atp_set_use_sos(s, (ss[0] != '0') ? 1u : 0u);
  }
  // THVM_ATP_UNFAILING=0/1: independent override of unfailing CP
  // generation (WM standard; in preset by default).
  {
    const char *uf = getenv("THVM_ATP_UNFAILING");
    if (uf != NULL && uf[0] != '\0')
      thvm_atp_set_use_unfailing_cp(s, (uf[0] != '0') ? 1u : 0u);
  }
  // THVM_ATP_CP_FIFO=1: enable FIFO tiebreaker for CPs of equal weight
  // (sequence-order disambiguation instead of arbitrary heap order).
  {
    const char *cf = getenv("THVM_ATP_CP_FIFO");
    if (cf != NULL && cf[0] != '\0')
      thvm_atp_set_cp_fifo_tiebreak(s, (cf[0] != '0') ? 1u : 0u);
  }
  // THVM_ATP_SEL_RATIO=<N>: override the WM preset's selection_ratio
  // (WM uses 51 = 1 FIFO pick per 51 weight picks).
  {
    const char *sr = getenv("THVM_ATP_SEL_RATIO");
    if (sr != NULL && sr[0] != '\0') {
      u32 r = (u32)atoi(sr);
      if (r > 0) thvm_atp_set_selection_ratio(s, r);
    }
  }
  // THVM_ATP_GOAL_INTERLEAVE=<N>: enable goal-direction interleaving --
  // every N-th CP selection picks the CP with the smallest goal-matching
  // weight instead of the smallest plain weight.  0 = off.
  {
    const char *gi = getenv("THVM_ATP_GOAL_INTERLEAVE");
    if (gi != NULL && gi[0] != '\0') {
      u32 r = (u32)atoi(gi);
      thvm_atp_set_goal_interleave(s, r);
    }
  }

  // THVM_ATP_GROUND_JOIN=1 / THVM_ATP_CONNECT=1: sound CP-volume
  // reducers (Waldmeister Grundzusammenfuehrbar / Bachmair-Dershowitz
  // connectedness).  Compiled in by default (Makefile ATP_CP_GROUND_JOIN
  // ?= 1); these flip the runtime gate so a CP that is ground-joinable /
  // connected-below-peak is dropped at generate time, shrinking the live
  // queue.
  {
    const char *gj = getenv("THVM_ATP_GROUND_JOIN");
    if (gj != NULL && gj[0] != '\0')
      thvm_atp_set_use_ground_join(s, (gj[0] != '0') ? 1u : 0u);
    const char *cn = getenv("THVM_ATP_CONNECT");
    if (cn != NULL && cn[0] != '\0')
      thvm_atp_set_use_connectedness(s, (cn[0] != '0') ? 1u : 0u);
  }

  // THVM_ATP_RECORD_NORM=1 turns on per-step normalize-trace recording
  // (the WL ProofObject path's setting) so a C bench can A/B the cost of
  // record_norm_steps -- the suspected paclet-vs-C overhead.
  {
    const char *rn = getenv("THVM_ATP_RECORD_NORM");
    if (rn != NULL && rn[0] == '1') thvm_atp_set_record_norm_steps(s, 1u);
  }

  // THVM_ATP_LRS=1 turns on the Vampire Limited Resource Strategy.  Also
  // wires the user-loop `wall_cap` into thvm_atp_set_wall_deadline so the
  // LRS horizon has a budget to plan against -- the engine's saturation
  // is still bounded by the same wall_cap, so the timing is comparable
  // to LRS=0.
  {
    const char *lrs = getenv("THVM_ATP_LRS");
    if (lrs != NULL && lrs[0] != '\0' && lrs[0] != '0') {
      thvm_atp_set_use_lrs(s, 1u);
      if (wall_cap > 0.0) thvm_atp_set_wall_deadline(s, wall_cap);
    }
  }
  // THVM_ATP_RANDOM_RATIO=<n> -- every n-th CP selection picks a
  // uniformly-random queued CP via xorshift64.  Default off.
  // THVM_ATP_RANDOM_SEED=<u64> seeds the stream (default: a fixed
  // nonzero so unseeded runs are deterministic).
  {
    const char *rratio = getenv("THVM_ATP_RANDOM_RATIO");
    const char *rseed  = getenv("THVM_ATP_RANDOM_SEED");
    if (rseed != NULL && rseed[0] != '\0') {
      thvm_atp_set_random_seed(s, strtoull(rseed, NULL, 10));
    }
    if (rratio != NULL && rratio[0] != '\0' && rratio[0] != '0') {
      thvm_atp_set_random_modulo(s, (u32)strtoul(rratio, NULL, 10));
    }
  }

  if (strcmp(goal, "mccune") == 0) {
    thvm_atp_add_equation(s, mccune_axiom_lhs(), fv(3));
  } else if (strcmp(goal, "robbins") == 0) {
    Term r_l1, r_r1, r_l2, r_r2, r_l3, r_r3;
    robbins_axioms(&r_l1, &r_r1, &r_l2, &r_r2, &r_l3, &r_r3);
    thvm_atp_add_equation(s, r_l1, r_r1);
    thvm_atp_add_equation(s, r_l2, r_r2);
    thvm_atp_add_equation(s, r_l3, r_r3);
  } else {
    thvm_atp_add_equation(s, axiom_lhs(), fv(2));
  }

  // "sat" mode: pure completion, NO goal -- run to the step/wall cap
  // and watch the CP queue grow.  This is the explosion probe (a deep
  // goal that never closes would do the same, but `sat` is unambiguous
  // about what is being measured).
  int saturate = (strcmp(goal, "sat") == 0);

  Term gl = 0, gr = 0;
  if      (strcmp(goal, "cpl1")   == 0) goal_cpl1(&gl, &gr);
  else if (strcmp(goal, "cpl2")   == 0) goal_cpl2(&gl, &gr);
  else if (strcmp(goal, "subl2")  == 0) goal_subl2(&gl, &gr);
  else if (strcmp(goal, "chain3") == 0) goal_chain(&gl, &gr, 3u);
  else if (strcmp(goal, "chain4") == 0) goal_chain(&gl, &gr, 4u);
  else if (strcmp(goal, "chain6") == 0) goal_chain(&gl, &gr, 6u);
  else if (strcmp(goal, "deep5")  == 0) goal_deep(&gl, &gr, 5u);
  else if (strcmp(goal, "wolfram")== 0) goal_wolfram(&gl, &gr);
  else if (strcmp(goal, "andassoc")==0) goal_andassoc(&gl, &gr);
  else if (strcmp(goal, "andassocu")==0) goal_andassocu(&gl, &gr);
  else if (strcmp(goal, "mccune")  == 0) goal_mccune(&gl, &gr);
  else if (strcmp(goal, "robbins") == 0) goal_robbins(&gl, &gr);
  else if (!saturate)                   goal_thm(&gl, &gr);
  if (!saturate) thvm_atp_set_goal(s, gl, gr);

  // THVM_ATP_MNF=1 activates the MNF goal-directed (two-coloured front)
  // search -- the only path that closes a symmetric goal like wolfram
  // commutativity.  Requires the binary be built with `make ATP_MNF=1`
  // (otherwise the setter is a no-op).
  {
    const char *mnf = getenv("THVM_ATP_MNF");
    if (mnf != NULL && *mnf == '1') thvm_atp_set_use_mnf(s, 1u);
  }

  // Optional automatic growing CP-weight bound (Waldmeister MaxWeight).
  // THVM_ATP_AUTO_MAXW=<base> enables it; the engine grows the bound as
  // the rule set deepens so it never permanently discards a CP the
  // proof needs (completeness preserved).  0 / unset = unbounded (the
  // historical default).
  {
    const char *amw = getenv("THVM_ATP_AUTO_MAXW");
    if (amw != NULL && *amw != 0) {
      thvm_atp_set_auto_max_cp_weight(s, (u32)strtoul(amw, NULL, 10));
    }
  }

  printf("=== Wolfram-axiom completion : goal=%s ===\n", goal);
  printf("ordering=%s  step_cap=%u  wall_cap=%.0fs  cp_weight_mode=%u\n",
         use_lpo ? "lpo" : (use_kbo0 ? "kbo0" : "kbo"),
         step_cap, wall_cap, (u32)s->cp_weight_mode);
  if (s->random_modulo > 0u) {
    printf("random_ratio=%u  random_seed=%llu\n",
           s->random_modulo, (unsigned long long)s->rng_state);
  }
  if (!saturate) {
    char glb[2048], grb[2048];
    atp_pretty_term(gl, glb, sizeof glb);
    atp_pretty_term(gr, grb, sizeof grb);
    printf("goal-lhs = %s\n", glb);
    printf("goal-rhs = %s\n", grb);
  }

  // THVM_ATP_DIAG=<period>: every <period> steps, measure the
  // interreduction deficit (rules with a side reducible by another
  // rule) and the max/total symbol footprint of R.  Confirms whether R
  // is genuinely under-interreduced as it grows.
  u32 diag_period = 0;
  {
    const char *dg = getenv("THVM_ATP_DIAG");
    if (dg != NULL && dg[0] != '\0') diag_period = (u32)strtoul(dg, NULL, 10);
  }

  // Phase timing: THVM_ATP_PROFILE=1 wires up the per-phase wall-clock
  // counters inside thvm_atp_step (pop-normalize, CP-gen, push-normalize,
  // interreduce, goal-check, normalize-graph) so the headline output
  // reports the dominant cost slice.
  {
    const char *pr = getenv("THVM_ATP_PROFILE");
    if (pr != NULL && pr[0] == '1') g_atp_phase_enabled = 1u;
  }

  clock_t   t0  = clock();
  AtpStatus st  = ATP_RUNNING;
  u32       i   = 0;
  u32       max_cps = 0;
  for (; i < step_cap; i++) {
    st = thvm_atp_step(s);
    if (s->n_cps > max_cps) max_cps = s->n_cps;
    if (st != ATP_RUNNING) break;
    double el = (double)(clock() - t0) / CLOCKS_PER_SEC;
    if (i > 0 && i % 250u == 0u) {
      printf("  step %7u  rules=%-6u cps=%-8u %.1fs\n",
             i, s->n_rules, s->n_cps, el);
      fflush(stdout);
    }
    if (diag_period > 0 && i > 0 && i % diag_period == 0u) {
      u32 nl = 0, nr = 0, na = 0, maxside = 0; u64 tot = 0;
      thvm_atp_interreduce_deficit(s, &nl, &nr, &na, &maxside, &tot);
      printf("  DIAG step %7u  rules=%u  deficit: lhs-red=%u rhs-red=%u "
             "any-red=%u (%.1f%%)  max-side=%u  total-syms=%llu\n",
             i, s->n_rules, nl, nr, na,
             (s->n_rules > 0) ? (100.0 * (double)na / (double)s->n_rules) : 0.0,
             maxside, (unsigned long long)tot);
      fflush(stdout);
    }
    if (el > wall_cap) {
      printf("  [wall cap %.0fs reached]\n", wall_cap);
      break;
    }
  }
  double el = (double)(clock() - t0) / CLOCKS_PER_SEC;

  const char *sn =
      (st == ATP_PROVED)      ? "PROVED" :
      (st == ATP_REFUTED)     ? "REFUTED" :
      (st == ATP_QUEUE_EMPTY) ? "QUEUE_EMPTY (saturated, no proof)" :
      (st == ATP_TIMEOUT)     ? "TIMEOUT (step cap)" :
                                "RUNNING (wall cap)";
  printf("=> %s\n", sn);
  printf("   goal=%s  steps=%u  rules=%u  cps=%u  max_cps=%u  %.1fs\n",
         goal, i, s->n_rules, s->n_cps, max_cps, el);
  printf("   trace: n_trace=%u  t_max=%u  record_norm=%u\n",
         s->n_trace, s->t_max, s->record_norm_steps);
  printf("   dropped: joinable=%u queue-subsumed=%u "
         "rule-subsumed=%u connected=%u orphan=%u lrs=%u\n",
         s->n_cps_dropped_joinable, s->n_cps_dropped_queue_subsumed,
         s->n_cps_dropped_rule_subsumed, s->n_cps_dropped_connected,
         s->n_cps_dropped_orphan, s->n_cps_dropped_lrs);
  if (s->use_lrs) {
    printf("   lrs: recomputes=%u  horizon=%u  warmup=%u  period=%u\n",
           s->n_lrs_recomputes, s->lrs_horizon,
           s->lrs_warmup_selections, s->lrs_recompute_period);
  }
  printf("   dropped: ground-joinable=%u connected-below-peak=%u\n",
         s->n_cps_ground_joinable, s->n_cps_dropped_connected_below_peak);
  printf("   right-reduced (RHS composed) rules: %u\n", s->n_right_reduced);
  printf("   lazy-normalize=%u  push-time full-R normalizes=%llu\n",
         s->use_lazy_normalize,
         (unsigned long long)s->n_cps_push_normalized);
  if (g_atp_phase_enabled) {
    double w = (el > 0.0) ? el : 1.0;
    u64 sumus = g_atp_phase_us_pop_normalize + g_atp_phase_us_cp_gen +
                g_atp_phase_us_push_normalize + g_atp_phase_us_interreduce +
                g_atp_phase_us_goal_check + g_atp_phase_us_norm_graph;
    printf("   phase: pop-norm=%.2fs (%.0f%%) cp-gen=%.2fs (%.0f%%)\n"
           "          push-norm=%.2fs (%.0f%%) interreduce=%.2fs (%.0f%%)\n"
           "          goal-check=%.2fs (%.0f%%) norm-graph=%.2fs (%.0f%%)\n"
           "          sum=%.2fs / wall=%.2fs (%.0f%%)\n",
           g_atp_phase_us_pop_normalize / 1e6,
           100.0 * (g_atp_phase_us_pop_normalize / 1e6) / w,
           g_atp_phase_us_cp_gen / 1e6,
           100.0 * (g_atp_phase_us_cp_gen / 1e6) / w,
           g_atp_phase_us_push_normalize / 1e6,
           100.0 * (g_atp_phase_us_push_normalize / 1e6) / w,
           g_atp_phase_us_interreduce / 1e6,
           100.0 * (g_atp_phase_us_interreduce / 1e6) / w,
           g_atp_phase_us_goal_check / 1e6,
           100.0 * (g_atp_phase_us_goal_check / 1e6) / w,
           g_atp_phase_us_norm_graph / 1e6,
           100.0 * (g_atp_phase_us_norm_graph / 1e6) / w,
           sumus / 1e6, w,
           100.0 * (sumus / 1e6) / w);
    if (g_atp_unorient_step_calls > 0) {
      printf("   unorient-step: %llu calls  %llu fires  %llu empty (%.0f%% wasted)  %.2fs total\n",
             (unsigned long long)g_atp_unorient_step_calls,
             (unsigned long long)g_atp_unorient_step_fires,
             (unsigned long long)g_atp_unorient_step_empty,
             100.0 * (double)g_atp_unorient_step_empty /
                     (double)g_atp_unorient_step_calls,
             g_atp_unorient_step_us / 1e6);
    }
    {
      u64 unf_total = g_atp_unf_memo_hits + g_atp_unf_memo_misses;
      if (unf_total > 0) {
        printf("   unf-memo: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
               (unsigned long long)g_atp_unf_memo_hits,
               (unsigned long long)unf_total,
               100.0 * (double)g_atp_unf_memo_hits / (double)unf_total);
      }
    }
  }
  { u32 unor = 0;
    for (u32 i = 0; i < s->n_rules; i++) if (!s->r_orient[i]) unor++;
    printf("   unorientable rules: %u / %u\n", unor, s->n_rules); }
  printf("   steps/sec=%.1f  cps/step=%.1f\n",
         (el > 0.0) ? (double)i / el : 0.0,
         (i > 0) ? (double)max_cps / (double)i : 0.0);
  { u32 mn = 0, mx = 0, bins[12];
    double mean = 0.0;
    u32 qlen = thvm_atp_cp_size_stats(s, &mn, &mx, &mean, bins, 12u, 10u);
    printf("   cp-size: live=%u  min=%u  mean=%.1f  max=%u\n",
           qlen, mn, mean, mx);
    printf("   cp-size histogram (bucket=10 symbols):\n     ");
    for (u32 b = 0; b < 12u; b++) printf("[%u-%u)=%u ", b*10u, (b+1u)*10u, bins[b]);
    printf("\n"); }

#ifdef ATP_NORM_STATS
  // 8b: optimal-sharing ratio of the cross-CP normalization memo.
  // A hit = a subterm shared with an already-visited CP whose normal
  // form was reused; misses = distinct subterm cells actually
  // normalized.  hits/(hits+misses) is the work the sharing saved.
  {
    u64 hits = 0, misses = 0;
    double secs = 0.0;
    thvm_atp_norm_stats(&hits, &misses, &secs);
    u64 total = hits + misses;
    double ratio = (total > 0) ? (100.0 * (double)hits / (double)total) : 0.0;
    double frac  = (el > 0.0) ? (100.0 * secs / el) : 0.0;
    printf("   norm-memo: %llu hits / %llu distinct-cells  (%.1f%% shared)\n",
           (unsigned long long)hits, (unsigned long long)misses, ratio);
    printf("   norm-sweep: %.2fs  (%.1f%% of %.1fs total)\n",
           secs, frac, el);
  }
#endif

#ifdef ATP_MATCH_STATS
  // 8e: shared-traversal match stats.  memo_hits / (hits + misses) is
  // the per-subterm sharing ratio -- how often a (pattern_cell,
  // subject_cell) pair was served from the memo rather than walked.
  // node_visits / calls is the average traversal size.
  {
    u64 calls = 0, nodes = 0, hits = 0, miss = 0;
    double secs = 0.0;
    thvm_atp_match_stats(&calls, &nodes, &hits, &miss, &secs);
    u64 mtot = hits + miss;
    double mratio = (mtot > 0) ? (100.0 * (double)hits / (double)mtot) : 0.0;
    double frac   = (el > 0.0) ? (100.0 * secs / el) : 0.0;
    printf("   match-multi: %llu calls  %llu node-visits\n",
           (unsigned long long)calls, (unsigned long long)nodes);
    printf("   match-memo: %llu hits / %llu miss  (%.1f%% shared)\n",
           (unsigned long long)hits, (unsigned long long)miss, mratio);
    printf("   match-sweep: %.2fs  (%.1f%% of %.1fs total)\n",
           secs, frac, el);
  }
#endif

#ifdef ATP_FV_INDEX
  // 7d: subsumption-index retrieval stats.  candidates / query is the
  // average CPs the discrimination tree handed to thvm_match (the
  // array scan handed it n_cps); matchcalls is the thvm_match volume
  // the index issued -- the milestone-7 scan would have issued
  // ~4 * n_cps per query.  node_visits / query is the average tree
  // nodes the descent touched.
  {
    u64 calls = 0, nodevisits = 0, cands = 0, matchcalls = 0;
    u32 nodes = 0;
    thvm_atp_fv_stats(s, &calls, &nodevisits, &cands, &matchcalls, &nodes);
    double avg_cand = (calls > 0) ? ((double)cands / (double)calls) : 0.0;
    double avg_node = (calls > 0) ? ((double)nodevisits / (double)calls) : 0.0;
    printf("   fv-index: %llu queries  %u tree-nodes\n",
           (unsigned long long)calls, nodes);
    printf("   fv-retrieval: %.1f candidates/query  %.1f tree-nodes/query\n",
           avg_cand, avg_node);
    printf("   fv-match: %llu thvm_match calls on survivors\n",
           (unsigned long long)matchcalls);
    if (s->fv_index != NULL && s->fv_index->q_depth_capped > 0) {
      printf("   fv-descent: %llu depth-cap bails (latent DT-cycle)\n",
             (unsigned long long)s->fv_index->q_depth_capped);
    }
  }
#endif

#ifdef ATP_RULE_INDEX
  // 7e: rule-LHS redex-index retrieval stats -- THE Sheffer-pruning
  // measurement.  candidates/query is the candidates-returned-per-query:
  // a linear scan would touch n_rules records per position-query.  If
  // candidates/query tracks n_rules the discrimination tree does NOT
  // prune on single-symbol Sheffer; if it stays bounded it does.
  {
    u64 queries = 0, cands = 0, matchcalls = 0, nodevisits = 0;
    u32 nodes = 0, rbuilt = 0;
    thvm_atp_ri_stats(s, &queries, &cands, &matchcalls, &nodevisits,
                      &nodes, &rbuilt);
    double avg_cand = (queries > 0) ? ((double)cands / (double)queries) : 0.0;
    double avg_node = (queries > 0) ? ((double)nodevisits / (double)queries) : 0.0;
    double avg_mc   = (queries > 0) ? ((double)matchcalls / (double)queries) : 0.0;
    printf("   ri-index: %llu queries  %u tree-nodes  %u rules-built\n",
           (unsigned long long)queries, nodes, rbuilt);
    printf("   ri-retrieval: %.3f candidates/query  %.3f tree-nodes/query  %.4f thvm_match/query\n",
           avg_cand, avg_node, avg_mc);
    printf("   ri-cmp: candidates/query vs n_rules=%u  ->  prune-ratio %.4f\n",
           rbuilt, (rbuilt > 0) ? (avg_cand / (double)rbuilt) : 0.0);
    if (s->rule_index != NULL && s->rule_index->q_depth_capped > 0) {
      printf("   ri-descent: %llu depth-cap bails (latent DT-cycle)\n",
             (unsigned long long)s->rule_index->q_depth_capped);
    }
    if (s->unorient_index != NULL && s->unorient_index->q_depth_capped > 0) {
      printf("   ri-descent (unorient): %llu depth-cap bails (latent DT-cycle)\n",
             (unsigned long long)s->unorient_index->q_depth_capped);
    }
    if (s->cp_index != NULL && s->cp_index->q_depth_capped > 0) {
      printf("   ri-descent (cp): %llu depth-cap bails (latent DT-cycle)\n",
             (unsigned long long)s->cp_index->q_depth_capped);
    }
    if (s->cp_subindex != NULL && s->cp_subindex->q_depth_capped > 0) {
      printf("   ri-descent (cp-sub): %llu depth-cap bails (latent DT-cycle)\n",
             (unsigned long long)s->cp_subindex->q_depth_capped);
    }
  }
#endif

  thvm_atp_free(s);
  return (st == ATP_PROVED) ? 0 : 1;
}
