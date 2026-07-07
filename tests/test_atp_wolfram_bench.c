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

// AbelianGroupAxioms (4 equations) reused via L_AND as group op `f` and
// L_NOT as inverse `inv`.  Identity element reuses L_R (a free constant
// label that isn't touched by Sheffer goals).  This is the
// AbelianGroup/InverseOfComposite conjecture from AxiomaticTheory:
//   inv(f(p, q)) = f(inv(q), inv(p))
// over the axioms:
//   f(f(x,y), z)   = f(x, f(y,z))      -- associativity
//   f(x, y)        = f(y, x)           -- commutativity
//   f(x, e)        = x                 -- right identity (e = L_R const)
//   f(x, inv(x))   = e                 -- right inverse
// Used to dump the CP-pick trajectory (THVM_ATP_CP_PICK_TRACE=1) and
// compare against wmcli's trace from the matching .pr file under
// tools/baselines/wm_pr/AbelianGroupAxioms__InverseOfComposite.pr -- the
// first divergent pick is the algorithmic-parity mismatch point.
#define L_AG_E L_R
static void aboig_axioms(Term *l1, Term *r1, Term *l2, Term *r2,
                         Term *l3, Term *r3, Term *l4, Term *r4) {
  Term x = fv(0), y = fv(1), z = fv(2);
  Term e = konst(L_AG_E);
  *l1 = and_op(and_op(x, y), z); *r1 = and_op(x, and_op(y, z));
  *l2 = and_op(x, y);            *r2 = and_op(y, x);
  *l3 = and_op(x, e);            *r3 = x;
  *l4 = and_op(x, not_op(x));    *r4 = e;
}
static void goal_aboig_ioc(Term *l, Term *r) {
  Term p = konst(L_P), q = konst(L_Q);
  *l = not_op(and_op(p, q));
  *r = and_op(not_op(q), not_op(p));
}

// GroupAxioms (3 equations): same f/inv/e as agioc but no commutativity.
// Conjecture (LeftIdentity): f(e, p) = p -- the right-id-and-right-inv
// implies left-id derivation.  Mirrors wm_pr/GroupAxioms__LeftIdentity.pr.
static void group_axioms(Term *l1, Term *r1, Term *l2, Term *r2,
                         Term *l3, Term *r3) {
  Term x = fv(0), y = fv(1), z = fv(2);
  Term e = konst(L_AG_E);
  *l1 = and_op(and_op(x, y), z); *r1 = and_op(x, and_op(y, z));
  *l2 = and_op(x, e);            *r2 = x;
  *l3 = and_op(x, not_op(x));    *r3 = e;
}
static void goal_group_leftid(Term *l, Term *r) {
  Term p = konst(L_P);
  Term e = konst(L_AG_E);
  *l = and_op(e, p);
  *r = p;
}

// AbelianMcCuneAxioms (1 single CAG equation): cancellative abelian group
// in one equation, `and(and(and(x,y),z), not(and(x,z))) = y`.  L_AND
// stays the binary op, L_NOT the inverse, no identity (CAG has none).
// Mirrors wm_pr/AbelianMcCuneAxioms__Associativity.pr.
static void amc_axiom(Term *l, Term *r) {
  Term x = fv(0), y = fv(1), z = fv(2);
  *l = and_op(and_op(and_op(x, y), z), not_op(and_op(x, z)));
  *r = y;
}
// Associativity conjecture: and(skC1, and(skC2, skC3)) =
// and(and(skC1, skC2), skC3).  L_P, L_Q, L_R are 0-arity skolems.
static void goal_amc_assoc(Term *l, Term *r) {
  Term p = konst(L_P), q = konst(L_Q), rr = konst(L_R);
  *l = and_op(p, and_op(q, rr));
  *r = and_op(and_op(p, q), rr);
}

// McCune InverseOfInverse: not(not(skC1)) = skC1 (the McCune-single-axiom
// CAG characterization derives double-negation).  Same axiom shape as
// goal_mccune (mccune_axiom_lhs) -- L_AND binary, L_NOT unary, skC1 ground.
static void goal_mc_ioi(Term *l, Term *r) {
  Term p = konst(L_P);
  *l = not_op(not_op(p));
  *r = p;
}

// McCune Associativity: associativity over 3 skolem constants, same
// McCune-single-axiom characterization as goal_mc_ioi.  Equivalent to
// amc_assoc but with the long McCune axiom instead of the short AbelianMcCune.
static void goal_mc_assoc(Term *l, Term *r) {
  Term p = konst(L_P), q = konst(L_Q), rr = konst(L_R);
  *l = and_op(p, and_op(q, rr));
  *r = and_op(and_op(p, q), rr);
}

// HigmanNeumann single-axiom group: single binary op `fop` (reuse L_NAND
// because both are 1-binary-op shapes; KBO doesn't care about the label
// name).  Axiom: fop(x, fop(fop(fop(fop(x,x),y),z), fop(fop(fop(x,x),x),z))) = y.
// Goal LeftIdentity: fop(fop(p,p), fop(fop(p,p), p)) = p.
// Mirrors wm_pr/HigmanNeumannAxioms__LeftIdentity.pr.
static Term hn_axiom_lhs(void) {
  Term x = fv(0), y = fv(1), z = fv(2);
  Term xx = nand2(x, x);
  Term i1 = nand2(nand2(nand2(xx, y), z), nand2(nand2(xx, x), z));
  return nand2(x, i1);
}
static void goal_hn_leftid(Term *l, Term *r) {
  Term p = konst(L_P);
  Term pp = nand2(p, p);
  *l = nand2(pp, nand2(pp, p));
  *r = p;
}

// ---------------------------------------------------------------------------
// Waldmeister .pr problem-file mode.  A goal argument containing ".pr" is
// parsed as a WM problem file (tools/baselines/wm_pr/*.pr): SIGNATURE
// symbols get CTR labels 1..n in declaration order, the ORDERING section
// supplies the KBO weights + total precedence (the `a > b > c` chain,
// descending), VARIABLES name the TAG_FVR ids, and EQUATIONS / CONCLUSION
// are prefix-notation first-order terms.  This runs the byte-same problem
// wmcli runs, so the CP-selection trace (THVM_ATP_CP_PICK_TRACE) can be
// aligned position-by-position against `wmcli -a 4` -- see
// tools/wm_align_sweep/.  Header (signature/ordering) parses before
// thvm_atp_init so the KboConfig is ready; terms parse afterwards at the
// goal-dispatch site like every built-in goal (no Term may exist before
// the engine owns the heap roots).

#include <ctype.h>

#define PR_MAX_SYMS 32u
#define PR_MAX_VARS 16u
#define PR_MAX_EQNS 16u
#define PR_NAME_LEN 64u

typedef struct {
  char name[PR_NAME_LEN];
  u32  arity;
} PrSym;

// User symbols get labels PR_LABEL_BASE.. in declaration order; labels 1
// and 2 are ATP_RESERVED_LABEL_MIN_CONST / _MIN_CONST2 (the engine's FVI
// grounding constants, see thvm_atp_init) -- same convention as the WL
// encoder (encodeAtpTermInit pre-seeds cAtp1/cAtp2 and starts users at 3).
#define PR_LABEL_BASE 3u

typedef struct {
  char  path[1024];
  PrSym syms[PR_MAX_SYMS];
  u32   n_syms;
  char  vars[PR_MAX_VARS][PR_NAME_LEN];
  u32   n_vars;
  u32   weights[PR_MAX_SYMS + PR_LABEL_BASE];     // by label, label 0 unused
  u32   precedence[PR_MAX_SYMS + PR_LABEL_BASE];  // by label; higher = greater
  Term  eq_l[PR_MAX_EQNS], eq_r[PR_MAX_EQNS];
  u32   n_eqns;
  Term  goal_l, goal_r;
} PrProblem;

static void pr_die(const PrProblem *pr, const char *msg, const char *at) {
  fprintf(stderr, "pr parse error (%s): %s%s%.40s\n",
          pr->path, msg, (at != NULL) ? " at: " : "", (at != NULL) ? at : "");
  exit(2);
}

// Mark every 0-ary constructor label occurring in `t` (an axiom side).  Used by
// the -auto weight heuristic to tell "defined operator" constants (which appear
// in the axioms and are consumed by their reductions) from goal-only skolems.
static void pr_mark_axiom_consts(Term t, u8 *is_ax_const, u32 cap) {
  if (term_tag(t) != TAG_CTR) return;
  u32 n = term_ctr_n(t);
  if (n == 0u) {
    u32 e = term_ext(t);
    if (e < cap) is_ax_const[e] = 1u;
    return;
  }
  for (u32 i = 0; i < n; i++)
    pr_mark_axiom_consts(term_ctr_at(t, i), is_ax_const, cap);
}

// Label of `name[0..n)`, or 0 when unknown.
static u32 pr_sym_find(const PrProblem *pr, const char *s, size_t n) {
  for (u32 i = 0; i < pr->n_syms; i++) {
    if (strlen(pr->syms[i].name) == n &&
        memcmp(pr->syms[i].name, s, n) == 0) {
      return i + PR_LABEL_BASE;
    }
  }
  return 0u;
}

static int pr_var_find(const PrProblem *pr, const char *s, size_t n) {
  for (u32 i = 0; i < pr->n_vars; i++) {
    if (strlen(pr->vars[i]) == n && memcmp(pr->vars[i], s, n) == 0) {
      return (int)i;
    }
  }
  return -1;
}

// `name: ANY ANY -> ANY` -- one symbol declaration; arity = #ANY before ->.
static void pr_sig_line(PrProblem *pr, const char *body) {
  const char *colon = strchr(body, ':');
  if (colon == NULL) return;  // blank continuation
  const char *a = body;
  while (isspace((unsigned char)*a)) a++;
  const char *b = colon;
  while (b > a && isspace((unsigned char)b[-1])) b--;
  if (b == a) pr_die(pr, "empty symbol name", body);
  if (pr->n_syms >= PR_MAX_SYMS) pr_die(pr, "too many symbols", body);
  PrSym *sy = &pr->syms[pr->n_syms++];
  size_t n = (size_t)(b - a);
  if (n >= PR_NAME_LEN) pr_die(pr, "symbol name too long", a);
  memcpy(sy->name, a, n);
  sy->name[n] = '\0';
  const char *arrow = strstr(colon, "->");
  if (arrow == NULL) pr_die(pr, "signature line without ->", body);
  u32 ar = 0;
  for (const char *p = colon + 1; p + 2 < arrow; p++) {
    if (strncmp(p, "ANY", 3) == 0) { ar++; p += 2; }
  }
  sy->arity = ar;
}

// ORDERING content: "KBO", then `a=1, b=1, ...` weight pairs, then the
// `a > b > c` precedence chain (symbols in DESCENDING precedence).
static void pr_ord_line(PrProblem *pr, const char *body) {
  if (strstr(body, "LPO") != NULL) pr_die(pr, "LPO ordering unsupported", body);
  int is_chain = (strchr(body, '>') != NULL);
  int is_weights = !is_chain && (strchr(body, '=') != NULL);
  if (!is_chain && !is_weights) return;  // the "KBO" line
  u32 rank = 0;
  if (is_chain) {
    // count chain symbols first so the head gets the highest rank;
    // ranks sit above the reserved labels' 1/2 so the FVI minimal
    // constant stays precedence-minimal (the WL-encoder layout).
    for (const char *p = body; *p != '\0'; p++) {
      if (*p == '>') rank++;
    }
    rank += PR_LABEL_BASE;  // n separators = n+1 symbols, base offset
  }
  const char *p = body;
  while (*p != '\0') {
    while (*p != '\0' && !(isalnum((unsigned char)*p) || *p == '_')) p++;
    if (*p == '\0') break;
    size_t n = 0;
    while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
    u32 lab = pr_sym_find(pr, p, n);
    if (lab == 0u) pr_die(pr, "ORDERING names unknown symbol", p);
    if (is_weights) {
      const char *q = p + n;
      while (*q == ' ') q++;
      if (*q != '=') pr_die(pr, "expected = in weight pair", p);
      pr->weights[lab] = (u32)strtoul(q + 1, (char **)&q, 10);
      p = q;
    } else {
      pr->precedence[lab] = rank--;
      p += n;
    }
  }
}

// `X1, X2, X3: ANY` -- variable names, in declaration order = fv ids.
static void pr_vars_line(PrProblem *pr, const char *body) {
  const char *colon = strchr(body, ':');
  const char *end = (colon != NULL) ? colon : body + strlen(body);
  const char *p = body;
  while (p < end) {
    while (p < end && !(isalnum((unsigned char)*p) || *p == '_')) p++;
    if (p >= end) break;
    size_t n = 0;
    while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
    if (pr->n_vars >= PR_MAX_VARS) pr_die(pr, "too many variables", p);
    if (n >= PR_NAME_LEN) pr_die(pr, "variable name too long", p);
    memcpy(pr->vars[pr->n_vars], p, n);
    pr->vars[pr->n_vars][n] = '\0';
    pr->n_vars++;
    p += n;
  }
}

typedef enum {
  PR_SEC_NONE, PR_SEC_SIG, PR_SEC_ORD, PR_SEC_VARS,
  PR_SEC_EQNS, PR_SEC_CONCL, PR_SEC_OTHER,
} PrSec;

// One pass over the file, dispatching content to `on_line(sec, body)`.
static void pr_walk(PrProblem *pr,
                    void (*on_line)(PrProblem *, PrSec, const char *)) {
  FILE *fp = fopen(pr->path, "r");
  if (fp == NULL) pr_die(pr, "cannot open file", NULL);
  char line[8192];
  PrSec sec = PR_SEC_NONE;
  while (fgets(line, sizeof line, fp) != NULL) {
    const char *body = line;
    if (!isspace((unsigned char)line[0]) && line[0] != '\0') {
      if      (strncmp(line, "SIGNATURE", 9)  == 0) { sec = PR_SEC_SIG;   body = line + 9; }
      else if (strncmp(line, "ORDERING", 8)   == 0) { sec = PR_SEC_ORD;   body = line + 8; }
      else if (strncmp(line, "VARIABLES", 9)  == 0) { sec = PR_SEC_VARS;  body = line + 9; }
      else if (strncmp(line, "EQUATIONS", 9)  == 0) { sec = PR_SEC_EQNS;  body = line + 9; }
      else if (strncmp(line, "CONCLUSION", 10) == 0) { sec = PR_SEC_CONCL; body = line + 10; }
      else { sec = PR_SEC_OTHER; continue; }
    }
    on_line(pr, sec, body);
  }
  fclose(fp);
}

static void pr_header_line(PrProblem *pr, PrSec sec, const char *body) {
  switch (sec) {
    case PR_SEC_SIG:  pr_sig_line(pr, body);  break;
    case PR_SEC_ORD:  pr_ord_line(pr, body);  break;
    case PR_SEC_VARS: pr_vars_line(pr, body); break;
    default: break;
  }
}

// Phase 1 (before thvm_atp_init): signature + ordering + variables.
static void pr_load_header(const char *path, PrProblem *pr) {
  memset(pr, 0, sizeof *pr);
  snprintf(pr->path, sizeof pr->path, "%s", path);
  pr_walk(pr, pr_header_line);
  if (pr->n_syms == 0u) pr_die(pr, "no SIGNATURE symbols", NULL);
  for (u32 i = 0; i < pr->n_syms; i++) {
    if (pr->weights[i + PR_LABEL_BASE] == 0u) {
      pr->weights[i + PR_LABEL_BASE] = 1u;  // WM default
    }
  }
  // Reserved FVI constants: KBO weight 1, precedence below every user
  // symbol (1 < 2 < PR_LABEL_BASE <= user chain ranks).
  pr->weights[1] = 1u;
  pr->weights[2] = 1u;
  pr->precedence[1] = 1u;
  pr->precedence[2] = 2u;
}

// Recursive-descent prefix-term parser: `name(arg,arg)` / `name` / var.
static const char *pr_term(PrProblem *pr, const char *s, Term *out) {
  while (*s == ' ') s++;
  size_t n = 0;
  while (isalnum((unsigned char)s[n]) || s[n] == '_') n++;
  if (n == 0) pr_die(pr, "expected identifier", s);
  int v = pr_var_find(pr, s, n);
  if (v >= 0) {
    *out = term_new_fvr((u32)v);
    return s + n;
  }
  u32 lab = pr_sym_find(pr, s, n);
  if (lab == 0u) pr_die(pr, "unknown symbol in term", s);
  const PrSym *sy = &pr->syms[lab - PR_LABEL_BASE];
  const char *p = s + n;
  Term kids[8];
  u32 k = 0;
  if (*p == '(') {
    p++;
    for (;;) {
      if (k >= 8u) pr_die(pr, "arity > 8", s);
      p = pr_term(pr, p, &kids[k]);
      k++;
      if (*p == ',') { p++; continue; }
      if (*p == ')') { p++; break; }
      pr_die(pr, "expected , or ) in term", p);
    }
  }
  if (k != sy->arity) pr_die(pr, "arity mismatch", s);
  *out = term_new_ctr(lab, (k > 0u) ? kids : NULL, k);
  return p;
}

// `lhs = rhs` -- both sides full terms, nothing but whitespace around them.
static void pr_eq_line(PrProblem *pr, const char *body, Term *l, Term *r) {
  const char *eq = strchr(body, '=');
  if (eq == NULL) pr_die(pr, "equation line without =", body);
  const char *p = pr_term(pr, body, l);
  while (*p == ' ') p++;
  if (p != eq) pr_die(pr, "junk before = in equation", p);
  p = pr_term(pr, eq + 1, r);
  while (*p == ' ' || *p == '\n' || *p == '\r') p++;
  if (*p != '\0') pr_die(pr, "junk after equation", p);
}

static void pr_terms_line(PrProblem *pr, PrSec sec, const char *body) {
  const char *p = body;
  while (isspace((unsigned char)*p)) p++;
  if (*p == '\0') return;
  if (sec == PR_SEC_EQNS) {
    if (pr->n_eqns >= PR_MAX_EQNS) pr_die(pr, "too many equations", body);
    pr_eq_line(pr, body, &pr->eq_l[pr->n_eqns], &pr->eq_r[pr->n_eqns]);
    pr->n_eqns++;
  } else if (sec == PR_SEC_CONCL) {
    if (pr->goal_l != 0) pr_die(pr, "multiple CONCLUSION lines", body);
    pr_eq_line(pr, body, &pr->goal_l, &pr->goal_r);
  }
}

// Phase 2 (after thvm_atp_init, at the goal-dispatch site): the terms.
static void pr_load_terms(PrProblem *pr) {
  pr_walk(pr, pr_terms_line);
  if (pr->n_eqns == 0u) pr_die(pr, "no EQUATIONS", NULL);
  if (pr->goal_l == 0)  pr_die(pr, "no CONCLUSION", NULL);
}

int main(int argc, char **argv) {
  thvm_init();

  const char *goal = (argc > 1) ? argv[1] : "thm";
  u32    step_cap  = (argc > 2) ? (u32)strtoul(argv[2], NULL, 10) : 200000u;
  double wall_cap  = (argc > 3) ? strtod(argv[3], NULL)          : 120.0;

  // .pr file mode (see PrProblem above): parse the header now so the
  // KboConfig below carries the file's signature + ordering.
  static PrProblem prp;
  int pr_mode = (strstr(goal, ".pr") != NULL);
  if (pr_mode) pr_load_header(goal, &prp);
  // Load the equation/goal terms up front too (they only need thvm_init, done
  // above) so the -auto weight heuristic below can inspect the axioms before
  // the KBO config is finalised.  The later pr_load_terms call is guarded.
  if (pr_mode) pr_load_terms(&prp);

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
  // Default precedence (Sheffer): WM .pr files put `skC > nand` --
  // skolem constants HIGH, binary operator LOW.  Previously thvm had
  // nand HIGH which orients axiom CPs differently than WM and diverts
  // the trajectory.  Mirrors wm_pr/WolframAxioms__*.pr ORDERING.
  // Labels: 1=nand, 2-4=skolems (P,Q,R), 5=and, 6=not.
  static u32 precedence[7] = { 0u, 1u, 6u, 5u, 4u, 3u, 2u };
  // The default precedence above already matches WM's Fuchs arity
  // ladder (constants > unary > binary) for both Sheffer and AbelianGroup
  // paths -- L_P/Q/R skolems HIGH, L_NOT MID, L_NAND/L_AND LOW.
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

  // .pr mode: the file's ORDERING section IS the reduction ordering --
  // KBO with the declared per-symbol weights and the `>` chain as the
  // total precedence (all wm_pr files are KBO; pr_ord_line rejects LPO).
  if (pr_mode) {
    cfg.weights    = prp.weights;
    cfg.precedence = prp.precedence;
    cfg.n_labels   = prp.n_syms + PR_LABEL_BASE;
    cfg.var_weight = 1u;
    use_lpo  = 0;
    use_kbo0 = 0;
    // -auto weight heuristic (ATP_BENCH_AUTO_WEIGHTS=1): mirror WM's Automodus,
    // which self-selects the reduction order (PhilMarlow) and IGNORES the .pr's
    // declared ORDERING -- so the .pr's uniform weight-1 is NOT what `wmcli
    // -auto` uses.  A defined-operator constant (0-ary, occurring in an axiom,
    // consumed by its own reduction e.g. W in (Wx)y=(xy)y) must weigh MORE than
    // the goal-only skolems it duplicates, else the reduction runs uphill and
    // the goal never normalises (thvm then saturates 549 CPs where wmcli -auto
    // proves in 7).  Give such constants weight 2; skolems + application stay 1.
    // Byte-identical to `wmcli -auto` on SKIToBCKW c1/c2/c3.  Off by default so
    // the plain `-a 4` matrix track (which DOES use the .pr weights) is unchanged.
    if (getenv("ATP_BENCH_AUTO_WEIGHTS") != NULL) {
      u32 cap = prp.n_syms + PR_LABEL_BASE;
      static u8 ax_const[PR_MAX_SYMS + PR_LABEL_BASE];
      memset(ax_const, 0, sizeof ax_const);
      for (u32 e = 0; e < prp.n_eqns; e++) {
        pr_mark_axiom_consts(prp.eq_l[e], ax_const, cap);
        pr_mark_axiom_consts(prp.eq_r[e], ax_const, cap);
      }
      const char *wv = getenv("ATP_BENCH_AUTO_WT");
      u32 aw = (wv != NULL) ? (u32)atoi(wv) : 2u;
      for (u32 lab = PR_LABEL_BASE; lab < cap; lab++)
        if (ax_const[lab]) prp.weights[lab] = aw;
    }
  }

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
  // to earlier milestones; set it to exercise thvm_atp_gc_collect.
  {
    const char *gc_env = getenv("ATP_BENCH_GC");
    if (gc_env != NULL && gc_env[0] != '\0') {
      u64 half = strtoull(gc_env, NULL, 10);
      if (half > 0) gc_init(half);
    }
  }

  AtpState *s = thvm_atp_init(&cfg, step_cap);
  // Off-heap packed proof trace ON by default.  The on-heap trace is
  // ~99.9% of the GC live set on a deep completion (WolframAxioms/
  // OrAssociativity: trace 32.4M of 46.8M live K-nodes vs rules 11K;
  // 35 heap-pressure collections = 6.6s of a 26.1s batch wall, 25%),
  // and packing collapses that to 11 collections / 0.04s with the
  // trajectory byte-identical (steps/rules/cps exact, both formers)
  // and RSS down ~0.6GB -- every search-time trace reader goes through
  // atp_trace_eq_load, and the WL bridge's post-run materialize has no
  // bench equivalent to break (the bench reads counts only; the goal
  // cone runs on the packed trace exactly like the WL wrapper).  Kill
  // switch: THVM_ATP_NO_TRACE_PACK=1 restores the on-heap trace
  // (THVM_ATP_TRACE_PACK=1 is now redundant here).
  s->trace_pack = atp_env_on("THVM_ATP_NO_TRACE_PACK") ? 0u : 1u;
  if (use_lpo) thvm_atp_set_lpo(s, &lpo);
  // ENIGMA Tier 2: THVM_ATP_GNN_ASSET=<path.safetensors> loads a
  // pretrained GCN scorer (the bundled GCNAtpScorer asset or a
  // TAtpSaveGnnScorer file) C-direct, and THVM_ATP_GNN_RERANK_PERIOD
  // (read in thvm_atp_init) > 0 makes thvm_atp_step re-rank the live CP
  // queue with it.  Off unless both are set, so the baseline is unchanged.
  {
    const char *ga = getenv("THVM_ATP_GNN_ASSET");
    if (ga != NULL && ga[0] != '\0') {
      int loaded = thvm_atp_load_gnn_safetensors(ga);
      // THVM_ATP_GNN_COOP_RATIO=N: route the GNN to the coop secondary
      // dimension (cp_pri2) and pick from it every N-th selection, so the
      // GNN guides selection alongside the primary preset (e.g.
      // THVM_ATP_WALDMEISTER) instead of overwriting its heap.  Unset =
      // GT-mode (GNN overwrites the primary priorities).
      const char *gc = getenv("THVM_ATP_GNN_COOP_RATIO");
      u32 coop = (gc != NULL) ? (u32)strtoul(gc, NULL, 10) : 0u;
      if (coop > 0u) thvm_atp_set_gnn_coop(s, coop);
      printf("gnn-scorer: %s from %s  (rerank period=%u, coop ratio=%u)\n",
             loaded ? "LOADED" : "FAILED-TO-LOAD", ga, s->gnn_rerank_period, coop);
    }
  }
  // CP-weight mode: when `THVM_ATP_CP_WEIGHT` (an `AtpCpWeightMode`
  // integer) is exported, override the mode for the experiment;
  // otherwise leave the engine default (ATP_CP_WEIGHT_GT) in place
  // (or the WALDMEISTER preset's CH_MaxWeight if that is set below).
  // The env-override block runs AFTER the WALDMEISTER preset so a user
  // can opt out of the preset's mode without disabling the preset.
  const char *env_cp_weight = getenv("THVM_ATP_CP_WEIGHT");
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
  // THVM_ATP_CP_SET_IR_PERIOD: override the default 16-rule period for
  // the full-R CP-queue interreduction sweep.  Lower = more frequent.
  {
    const char *pr = getenv("THVM_ATP_CP_SET_IR_PERIOD");
    if (pr != NULL && pr[0] != '\0') {
      s->cp_set_ir_period = (u32)strtoul(pr, NULL, 10);
    }
  }
  // THVM_ATP_PERM_SUB=1: port of WM `GZ_ACVerzichtbar`.  Drops a CP at
  // push time when its two sides are equal as multisets at the top
  // (commutativity-shaped, e.g. nand(x,y) = nand(y,x)).  On AndAssoc
  // faithful-preset trajectory this is the single biggest source of
  // the 11x per-CP LPO load gap vs wmcli.
  {
    const char *ps = getenv("THVM_ATP_PERM_SUB");
    if (ps != NULL && ps[0] != '\0' && ps[0] != '0') {
      thvm_atp_set_use_perm_subsume(s, 1u);
    }
    // Optional: restrict perm_sub to specific top-symbol labels.  Format
    // is a comma-separated list of label ints (e.g. "1" for nand,
    // "1,5" for nand + and).  Default (unset) = drop all AC-equal CPs.
    const char *pm = getenv("THVM_ATP_PERM_SUB_MASK");
    if (pm != NULL && pm[0] != '\0') {
      u64 mask = 0ull;
      const char *p = pm;
      while (*p) {
        char *end = NULL;
        unsigned long lab = strtoul(p, &end, 10);
        if (end == p) break;
        if (lab < 64u) mask |= (1ull << lab);
        p = end;
        if (*p == ',') p++;
      }
      thvm_atp_set_perm_subsume_mask(mask);
    }
    // THVM_ATP_RULE_SUB_DROP=1: port of WM `dokgS` (KPBehandelt's
    // SS_TermpaarSubsummiertVonGM branch).  Push-time rule-subsumption
    // drop -- a CP whose two sides are directly subsumed by an existing
    // rule's pattern is joinable in one step via that rule, so dropping
    // is sound.  Cheaper than the full trivial-join normalize for the
    // dropped subset.
    const char *rsd = getenv("THVM_ATP_RULE_SUB_DROP");
    if (rsd != NULL && rsd[0] != '\0' && rsd[0] != '0') {
      thvm_atp_set_use_rule_subsume_drop(s, 1u);
    }
  }
  // THVM_ATP_W2_MODULO / THVM_ATP_W2_MODE: K-D Heap alternating-
  // dimension selection (port of WM `CPdimension` in KPVerwaltung.c).
  // Every w2_modulo-th selection picks min-cp_pri2 instead of the
  // primary heap root.  cp_pri2 is computed with the alternate weight
  // mode (e.g. CH_MaxWeight when primary is CH_MixWeight).  Surfaces
  // structurally simple CPs buried under the primary ordering.
  {
    const char *w2m = getenv("THVM_ATP_W2_MODULO");
    const char *w2d = getenv("THVM_ATP_W2_MODE");
    if (w2m != NULL && w2m[0] != '\0') {
      u32 mod = (u32)strtoul(w2m, NULL, 10);
      u32 mode = (w2d != NULL && w2d[0] != '\0')
                   ? (u32)strtoul(w2d, NULL, 10)
                   : (u32)ATP_CP_WEIGHT_MAX;
      thvm_atp_set_w2(s, mod, (u8)mode);
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
      // FIFO interleave = `wmcli -auto` itl(mi): `-pq interleave=1.50`
      // -> moduloCP 51, thresholdCP 1 (see atp_cp_fifo_dimension; the
      // threshold defaults to 1 under the preset, and
      // THVM_ATP_FIFO_THRESHOLD=0 restores the plain-wmcli no-FIFO
      // mode the older matrix rows were recorded against).
      // THVM_ATP_SELECTION_RATIO env overrides the modulo.
      {
        const char *sr = getenv("THVM_ATP_SELECTION_RATIO");
        u32 ratio = (sr != NULL && sr[0] != '\0')
                      ? (u32)strtoul(sr, NULL, 10) : 51u;
        thvm_atp_set_selection_ratio(s, ratio);
      }
      thvm_atp_set_use_rhs_interreduce(s, 1u);
      thvm_atp_set_use_unfailing_cp(s, 1u);
      // WM orphan layout (-ocrit default ON): lazy at-pop discard ON,
      // eager interreduce-time queue sweep OFF -- WM has no such sweep
      // (its only orphan mechanism is KPVerwaltung.c selectNonOrphan),
      // and the sweep changes live-queue composition vs WM.
      // THVM_ATP_ORPHAN / THVM_ATP_EAGER_SWEEP envs override below.
      thvm_atp_set_use_orphan_murder(s, 1u);
      thvm_atp_set_use_eager_orphan_sweep(s, 0u);
      thvm_atp_set_use_unorient_index(s, 1u);
      // WM KPBehandelt semantics (KPVerwaltung.c:435-467): CPs with
      // combined RAW size < 50 are treated (doR-only normalize) at
      // push; BIGGER CPs queue RAW and weigh on their RAW sides --
      // thvm's lazy path implements exactly this gate, so lazy stays
      // ON for -auto faithfulness.  THVM_ATP_LAZY_NORM=0 gives the
      // eager A/B (treats ALL CPs; proves OrAssociativity in ~45s but
      // weighs >=50-size CPs on treated forms, off WM's trajectory).
      // The faithful config's cost problem is the pop-time normalize
      // of stored-raw giants at the interreduction avalanche -- an
      // allocation-profile bug to fix in the engine, not a semantics
      // knob (see project_atp_wm_speed_profiled 2026-07-02).
      thvm_atp_set_use_lazy_normalize(s, 1u);
      // WM's KPV_KPMengeInterreduzieren (CP-set IR) is NOT enabled: the
      // -ki CLI default carries no period and no checkpoints
      // (RUN/Parameter.c:378-387 default InfoString = "statistics=..."
      // only), so SP_IstStichpunkt never fires (WASIC/Stichpunkte.c:
      // 179-198 with Period=0 + empty list; trigger KPVerwaltung.c:
      // 1030-1043), and no Sinai strategy -- including the Orkus
      // fallback StdS the McCune/Group .pr files run (Sinai.h:109,131;
      // PhilMarlow.c:1315) -- emits -ki.  THVM_ATP_CP_SET_IR=1 is the
      // -ki opt-in (period via THVM_ATP_CP_SET_IR_PERIOD).
      // WM-faithful IR-victim demotion (KPV_IROpferBehandeln /
      // IR_PufferAuslesen): a rule demoted by interreduction re-queues
      // its ORIGINAL sides after CP generation (late FIFO age), with
      // the KPBehandelt -kg r treatment (size-gated oriented-only
      // renormalize + joined-discard).  Closes the McCune-II pick-4
      // fork (the slice-reduced victim re-queued early at weight 461
      // where WM's original pair re-queues late at 505 and joins away
      // on re-selection).  THVM_ATP_WM_DEMOTE env overrides.
      thvm_atp_set_use_wm_demote(s, 1u);
      // WM -ks "s" stage (default "r:e:s:p", RUN/Parameter.c:407): a
      // popped UNORIENTABLE pair subsumed by an existing unorientable
      // equation drops before orientation (KPV_Select,
      // KPVerwaltung.c:667 SS_TermpaarSubsummiertVonGM).
      // THVM_ATP_POP_SUBSUME env overrides.
      thvm_atp_set_use_pop_subsume(s, 1u);
      // WM E-set subsumption (GMSubsummierenMitGleichung,
      // INF/Interreduktion.c:251-274): a new unorientable equation
      // destroys every existing E-member it subsumes -- no requeue,
      // no CP made.  Unconditional in WM's IR_InterreduktionLinks
      // (:371-373) for every non-rule new fact, so part of the
      // faithful preset.  THVM_ATP_ESET_SUBSUME env overrides.
      thvm_atp_set_use_eset_subsume(s, 1u);
      // WM flatterm-faithful (over-eager counter-cross) eset-subsume matcher
      // (MO_TermpaarSubsummiertZweites): WM removes axiom2 `x*x = x*(y*(y*y))`
      // on commutativity-add via a binding-slot vs variable-symbol cross that
      // over-matches the Gegenrichtung of commutativity (firstdiv-19).  The
      // matcher is faithful and removes axiom2 correctly, BUT removing it then
      // orphan-murders its queued CPs more broadly than WM's KPV_KillParent --
      // WM re-derives axiom2 and reselects it at pick 16, thvm kills that CP --
      // so the standalone matcher regresses PLAIN soa firstdiv 19->16 (the
      // committed plain-matrix row keeps that honest residual).  DEFAULT ON
      // (matching the WL "Waldmeister" preset's FlatSubsume->True): the
      // WM_SUBDUMP ground truth on Sheffer -auto shows commutativity's E-add
      // MUST GM-subsume E1/E4 out of the E-set (GMSubsummierenMitGleichung +
      // the MO_TermpaarSubsummiertZweites cursor quirk) or the re-derived
      // duplicate at fact 10 never re-enters -- arming this moved the Sheffer
      // -auto fact prefix 9 -> 51 and holds OA 1500 / Meredith 177 /
      // combinators 6/6 exactly.  THVM_ATP_FLAT_SUBSUME=0 opts out.
      {
        const char *fs = getenv("THVM_ATP_FLAT_SUBSUME");
        thvm_atp_set_use_flat_subsume(
            s, (fs != NULL && fs[0] == '0') ? 0u : 1u);
      }
      // Commutativity-aware E-set subsumption widening (DEFAULT OFF):
      // drops a redundant equation whose RHS is one top-`.`-swap from the
      // new equation's RHS under a live commutativity axiom -- WM's
      // commutativity-normalised Interreduktion.  Drops the soa slot15
      // equation exactly as WM does, but is a DIAGNOSTIC knob, not a parity
      // win: ON forks soa firstdiv 125->99 (slot15 uniquely parents the
      // displaced pick-99 COMM copy) and explodes commutative-ring baselines
      // via remove-and-rederive thrash.  THVM_ATP_COMM_SUBSUME opts in.
      thvm_atp_set_use_comm_subsume(
          s, (getenv("THVM_ATP_COMM_SUBSUME") != NULL) ? 1u : 0u);
      // WM IR-victim drain within-leaf chain tiebreak (DEFAULT ON): fold a
      // re-derivation victim's chain index within its discrimination-tree leaf
      // into the drain-order key so two victims sharing a leaf re-enter the
      // queue in WM's BK_Regeln -> TP_Nachf head-first chain order
      // (DSBaumKnoten.h:482-495), not thvm's slot-scan push order.  The w=224
      // nested cube-mirror pair at soa pick 1505 (traces 2570 at chainpos 2,
      // 2817 at chainpos 1 in equation leaf rank 1) collided on one leaf-rank
      // key; this tiebreak orders them head-first like WM.  Advances soa
      // firstdiv past 1505, and (with DRAIN_REVFACE + the drain-key snapshot)
      // fixes the OA -auto @8582 age-lane transposition: the three x3-template
      // equation victims of the rule-627 avalanche share ONE Gleichungsbaum
      // leaf, and only the chain tiebreak reproduces WM's newest-first pull
      // order (eqn -7 before -6 before -5 = re-add weights 624, 1088, 528).
      // THVM_ATP_DRAIN_CHAINPOS=0 restores the pre-fix rank-only key.
      {
        const char *dc = getenv("THVM_ATP_DRAIN_CHAINPOS");
        thvm_atp_set_use_drain_chainpos(
            s, (dc != NULL && dc[0] == '0') ? 0u : 1u);
      }
      // WM GMInterred reducible-face drain order (DEFAULT ON).  GMInterred
      // (RE_forGMReferenzen = BK_ReferenzDurchlauf, DSBaumKnoten.h:499-514)
      // pulls each IR-victim through the face the new rule REDUCES, so a
      // cube-mirror pair whose distinguished faces coincide (`x.x = cube`,
      // soa pick 1558: traces 2092/-14 and 3089/-18) drains by the distinct
      // reducible cube faces' leaf order, not the colliding `x.x` leaf.
      // Advances soa firstdiv past 1558; part of the OA -auto @8582 fix
      // (the x3-template victims key at their REDUCIBLE template face's
      // shared leaf, matching WM's pull).  THVM_ATP_DRAIN_REVFACE=0
      // restores the distinguished-face-only key.
      {
        const char *dr = getenv("THVM_ATP_DRAIN_REVFACE");
        thvm_atp_set_use_drain_revface(
            s, (dr != NULL && dr[0] == '0') ? 0u : 1u);
      }
      // WM-faithful distinguished-direction E-set subsumption (DEFAULT OFF).
      // The flat new-equation E-subsumer is 4-way (both pattern AND both subject
      // orientations of the old equation); WM's GMSubsummierenMitGleichung
      // (INF/Interreduktion.c:261) tests the old equation ONLY in its
      // distinguished (stored) orientation (TP_RichtungAusgezeichnet).  Drops the
      // two subject-swapped match attempts so WM-kept equations survive -- the
      // faithful fix for the Meredith firstdiv-6078 eqn-12 divergence,
      // superseding the retired content gate.  THVM_ATP_ESET_DISTDIR opts in
      // (also turned on by FORMATION_FIFO below).  See soa.txt.
      thvm_atp_set_use_eset_distdir(
          s, (getenv("THVM_ATP_ESET_DISTDIR") != NULL) ? 1u : 0u);
      // WM-faithful discrimination-tree construction (DEFAULT OFF).  Splices
      // BlattAufgeteilt parallels for genuine enclosing subterms AFTER the model
      // (WM AltesBlattPolieren DSBaumOperationen.c :523-526), but keeps the
      // NeuesBlattEinhaengen immediate-strict-ancestor jump (:466-467)
      // head-inserted in BOTH construction modes -- so the runtime tops DFS
      // reaches a split leaf's parallel face at WM's arrival rank.  With the
      // FormationFifo base this reaches soa firstdiv 1953 (closes the rule-44
      // partner-batch transposition at pick 1884 that the bare splice-after
      // exposed).  NOT auto-enabled under FormationFifo: it neither advances nor
      // regresses soa's firstdiv (the residual at 1953 is the equation-tree
      // round-robin batch, untouched by this rule-tree construction), so it
      // ships as the byte-identical-OFF faithful-construction foundation.
      // THVM_ATP_WM_TRIE_FAITHFUL opts in.  See soa.txt.
      thvm_atp_set_use_wm_trie_faithful(
          s, (getenv("THVM_ATP_WM_TRIE_FAITHFUL") != NULL) ? 1u : 0u);
      // WM Gleichungsbaum insertion order (lhs face first, rhs face second).
      // DEFAULT OFF; THVM_ATP_WMO_INSERT_LR opts in.  See soa.txt.
      thvm_atp_set_use_wmo_insert_lr(
          s, (getenv("THVM_ATP_WMO_INSERT_LR") != NULL) ? 1u : 0u);
      // WM CP-formation FIFO lineage (DEFAULT OFF): the faithful-stack
      // master knob.  CP formation emits natively in WM's U1 order (the
      // single-walk former), so this arms only the shared WM order
      // refinements outside formation: the IR-victim drain-order keys
      // (DRAIN_CHAINPOS / DRAIN_REVFACE) and the distinguished-direction
      // E-set subsumption (ESET_DISTDIR).  The setter runs last, after
      // the individual env reads above, so it ORs them on.  See
      // tools/baselines/wm_align_reports/soa.txt.
      thvm_atp_set_use_formation_fifo(
          s, (getenv("THVM_ATP_FORMATION_FIFO") != NULL) ? 1u : 0u);
      // Overlap-exhausted-equation gate (WM: a newly-derived commutativity
      // overlaps an equation's FRESH re-derivation, not the stale exhausted
      // original).  Verified to preserve all 73 byte-identical baselines and
      // advance ShefferAxioms__OrAssociativity firstdiv 14->19.  ON in the
      // faithful preset; THVM_ATP_NO_OVERLAP_EXHAUST env disables.
      s->use_overlap_exhaust =
          (getenv("THVM_ATP_NO_OVERLAP_EXHAUST") == NULL) ? 1u : 0u;
      // WM LRSortieren side-canonicalisation (SpezNormierung.c:517-534):
      // store each derived unorientable equation with WM's canonical side
      // order (variable < non-variable, preorder).  DEFAULT OFF: a
      // UNIVERSAL canonicalisation regresses the Sheffer OrAssociativity
      // selection prefix 20->16 -- WM does NOT re-sort every derived
      // equation; it keeps the CP-formation side order for most E-members
      // and only the intake axioms run LRSortieren.  Matching WM's exact
      // per-equation side order needs the CP-FORMATION reduct-assignment
      // geometry (which parent is overlapped), not a blanket re-sort.
      // Opt-in via THVM_ATP_LR_SORTIEREN for experimentation.
      s->use_lr_sortieren =
          (getenv("THVM_ATP_LR_SORTIEREN") != NULL) ? 1u : 0u;
      // WM CP-formation side geometry (Unifikation1.c:916-917): store
      // each derived UNORIENTABLE equation with WM's KPLinks=sigma(r_i)
      // (the overlapped rule's RHS, WM's distinguished face) as the stored
      // LHS and the reduct on the RHS -- the geometry swap of thvm's
      // natural popped CP order, so the equation's own CP batch overlaps
      // WM's redex set instead of forking on the stored term structure.
      // ADVANCES Sheffer OrAssociativity 20->52 (the C-shape now stored
      // short-side-left like WM, consumed at the eq-7 batch instead of
      // deferred to a late commutativity overlap).
      //
      // The swap itself is now PARENT-OVERLAP-AWARE (atp_cp_wm_side_swaps):
      // a derived superposition CP is swapped per its overlap face-combo
      // (tagged at formation, carried on the TRACE_CP record), not by a
      // blanket reorder, and an initial axiom is canonicalised by
      // LRSortieren -- so when on, the swap fires only where WM's
      // KPLinks=sigma(r_Vater) geometry actually applies.  That faithful
      // gate advances the Sheffer OrAssociativity prefix to 124.
      //
      // DEFAULT OFF: the residual axiom-orientation case is irreducible --
      // soa needs the unorientable-axiom swap (its Sheffer C-shape stored
      // short-side-left), while CombinatorAxioms__BCKWToSKI__c2 needs the
      // SAME axiom NOT swapped (a downstream FIFO-age cascade re-times one
      // w1=41 cCombinatorI CP and forks selection at pick 55).  No single
      // axiom rule satisfies both, so the bench DEFAULT keeps the swap OFF
      // (matching the /goal clean baselines, incl. BCKWToSKI__c2 -> Y/109);
      // the soa Sheffer prefix is measured opt-in via THVM_ATP_CP_SIDE.
      // (THVM_ATP_NO_CP_WM_SIDE remains an explicit force-off override.)
      // FOLLOW-UP (2026-06-20): the faithful axiom-intake geometry that
      // would reconcile the two -- so the swap can default ON again -- is a
      // deeper WM intake-ordering port, left open.
      s->use_cp_side =
          (getenv("THVM_ATP_CP_SIDE") != NULL
           && getenv("THVM_ATP_NO_CP_WM_SIDE") == NULL) ? 1u : 0u;
      // Push-time queue-vs-queue subsumption OFF: thvm-native filter
      // with no WM counterpart (recentCPinsert queues every treated
      // survivor; SS_TermpaarSubsummiertTermpaar's only set-level
      // caller is the E-set sweep, never the passive queue).  Each
      // WM insert consumes a w2 = ++CPNr FIFO age, so the filter
      // shifts every later CP's age vs WM.  THVM_ATP_QUEUE_SUBSUME
      // env overrides.
      thvm_atp_set_use_queue_subsume(s, 0u);
      // WM CP-emission ORDER: sort each new fact's CP batch into
      // Waldmeister's emission order (U1_KPsBildenZuFaktum phase walk
      // + DSBaum leaf-list order) before pushing, so equal-weight CPs
      // receive their FIFO ages (w2) in WM's order.  Closes the
      // McCune-II sels 96-101 equal-weight tie residual.
      // THVM_ATP_EMISSION_ORDER env overrides.
      thvm_atp_set_use_emission_order(s, 1u);
      // WM loader-level axiom INTAKE: the spec loader canonically
      // sorts the initial equation set (SpezNormierung, WASIC/
      // SpezNormierung.c:758-791) and the `-clas` default
      // initial=ultimate (RUN/Parameter.c:165-167) stamps every axiom
      // w1 = MIN_INT / w2 = ++CPNr in SORTED order, so axioms pop
      // FIRST in canonical-sort FIFO order -- thvm popped them by
      // computed weight, interleaved with early CPs (the firstdiv 1-3
      // class of the alignment matrix, 35/43 divergent rows).
      // THVM_ATP_INTAKE_ORDER env overrides.
      thvm_atp_set_use_intake_order(s, 1u);
      // WM normal-form STRATEGY: `-nf mixmost` (the CLI default,
      // RUN/Parameter.c:418-419; NormalformZuRegelnOderGleichungen-
      // AufNochmal, NF/NFBildung.c:349-377) -- local fixpoint at the
      // reduced position + ancestor ascent, never a rescan from the
      // root.  The legacy walk is WM's "outermost"; on non-confluent
      // mid-completion R the strategies reach different NFs, flipping
      // generation-time CP join verdicts (the duplicate-CP
      // multiplicity divergence class: McCune Assoc/EqInv @140,
      // HigmanNeumann Assoc @48).  THVM_ATP_MIXMOST_NF env overrides.
      thvm_atp_set_use_mixmost_nf(s, 1u);
      // Backward subsumption / backward demodulation are NOT in this
      // preset: they are Vampire mechanisms (bs=unit_only / bd=all)
      // with no WM analog.  WM's only backward treatment of existing
      // facts on a new-fact arrival is ArbeitsAufnahme's interreduction
      // (Hauptkomponenten.c:308-331): GMSubsummierenMitGleichung
      // (equation-vs-equation subsumption only, Interreduktion.c:
      // 251-274/:371) + GMInterred/RMLinksInterred demote-and-requeue
      // (:280-326) + RMRechtsInterred RHS compose (:329-360) -- all
      // already ported (ESET_SUBSUME, WM_DEMOTE, RHS_INTERREDUCE).
      // Rules are never subsumption victims in WM.  The old "+30% on
      // AndAssoc (iter 162)" enable predated every faithfulness port
      // and amplified rule-set collapses (the post-raw-queuing
      // 549->74 avalanche suspect).  Opt back in via THVM_ATP_BWD_SUB /
      // THVM_ATP_BWD_DEMOD below; VampireUEQ-style WL presets keep
      // them.
      // MNF goal-directed front search -- DROPPED from the preset after
      // SKEL_FRESH=0 became the default: under the corrected FT NF, MNF
      // diverts the saturator's trajectory on mccune (49s -> 60s+ with
      // MNF on, 28s with MNF off) and slows thm (1.9s vs 0.4s).  THVM_ATP
      // _MNF=1 still opts in for problems where MNF helps.
      // Auto-MaxWeight -- DROPPED from the preset (base=0).  The overflow
      // stash is a pure thvm perf heuristic with NO WM analog: WM SUEs
      // every classified CP at its formation age (CPNr), so deferring a
      // heavy treated CP to the stash re-stamps it on re-admission and
      // forks the FIFO/age lane.  Ground truth: ShefferAxioms
      // OrAssociativity -auto, where the age lane pops the eq -2
      // self-overlap CPs (WM cpnr 31/33, w1=2208, ~23 syms/side) that the
      // base-20 stash deferred -- fact stream 204 -> 694/694 firstdiv=NONE
      // with the stash off; OA/Meredith were stash-insensitive (their
      // deferred CPs never surfaced before the proof point).  The old
      // base=20 cut mccune 25.0s -> 16.0s; when chasing that perf point,
      // THVM_ATP_AUTO_MAXW=20 (parsed below the preset block) restores it.
      // CP-weight = CH_MaxWeight = max(|lhs|,|rhs|) symbol count.
      // PORTFOLIO TRADE-OFF: MaxWeight (mode=1) cracks mccune in 11.3s
      // where the engine-default GT does not crack at 60s; but on the
      // SHEFFER/Wolfram axiom (andassoc, wolfram, etc.) the bare engine
      // (no preset, just GT weight, larger heap) PROVES AndAssoc in
      // 11.6s -- WM-PARITY -- while THIS preset's MaxWeight + add-ons
      // (MNF, ORPHAN_MURDER, LAZY_NORMALIZE, AUTO_MAXW)
      // divert the trajectory and the cracker is never derived.  When chasing a Sheffer/Wolfram cracking-time result,
      // use the bare engine instead:
      //   THVM_HEAP_CELLS=$((1<<30)) ./bin/test_atp_wolfram_bench \
      //       andassoc 99999999 60
      // (no THVM_ATP_WALDMEISTER; just default GT weight + larger heap).
      // WM CLI's faithful default is CH_MixWeight (CriticalPairWeight ->
      // Mix in the WL preset), NOT Max.  Direct trace diff on
      // AbelianGroup/InverseOfComposite (tools/baselines/trace_diff_agioc.md):
      //   MAX  -> assoc picked as rule #5 (875 CP picks to PROVE).
      //   Mix  -> assoc picked as rule #2 (matching WM), 16 picks to PROVE.
      //   mccune drops from 14.4s -> 10.5s under the same flip.
      // Mix is byte-identical to WM's CH_MixWeight formula.
      thvm_atp_set_cp_weight_mode(s, ATP_CP_WEIGHT_MIX);
      // INCR_IR: incremental CP-set interreduce (0a98478e).  Sound by
      // construction; verified PROVED on agioc/glid/mccune/thm/cpl1/subl2
      // (thm even slightly faster 0.4s -> 0.3s).  On wolfram with IR=1,
      // gives +58% rules in 60s (219 -> 346).  Safe to enable by default
      // in the bench preset.
      thvm_atp_set_use_incr_ir(s, 1u);
      // CP_INDEX: discrimination-tree partner retrieval for cp-gen
      // overlap loops.  mccune 9.8s -> 8.3s (-15%); all PROVED cases
      // unchanged.  Wolfram trajectory shifts (fewer rules per second
      // but different exploration).  Safe to enable.
      thvm_atp_set_use_cp_index(s, 1u);
      // WM -auto minimal-constant re-resolution (SO_minimaleKonstante).
      // `wmcli -auto` installs its chosen precedence as a NEW chain
      // (PraezedenzAnalyse, WASIC/SymbolOperationen.c:456-517) WITHOUT
      // re-running NeueFunktionenEinbauen (:210, initial-precedence-only
      // per the :464 comment), then re-resolves SO_minimaleKonstante over
      // that chain (:515 -> minimaleKonstanteBestimmen :112-126, bottom-up
      // scan for the first CONSTANT) -- so -auto grounds free-variable
      // instances with the problem's own minimal-precedence signature
      // constant (OrAssociativity -auto: skC3; zero const1/const2 tokens
      // in the -auto -a 4 OA reference trace), NOT the extra const2.
      // Plain wmcli keeps the initial chain, where NeueFunktionenEinbauen
      // pinned `f > const1 > const2` for every signature symbol f, so
      // plain grounds with const2 <=> thvm's reserved
      // ATP_RESERVED_LABEL_MIN_CONST; the plain-emulation alignment rows
      // (THVM_ATP_FIFO_THRESHOLD=0, run_one.sh) therefore keep the
      // reserved default.  THVM_ATP_MINCONST_SIG=1/0 forces ON/OFF (the
      // OFF form is the kill switch restoring the pre-fix trajectory).
      {
        const char *mcs = getenv("THVM_ATP_MINCONST_SIG");
        const char *fth = getenv("THVM_ATP_FIFO_THRESHOLD");
        u8 plain_emu = (fth != NULL && fth[0] != '\0' &&
                        strtol(fth, NULL, 10) <= 0);
        u8 want = (mcs != NULL && mcs[0] != '\0')
                      ? (u8)(mcs[0] != '0')
                      : (u8)(pr_mode && !plain_emu);
        if (want && pr_mode) {
          u32 mc_lab = 0u;
          for (u32 k = 0; k < prp.n_syms; k++) {
            u32 lab = k + PR_LABEL_BASE;
            if (prp.syms[k].arity != 0u) continue;
            if (mc_lab == 0u ||
                prp.precedence[lab] < prp.precedence[mc_lab]) {
              mc_lab = lab;
            }
          }
          if (mc_lab != 0u) thvm_atp_set_min_const_label(s, mc_lab);
        }
      }
    }
  }
  // THVM_ATP_INITIAL_ULTIMATE=1: port of WM's `initial = ultimate` DEF
  // action (NewClassification.c:314).  Forces input axioms to the heap
  // front regardless of weight, so the first selections track WM's
  // axiom-first trajectory.  Independent of the WALDMEISTER preset --
  // set it alone, or stack on top of the preset.
  {
    const char *iu = getenv("THVM_ATP_INITIAL_ULTIMATE");
    if (iu != NULL && iu[0] != '\0' && iu[0] != '0') {
      thvm_atp_set_use_initial_ultimate(s, 1u);
    }
  }
  // THVM_ATP_INCR_IR=1: incremental CP-set interreduce.  Skips per-CP
  // normalize when no rule added since last IR pass has a top symbol
  // in the CP.  Cuts wolfram interreduce hotspot from 56% to <10%.
  {
    const char *ii = getenv("THVM_ATP_INCR_IR");
    if (ii != NULL && ii[0] != '\0' && ii[0] != '0') {
      thvm_atp_set_use_incr_ir(s, 1u);
    }
  }
  // THVM_ATP_CP_INDEX=0: disable the cp-gen overlap-partner index (it is ON
  // by default in the WM preset).  The index is a sound, parity-identical
  // candidate filter -- it prunes well on multi-symbol theories but is a
  // near-no-op on single-operator theories (Sheffer/OrAssoc) where every
  // rule LHS shares the binary head.  This override allows A/B measurement
  // and a safety-off.  An explicit "1" force-enables it independent of the
  // preset.
  {
    const char *ci = getenv("THVM_ATP_CP_INDEX");
    if (ci != NULL && ci[0] != '\0') {
      thvm_atp_set_use_cp_index(s, (ci[0] != '0') ? 1u : 0u);
    }
  }
  // THVM_ATP_DATABASE_ULTIMATE=1: port of WM's `database = ultimate`
  // (Parameter.c:166 -- the other half of the WM default classification
  // alongside initial=ultimate).  Tags every CP derived from rule-
  // database overlap as ultimate so newly-derived chains finish before
  // older axiom-CPs are revisited.  This depth-first bias is what lets
  // WM crack wolfram commutativity in 2.5s where breadth-first thvm
  // runs 90s+.
  {
    const char *du = getenv("THVM_ATP_DATABASE_ULTIMATE");
    if (du != NULL && du[0] != '\0' && du[0] != '0') {
      thvm_atp_set_use_database_ultimate(s, 1u);
    }
  }
  // Apply THVM_ATP_CP_WEIGHT after the WALDMEISTER preset so an
  // experiment-time env override (e.g. `THVM_ATP_CP_WEIGHT=8`) wins over
  // the preset's CH_MaxWeight default.
  if (env_cp_weight != NULL && *env_cp_weight != 0) {
    thvm_atp_set_cp_weight_mode(s, (u32)strtoul(env_cp_weight, NULL, 10));
  }
  // THVM_ATP_RHS_IR=0/1: independent override of the WM RHS-
  // interreduction mode (RMRechtsInterred "modify rule": full R+E
  // in-place RHS normalize for oriented rules + GMInterred RHS-face
  // drop for unorientable equations).  0 = legacy slice-only compose.
  {
    const char *ri = getenv("THVM_ATP_RHS_IR");
    if (ri != NULL && ri[0] != '\0')
      thvm_atp_set_use_rhs_interreduce(s, (ri[0] != '0') ? 1u : 0u);
  }

  // THVM_ATP_QUEUE_SUBSUME=0/1: independent override of the push-time
  // queue-vs-queue subsumption filter (default ON engine-wide, OFF in
  // the WALDMEISTER preset above -- no WM counterpart).
  {
    const char *qs = getenv("THVM_ATP_QUEUE_SUBSUME");
    if (qs != NULL && qs[0] != '\0')
      thvm_atp_set_use_queue_subsume(s, (qs[0] != '0') ? 1u : 0u);
  }

  // THVM_ATP_EMISSION_ORDER=0/1: independent override of the WM
  // CP-emission-order mirror (default OFF engine-wide, ON in the
  // WALDMEISTER preset above).
  {
    const char *eo = getenv("THVM_ATP_EMISSION_ORDER");
    if (eo != NULL && eo[0] != '\0')
      thvm_atp_set_use_emission_order(s, (eo[0] != '0') ? 1u : 0u);
  }

  // THVM_ATP_INTAKE_ORDER=0/1: independent override of the WM
  // loader-level axiom canonicalization + initial=ultimate intake
  // (default OFF engine-wide, ON in the WALDMEISTER preset above).
  {
    const char *io = getenv("THVM_ATP_INTAKE_ORDER");
    if (io != NULL && io[0] != '\0')
      thvm_atp_set_use_intake_order(s, (io[0] != '0') ? 1u : 0u);
  }

  // THVM_ATP_MIXMOST_NF=0/1: independent override of the WM mixmost
  // normal-form strategy (default OFF engine-wide, ON in the
  // WALDMEISTER preset above).
  {
    const char *mx = getenv("THVM_ATP_MIXMOST_NF");
    if (mx != NULL && mx[0] != '\0')
      thvm_atp_set_use_mixmost_nf(s, (mx[0] != '0') ? 1u : 0u);
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

  // THVM_ATP_EAGER_SWEEP=1 re-enables the eager interreduce-time orphan
  // sweep (compile default outside the WM preset; the preset turns it
  // OFF to match WM's lazy-only layout) so a run can A/B the layouts.
  {
    const char *es = getenv("THVM_ATP_EAGER_SWEEP");
    if (es != NULL && es[0] != '\0')
      thvm_atp_set_use_eager_orphan_sweep(s, (es[0] != '0') ? 1u : 0u);
  }

  // THVM_ATP_IMPLICIT_CP=1 toggles the deferred-CP (`implicit_pair`)
  // storage path independently of the WM preset: rule-x-rule CPs queue
  // as trace-backed descriptors and materialize at selection.  The
  // trajectory can differ from the eager default (no queue-vs-queue
  // subsumption on the implicit passive set; pop-normalize starts from
  // the raw overlap), so compare proof validity + steps, not bytes.
  {
    const char *ic = getenv("THVM_ATP_IMPLICIT_CP");
    if (ic != NULL && ic[0] != '\0' && ic[0] != '0') {
      thvm_atp_set_use_implicit_cp(s, 1u);
    }
  }

  // Independent A/B toggles for Vampire-style subsumption /
  // demodulation.  All three are OFF by default (no WM analog; the
  // WALDMEISTER preset does not set them) -- opt in for differential
  // debugging or Vampire-flavored runs.
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
  // THVM_ATP_WM_DEMOTE=0/1: independent toggle for the WM-faithful
  // IR-victim demotion (in the WALDMEISTER preset by default).
  {
    const char *wd = getenv("THVM_ATP_WM_DEMOTE");
    if (wd != NULL && wd[0] != '\0')
      thvm_atp_set_use_wm_demote(s, (wd[0] != '0') ? 1u : 0u);
  }
  // THVM_ATP_POP_SUBSUME=0/1: independent toggle for the WM -ks "s"
  // pop-time E-subsumption drop (in the WALDMEISTER preset by default).
  {
    const char *ps = getenv("THVM_ATP_POP_SUBSUME");
    if (ps != NULL && ps[0] != '\0')
      thvm_atp_set_use_pop_subsume(s, (ps[0] != '0') ? 1u : 0u);
  }
  // THVM_ATP_ESET_SUBSUME=0/1: independent toggle for the WM E-set
  // subsumption destroy on new-equation entry (in the WALDMEISTER
  // preset by default).
  {
    const char *es = getenv("THVM_ATP_ESET_SUBSUME");
    if (es != NULL && es[0] != '\0')
      thvm_atp_set_use_eset_subsume(s, (es[0] != '0') ? 1u : 0u);
  }
  // THVM_ATP_BWD_GROUND_JOIN=0/1: WM backward ground-joinability
  // sterilization (-gj, RueckwaertsGrundzusammenfuehrbarkeit).  NOT in
  // the WALDMEISTER preset: WM's -gj defaults OFF (RUN/Parameter.c:317)
  // and no strategy table enables it, so the faithful preset leaves it
  // off too.  thvm_atp_init already parsed the env; this re-set keeps
  // the toggle override-able after preset blocks like its siblings.
  {
    const char *bg = getenv("THVM_ATP_BWD_GROUND_JOIN");
    if (bg != NULL && bg[0] != '\0')
      thvm_atp_set_use_bwd_ground_join(s, (bg[0] != '0') ? 1u : 0u);
  }
  // THVM_ATP_EINSSTERN=0/1: WM -einsstern CP filter (EinsSternUeber-
  // lappung, Unifikation1.c:1039-1055) -- keep only CPs whose overlap
  // position lies on the "1*" leftmost-argument spine.  NOT in the
  // WALDMEISTER preset: WM's -einsstern defaults OFF (RUN/Parameter.c:
  // 250).  Live CP-gen gate; ON changes the trajectory.
  {
    const char *es = getenv("THVM_ATP_EINSSTERN");
    if (es != NULL && es[0] != '\0')
      thvm_atp_set_use_einsstern(s, (es[0] != '0') ? 1u : 0u);
  }
  // THVM_ATP_NO_OVERLAP_BELOW_SKOLEM=0/1: WM -nusfu CP filter
  // (NusfUeberlappung, Unifikation1.c:1082-1090) -- skip overlap
  // positions inside a skolem-function subterm.  Inert on the ground-
  // goal corpus (no skolem symbols), so a no-op even when on; NOT in
  // the WALDMEISTER preset.
  {
    const char *nu = getenv("THVM_ATP_NO_OVERLAP_BELOW_SKOLEM");
    if (nu != NULL && nu[0] != '\0')
      thvm_atp_set_use_no_overlap_below_skolem(s, (nu[0] != '0') ? 1u : 0u);
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

  if (pr_mode) {
    if (prp.n_eqns == 0u) pr_load_terms(&prp);
    for (u32 e = 0; e < prp.n_eqns; e++) {
      thvm_atp_add_equation(s, prp.eq_l[e], prp.eq_r[e]);
    }
    // WM ACSymboleSuchen (ANA/PhilMarlow.c:111): a symbol is AC iff the
    // axiom set contains BOTH an associativity and a commutativity
    // axiom for it.  WM marks those symbols (SO_SymbolInfo.IstAC) and
    // its dokgP/Permsub CP filter (default-ON) discards every AC-equal
    // CP via GZ_ACVerzichtbar.  thvm's auto-AC derives the same mask;
    // feeding it the parsed axioms (the engine's s->lhs[] hold
    // post-orientation rules) sets g_atp_ac_info + the perm_subsume
    // mask so atp_cp_perm_subsumed runs the flattened AC test.  Gated
    // on the WALDMEISTER preset (WM's default behavior); env-overridable.
    {
      const char *wm_ac = getenv("THVM_ATP_WALDMEISTER");
      const char *no_ac = getenv("THVM_ATP_PERM_SUB");   // "0" opts out
      u8 wm_on = (wm_ac != NULL && wm_ac[0] != '\0' && wm_ac[0] != '0');
      u8 ps_off = (no_ac != NULL && no_ac[0] == '0');
      if (wm_on && !ps_off) {
        // auto_ac populates the engine AC mask; capture it for the
        // perm-subsume filter, then RESET the engine mask to 0.  WM
        // runs plain KB completion (no AC-unification/AC-matching) and
        // only uses the AC operators to filter CPs via GZ_ACVerzichtbar
        // -- leaving the engine ac_mask set would activate thvm's full
        // AC-saturation path and diverge (explode) the trajectory.
        thvm_atp_auto_ac(prp.eq_l, prp.eq_r, prp.n_eqns);
        u64 acm = thvm_atp_get_ac_mask();
        thvm_atp_set_ac_mask(0ull);
        if (acm != 0ull) {
          thvm_atp_set_perm_subsume_mask(acm);
          thvm_atp_set_use_perm_subsume(s, 1u);
        }
      }
    }
  } else if (strcmp(goal, "mccune") == 0) {
    thvm_atp_add_equation(s, mccune_axiom_lhs(), fv(3));
  } else if (strcmp(goal, "robbins") == 0) {
    Term r_l1, r_r1, r_l2, r_r2, r_l3, r_r3;
    robbins_axioms(&r_l1, &r_r1, &r_l2, &r_r2, &r_l3, &r_r3);
    thvm_atp_add_equation(s, r_l1, r_r1);
    thvm_atp_add_equation(s, r_l2, r_r2);
    thvm_atp_add_equation(s, r_l3, r_r3);
  } else if (strcmp(goal, "agioc") == 0) {
    Term a1l,a1r,a2l,a2r,a3l,a3r,a4l,a4r;
    aboig_axioms(&a1l,&a1r, &a2l,&a2r, &a3l,&a3r, &a4l,&a4r);
    thvm_atp_add_equation(s, a1l, a1r);
    thvm_atp_add_equation(s, a2l, a2r);
    thvm_atp_add_equation(s, a3l, a3r);
    thvm_atp_add_equation(s, a4l, a4r);
  } else if (strcmp(goal, "glid") == 0) {
    Term g1l,g1r,g2l,g2r,g3l,g3r;
    group_axioms(&g1l,&g1r, &g2l,&g2r, &g3l,&g3r);
    thvm_atp_add_equation(s, g1l, g1r);
    thvm_atp_add_equation(s, g2l, g2r);
    thvm_atp_add_equation(s, g3l, g3r);
  } else if (strcmp(goal, "amc_assoc") == 0) {
    Term al, ar;
    amc_axiom(&al, &ar);
    thvm_atp_add_equation(s, al, ar);
  } else if (strcmp(goal, "mc_ioi") == 0 ||
             strcmp(goal, "mc_assoc") == 0) {
    /* Reuse the existing McCune axiom from mccune_axiom_lhs.  Both
       mc_ioi (not(not(p))=p) and mc_assoc (3-skolem associativity)
       share the McCune single-equation characterization. */
    thvm_atp_add_equation(s, mccune_axiom_lhs(), fv(3));
  } else if (strcmp(goal, "hn_leftid") == 0) {
    /* HigmanNeumann single-axiom group; axiom rhs is x2 = fv(1). */
    thvm_atp_add_equation(s, hn_axiom_lhs(), fv(1));
  } else {
    thvm_atp_add_equation(s, axiom_lhs(), fv(2));
  }

  // "sat" mode: pure completion, NO goal -- run to the step/wall cap
  // and watch the CP queue grow.  This is the explosion probe (a deep
  // goal that never closes would do the same, but `sat` is unambiguous
  // about what is being measured).
  int saturate = (strcmp(goal, "sat") == 0);

  Term gl = 0, gr = 0;
  if (pr_mode) { gl = prp.goal_l; gr = prp.goal_r; }
  else if (strcmp(goal, "cpl1")   == 0) goal_cpl1(&gl, &gr);
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
  else if (strcmp(goal, "agioc") == 0)   goal_aboig_ioc(&gl, &gr);
  else if (strcmp(goal, "glid") == 0)    goal_group_leftid(&gl, &gr);
  else if (strcmp(goal, "amc_assoc") == 0) goal_amc_assoc(&gl, &gr);
  else if (strcmp(goal, "mc_ioi") == 0)  goal_mc_ioi(&gl, &gr);
  else if (strcmp(goal, "mc_assoc") == 0) goal_mc_assoc(&gl, &gr);
  else if (strcmp(goal, "hn_leftid") == 0) goal_hn_leftid(&gl, &gr);
  else if (!saturate)                   goal_thm(&gl, &gr);
  if (!saturate) thvm_atp_set_goal(s, gl, gr);

  // THVM_ATP_MNF=1 activates the MNF goal-directed (two-coloured front)
  // search -- the only path that closes a symmetric goal like wolfram
  // commutativity.  Requires the binary be built with `make ATP_MNF=1`
  // (otherwise the setter is a no-op).
  {
    const char *mnf = getenv("THVM_ATP_MNF");
    if (mnf != NULL && *mnf == '1') thvm_atp_set_use_mnf(s, 1u);
    else if (mnf != NULL && *mnf == '0') thvm_atp_set_use_mnf(s, 0u);
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
      // Optional slope override: the auto bound is base + slope*deepest;
      // the default slope 2 grows past every CP on deepening theorems
      // (Wolfram) so it never binds.  A smaller slope makes it actually
      // stash heavy CPs (bounding memory) while still growing to re-admit
      // them (completeness preserved).
      const char *slp = getenv("THVM_ATP_MAXW_SLOPE");
      if (slp != NULL && *slp != 0) {
        s->auto_max_cp_weight_slope = (u32)strtoul(slp, NULL, 10);
      }
    }
  }
  // Optional HARD CP-weight cap (Waldmeister -mw): CPs whose max side size
  // exceeds the cap are stashed off the main queue (bounding memory) and
  // re-admitted lazily, so completeness is preserved.  THVM_ATP_MAXW=<cap>.
  {
    const char *mw = getenv("THVM_ATP_MAXW");
    if (mw != NULL && *mw != 0) {
      thvm_atp_set_max_cp_weight(s, (u32)strtoul(mw, NULL, 10));
    }
  }

  printf("=== Wolfram-axiom completion : goal=%s ===\n", goal);
  printf("ordering=%s  step_cap=%u  wall_cap=%.0fs  cp_weight_mode=%u\n",
         use_lpo ? "lpo" : (use_kbo0 ? "kbo0" : "kbo"),
         step_cap, wall_cap, (u32)s->cp_weight_mode);
  if (pr_mode) {
    // Label table for trace consumers (tools/wm_align_sweep/align.py
    // maps CPSEL's C<label> back to the .pr symbol names with these).
    for (u32 i = 0; i < prp.n_syms; i++) {
      u32 lab = i + PR_LABEL_BASE;
      printf("PRSYM %u %s %u w=%u prec=%u\n", lab, prp.syms[i].name,
             prp.syms[i].arity, prp.weights[lab], prp.precedence[lab]);
    }
  }
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
    if (pr != NULL && (pr[0] == '1' || pr[0] == '2')) g_atp_phase_enabled = 1u;
    // Tier 2: also split the push-normalize phase into its components
    // (mirror-ensure / from-term / normalize / eq / to-term / ac /
    // join-cache hash / raw-size gate).  Implies tier 1.
    if (pr != NULL && pr[0] == '2') g_atp_phase_detail = 1u;
  }

  // Reset CP-cap-hit counters so the per-run totals attribute lost CPs
  // to a specific cap (ATP_CP_BATCH per-pair vs CP_MAX_DEPTH per-position).
  thvm_cp_caps_reset();
  thvm_lpo_flat_stats_reset();

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
  // Post-PROVED proof-cone re-derivation (same call the WL wrapper
  // makes in thvm_wl_atp_run_proof): splice NORM_STEP chains behind
  // goal-family ORIENT / SIMPLIFY bridges.  On a t_max-capped trace it
  // no-ops after the scan (no headroom for chains).
  if (st == ATP_PROVED) {
    clock_t tc0 = clock();
    thvm_atp_record_goal_cone(s);
    printf("   cone: %.2fs  n_trace=%u\n",
           (double)(clock() - tc0) / CLOCKS_PER_SEC, s->n_trace);
  }
  // Mirror the WL bridge (thvmlink_atp.c): cone on the packed trace,
  // THEN materialize, so every later trace consumer (proof extraction,
  // dump modes) sees live Term entries.  No-op unless trace_pack.
  thvm_atp_materialize_trace(s);
  printf("   dropped: joinable=%u queue-subsumed=%u "
         "rule-subsumed=%u pop-subsumed=%u perm-subsumed=%u eset-subsumed=%u "
         "connected=%u orphan=%u lrs=%u\n",
         s->n_cps_dropped_joinable, s->n_cps_dropped_queue_subsumed,
         s->n_cps_dropped_rule_subsumed, s->n_cps_dropped_pop_subsumed,
         s->n_cps_dropped_perm_subsumed,
         s->n_eqs_dropped_eset_subsumed,
         s->n_cps_dropped_connected,
         s->n_cps_dropped_orphan, s->n_cps_dropped_lrs);
  if (s->use_lrs) {
    printf("   lrs: recomputes=%u  horizon=%u  warmup=%u  period=%u\n",
           s->n_lrs_recomputes, s->lrs_horizon,
           s->lrs_warmup_selections, s->lrs_recompute_period);
  }
  printf("   dropped: ground-joinable=%u connected-below-peak=%u "
         "facts-gj-sterilized=%u\n",
         s->n_cps_ground_joinable, s->n_cps_dropped_connected_below_peak,
         s->n_facts_bwd_ground_joinable);
  {
    u64 cp_capped = 0, depth_capped = 0;
    thvm_cp_caps_get(&cp_capped, &depth_capped);
    printf("   caps: cp_batch_hits=%llu  depth_capped_positions=%llu  "
           "(CP_MAX_DEPTH=%u)\n",
           (unsigned long long)cp_capped,
           (unsigned long long)depth_capped,
           (u32)CP_MAX_DEPTH);
  }
  {
    u64 lrec = 0, lhits = 0, lcomp = 0, ltop = 0;
    thvm_lpo_flat_stats(&lrec, &lhits, &lcomp, &ltop);
    double hr = (lrec > 0) ? (100.0 * (double)lhits / (double)lrec) : 0.0;
    double rec_per_top = (ltop > 0) ? ((double)lrec / (double)ltop) : 0.0;
    double comp_per_top = (ltop > 0) ? ((double)lcomp / (double)ltop) : 0.0;
    printf("   lpo: top=%llu  rec=%llu  compute=%llu  memo-hit=%.1f%%  "
           "(rec/top=%.1f compute/top=%.1f)\n",
           (unsigned long long)ltop, (unsigned long long)lrec,
           (unsigned long long)lcomp, hr, rec_per_top, comp_per_top);
  }
  printf("   right-reduced (RHS composed) rules: %u\n", s->n_right_reduced);
  printf("   lazy-normalize=%u  push-time full-R normalizes=%llu\n",
         s->use_lazy_normalize,
         (unsigned long long)s->n_cps_push_normalized);
  if (g_atp_phase_enabled) {
    double w = (el > 0.0) ? el : 1.0;
    u64 sumus = g_atp_phase_us_pop_normalize + g_atp_phase_us_cp_gen +
                g_atp_phase_us_push_normalize + g_atp_phase_us_interreduce +
                g_atp_phase_us_goal_check + g_atp_phase_us_cp_set_ir;
    double push_norm_us_per_cp = (s->n_cps_push_normalized > 0)
        ? (double)g_atp_phase_us_push_normalize /
          (double)s->n_cps_push_normalized
        : 0.0;
    printf("   phase: pop-norm=%.2fs (%.0f%%) cp-gen=%.2fs (%.0f%%)\n"
           "          push-norm=%.2fs (%.0f%%) [%.1fus/cp]"
           " interreduce=%.2fs (%.0f%%)\n"
           "          goal-check=%.2fs (%.0f%%) cp-set-ir=%.2fs (%.0f%%)\n"
           "          cp-set-ir-detail: unpack=%.2fs norm=%.2fs pack=%.2fs post=%.2fs\n"
           "          sum=%.2fs / wall=%.2fs (%.0f%%)\n",
           g_atp_phase_us_pop_normalize / 1e6,
           100.0 * (g_atp_phase_us_pop_normalize / 1e6) / w,
           g_atp_phase_us_cp_gen / 1e6,
           100.0 * (g_atp_phase_us_cp_gen / 1e6) / w,
           g_atp_phase_us_push_normalize / 1e6,
           100.0 * (g_atp_phase_us_push_normalize / 1e6) / w,
           push_norm_us_per_cp,
           g_atp_phase_us_interreduce / 1e6,
           100.0 * (g_atp_phase_us_interreduce / 1e6) / w,
           g_atp_phase_us_goal_check / 1e6,
           100.0 * (g_atp_phase_us_goal_check / 1e6) / w,
           g_atp_phase_us_cp_set_ir / 1e6,
           100.0 * (g_atp_phase_us_cp_set_ir / 1e6) / w,
           g_atp_phase_us_cpir_unpack / 1e6,
           g_atp_phase_us_cpir_normalize / 1e6,
           g_atp_phase_us_cpir_pack / 1e6,
           g_atp_phase_us_cpir_post / 1e6,
           sumus / 1e6, w,
           100.0 * (sumus / 1e6) / w);
    // The heap-pressure GC fires inside thvm_atp_generate_cps_wm, so its
    // wall is buried in the cp-gen tile above; report it explicitly.
    printf("   gc: collections=%llu total=%.2fs (%.0f%%; inside cp-gen tile)\n",
           (unsigned long long)g_atp_gc_n, g_atp_gc_us / 1e6,
           100.0 * (g_atp_gc_us / 1e6) / w);
    if (g_atp_phase_detail) {
      u64 sumd = g_atp_pushn_us_mirror + g_atp_pushn_us_from +
                 g_atp_pushn_us_norm + g_atp_pushn_us_eq +
                 g_atp_pushn_us_to + g_atp_pushn_us_ac +
                 g_atp_pushn_us_hash + g_atp_pushn_us_gate;
      double per = (g_atp_pushn_calls > 0)
          ? (double)sumd / (double)g_atp_pushn_calls : 0.0;
      printf("   push-norm detail: calls=%llu joined=%llu [%.2fus/call]\n"
             "     mirror=%.2fs from-term=%.2fs norm=%.2fs eq=%.2fs to-term=%.2fs\n"
             "     ac=%.2fs hash=%.2fs size-gate=%.2fs  sum=%.2fs / push-norm=%.2fs\n",
             (unsigned long long)g_atp_pushn_calls,
             (unsigned long long)g_atp_pushn_joined, per,
             g_atp_pushn_us_mirror / 1e6, g_atp_pushn_us_from / 1e6,
             g_atp_pushn_us_norm / 1e6, g_atp_pushn_us_eq / 1e6,
             g_atp_pushn_us_to / 1e6, g_atp_pushn_us_ac / 1e6,
             g_atp_pushn_us_hash / 1e6, g_atp_pushn_us_gate / 1e6,
             sumd / 1e6, g_atp_phase_us_push_normalize / 1e6);
      {
        extern u64 g_acp_pack_nodes_raw, g_acp_pack_nodes_treated,
                   g_acp_pack_n_raw, g_acp_pack_n_treated;
        u64 tot = g_acp_pack_nodes_raw + g_acp_pack_nodes_treated;
        printf("   pack class split: raw>=50 n=%llu nodes=%llu (%.0f%%) | "
               "treated n=%llu nodes=%llu (%.0f%%)\n",
               (unsigned long long)g_acp_pack_n_raw,
               (unsigned long long)g_acp_pack_nodes_raw,
               tot ? 100.0 * (double)g_acp_pack_nodes_raw / (double)tot : 0.0,
               (unsigned long long)g_acp_pack_n_treated,
               (unsigned long long)g_acp_pack_nodes_treated,
               tot ? 100.0 * (double)g_acp_pack_nodes_treated / (double)tot
                   : 0.0);
      }
      {
        extern u64 g_vcp_emit_n_ok, g_vcp_emit_n_fb;
        extern u64 g_vcp_treat_n_ok, g_vcp_treat_n_fb, g_vcp_treat_n_pkfb;
        extern u64 g_vcp_gate_n_test, g_vcp_gate_n_drop, g_vcp_gate_n_fb;
        if (g_vcp_emit_n_ok + g_vcp_emit_n_fb +
            g_vcp_treat_n_ok + g_vcp_treat_n_fb > 0) {
          printf("   ft-emit: raw ok=%llu fb=%llu | treat ok=%llu fb=%llu "
                 "pkfb=%llu | gate test=%llu drop=%llu fb=%llu\n",
                 (unsigned long long)g_vcp_emit_n_ok,
                 (unsigned long long)g_vcp_emit_n_fb,
                 (unsigned long long)g_vcp_treat_n_ok,
                 (unsigned long long)g_vcp_treat_n_fb,
                 (unsigned long long)g_vcp_treat_n_pkfb,
                 (unsigned long long)g_vcp_gate_n_test,
                 (unsigned long long)g_vcp_gate_n_drop,
                 (unsigned long long)g_vcp_gate_n_fb);
        }
        extern u64 g_vcp_lb_hit, g_vcp_lb_raw;
        if (g_vcp_lb_raw > 0) {
          printf("   ft-emit lb probe: hit=%llu of raw=%llu (%.1f%%)\n",
                 (unsigned long long)g_vcp_lb_hit,
                 (unsigned long long)g_vcp_lb_raw,
                 100.0 * (double)g_vcp_lb_hit / (double)g_vcp_lb_raw);
        }
        // later-25 per-CP walk-count attribution: node-visits and walk
        // invocations for the three composite traversals CP formation runs.
        extern u64 g_vcp_cnt_calls, g_vcp_cnt_nodes;
        extern u64 g_vcp_emt_calls, g_vcp_emt_nodes;
        extern u64 g_vcp_bem_calls, g_vcp_bem_nodes;
        if (g_vcp_cnt_calls + g_vcp_emt_calls + g_vcp_bem_calls > 0) {
          printf("   ft-emit walks: count calls=%llu nodes=%llu (%.1f/call) | "
                 "raw-emit calls=%llu nodes=%llu (%.1f/call) | "
                 "treat-bemit calls=%llu nodes=%llu (%.1f/call)\n",
                 (unsigned long long)g_vcp_cnt_calls,
                 (unsigned long long)g_vcp_cnt_nodes,
                 g_vcp_cnt_calls ? (double)g_vcp_cnt_nodes / (double)g_vcp_cnt_calls : 0.0,
                 (unsigned long long)g_vcp_emt_calls,
                 (unsigned long long)g_vcp_emt_nodes,
                 g_vcp_emt_calls ? (double)g_vcp_emt_nodes / (double)g_vcp_emt_calls : 0.0,
                 (unsigned long long)g_vcp_bem_calls,
                 (unsigned long long)g_vcp_bem_nodes,
                 g_vcp_bem_calls ? (double)g_vcp_bem_nodes / (double)g_vcp_bem_calls : 0.0);
        }
        // later-25 heap-Term interchange volume (dual-rep round-trips).
        extern u64 g_ft_from_calls, g_ft_from_nodes;
        extern u64 g_ft_to_calls, g_ft_to_nodes;
        extern u64 g_acp_unterm_nodes;
        if (g_ft_from_calls + g_ft_to_calls + g_acp_unterm_nodes > 0) {
          printf("   interchange: ft_from_term calls=%llu nodes=%llu (%.1f/call) | "
                 "ft_to_term calls=%llu nodes=%llu (%.1f/call) | "
                 "acp_unpack_term nodes=%llu\n",
                 (unsigned long long)g_ft_from_calls,
                 (unsigned long long)g_ft_from_nodes,
                 g_ft_from_calls ? (double)g_ft_from_nodes / (double)g_ft_from_calls : 0.0,
                 (unsigned long long)g_ft_to_calls,
                 (unsigned long long)g_ft_to_nodes,
                 g_ft_to_calls ? (double)g_ft_to_nodes / (double)g_ft_to_calls : 0.0,
                 (unsigned long long)g_acp_unterm_nodes);
        }
      }
      printf("   push-norm core shape: queries=%llu (var=%llu) cand=%llu "
             "att=%llu hit=%llu\n"
             "     pb=%llu win=%llu rematch=%llu splices=%llu "
             "[cand/q=%.2f att/q=%.2f hit/att=%.3f]\n"
             "     dt-nodes=%llu dt-branch-edges=%llu [node/q=%.2f]\n",
             (unsigned long long)g_atp_pushn_tr_q,
             (unsigned long long)g_atp_pushn_tr_var,
             (unsigned long long)g_atp_pushn_tr_cand,
             (unsigned long long)g_atp_pushn_tr_att,
             (unsigned long long)g_atp_pushn_tr_hit,
             (unsigned long long)g_atp_pushn_tr_pb,
             (unsigned long long)g_atp_pushn_tr_win,
             (unsigned long long)g_atp_pushn_tr_rematch,
             (unsigned long long)g_atp_pushn_rw,
             g_atp_pushn_tr_q ? (double)g_atp_pushn_tr_cand /
                                (double)g_atp_pushn_tr_q : 0.0,
             g_atp_pushn_tr_q ? (double)g_atp_pushn_tr_att /
                                (double)g_atp_pushn_tr_q : 0.0,
             g_atp_pushn_tr_att ? (double)g_atp_pushn_tr_hit /
                                  (double)g_atp_pushn_tr_att : 0.0,
             (unsigned long long)g_atp_pushn_dt_node,
             (unsigned long long)g_atp_pushn_dt_edge,
             g_atp_pushn_tr_q ? (double)g_atp_pushn_dt_node /
                                (double)g_atp_pushn_tr_q : 0.0);
      {
        // PHASE-0 allocs-per-rewrite (push scope): gross persistent
        // cells allocated / freed by norm-core splices, split into
        // regime-c (allocating) vs regime-a (in-place) rewrites.  Native
        // WM rewrites the flatterm in place at ~0 alloc/rewrite; this is
        // the direct thvm-vs-native ALLOCATION-axis measurement.
        extern u64 g_atp_pushn_alloc_cells, g_atp_pushn_free_cells;
        extern u64 g_atp_pushn_splice_c, g_atp_pushn_splice_a;
        u64 spl = g_atp_pushn_splice_c + g_atp_pushn_splice_a;
        printf("   push-norm alloc: splices=%llu (regC=%llu regA=%llu) "
               "alloc-cells=%llu free-cells=%llu\n"
               "     [alloc/rw=%.3f free/rw=%.3f regC-frac=%.3f]\n",
               (unsigned long long)spl,
               (unsigned long long)g_atp_pushn_splice_c,
               (unsigned long long)g_atp_pushn_splice_a,
               (unsigned long long)g_atp_pushn_alloc_cells,
               (unsigned long long)g_atp_pushn_free_cells,
               spl ? (double)g_atp_pushn_alloc_cells / (double)spl : 0.0,
               spl ? (double)g_atp_pushn_free_cells / (double)spl : 0.0,
               spl ? (double)g_atp_pushn_splice_c / (double)spl : 0.0);
      }
      if (g_atp_pushn_dt_node > 0) {
        // Deep descend split + per-depth visit histogram + mixmost
        // re-query provenance (see the g_atp_pushn_dt_* block in _.c).
        printf("     dt-split: chain=%llu branch=%llu prune=%llu emit=%llu\n"
               "       bind1=%llu fteq=%llu fteq-ok=%llu fteq-cells=%llu "
               "cprobe=%llu\n",
               (unsigned long long)g_atp_pushn_dt_chain,
               (unsigned long long)g_atp_pushn_dt_branch,
               (unsigned long long)g_atp_pushn_dt_prune,
               (unsigned long long)g_atp_pushn_dt_emit,
               (unsigned long long)g_atp_pushn_dt_bind1,
               (unsigned long long)g_atp_pushn_dt_fteq,
               (unsigned long long)g_atp_pushn_dt_fteq_ok,
               (unsigned long long)g_atp_pushn_dt_fteq_cell,
               (unsigned long long)g_atp_pushn_dt_cprobe);
        printf("     dt-depth-visits:");
        for (int hd = 0; hd < ATP_DT_HIST_N; hd++) {
          if (g_atp_pushn_dt_hist[hd] == 0) continue;
          printf(" %d:%llu", hd,
                 (unsigned long long)g_atp_pushn_dt_hist[hd]);
        }
        printf("\n");
        printf("     mm-requery: fix=%llu asc=%llu (of q=%llu)\n",
               (unsigned long long)g_atp_pushn_mm_fix,
               (unsigned long long)g_atp_pushn_mm_asc,
               (unsigned long long)g_atp_pushn_tr_q);
        // Norm-core waste attribution (see the g_atp_pushn_dt_fteq_*
        // block in _.c): repeat-compare decision tiers + walk-site
        // split, var/concrete edge-iteration split, branch fan-out.
        u64 fulleq = g_atp_pushn_dt_fteq - g_atp_pushn_dt_fteq_symfail;
        printf("     dt-fteq-split: chain=%llu branch=%llu | "
               "symfail=%llu (%.1f%%) fulleq=%llu (ok=%llu fail=%llu)\n",
               (unsigned long long)g_atp_pushn_dt_fteq_chain,
               (unsigned long long)(g_atp_pushn_dt_fteq -
                                    g_atp_pushn_dt_fteq_chain),
               (unsigned long long)g_atp_pushn_dt_fteq_symfail,
               g_atp_pushn_dt_fteq
                   ? 100.0 * (double)g_atp_pushn_dt_fteq_symfail /
                         (double)g_atp_pushn_dt_fteq
                   : 0.0,
               (unsigned long long)fulleq,
               (unsigned long long)g_atp_pushn_dt_fteq_ok,
               (unsigned long long)(fulleq - g_atp_pushn_dt_fteq_ok));
        printf("     dt-edge-iters: var=%llu cprobe=%llu "
               "skipped=%llu (of dt-edge=%llu)\n",
               (unsigned long long)g_atp_pushn_dt_vedge,
               (unsigned long long)g_atp_pushn_dt_cprobe,
               (unsigned long long)(g_atp_pushn_dt_edge -
                                    g_atp_pushn_dt_vedge -
                                    g_atp_pushn_dt_cprobe),
               (unsigned long long)g_atp_pushn_dt_edge);
        printf("     dt-branch-fanout (n_edge: visits/edges):");
        for (int fb = 0; fb < ATP_DT_FAN_N; fb++) {
          if (g_atp_pushn_dt_fan_visit[fb] == 0) continue;
          printf(" %d%s:%llu/%llu", fb,
                 (fb == ATP_DT_FAN_N - 1) ? "+" : "",
                 (unsigned long long)g_atp_pushn_dt_fan_visit[fb],
                 (unsigned long long)g_atp_pushn_dt_fan_edge[fb]);
        }
        printf("\n");
        atp_ftdt_hot_dump();
      }
      if (g_atp_ftnfm_probe > 0) {
        printf("   ftnfm subtree-NF-memo: probe=%llu hit=%llu [%.1f%%] "
               "store=%llu toolong=%llu\n",
               (unsigned long long)g_atp_ftnfm_probe,
               (unsigned long long)g_atp_ftnfm_hit,
               100.0 * (double)g_atp_ftnfm_hit / (double)g_atp_ftnfm_probe,
               (unsigned long long)g_atp_ftnfm_store,
               (unsigned long long)g_atp_ftnfm_toolong);
      }
      if (g_atp_swrn_hit + g_atp_swrn_miss > 0) {
        printf("   walk rename cache: hit=%llu miss=%llu [%.1f%%]\n",
               (unsigned long long)g_atp_swrn_hit,
               (unsigned long long)g_atp_swrn_miss,
               100.0 * (double)g_atp_swrn_hit /
                   (double)(g_atp_swrn_hit + g_atp_swrn_miss));
      }
      if (g_atp_wmo_rc_hit + g_atp_wmo_rc_miss > 0) {
        printf("   wmo rank cache: hit=%llu miss=%llu [%.1f%%]\n",
               (unsigned long long)g_atp_wmo_rc_hit,
               (unsigned long long)g_atp_wmo_rc_miss,
               100.0 * (double)g_atp_wmo_rc_hit /
                   (double)(g_atp_wmo_rc_hit + g_atp_wmo_rc_miss));
      }
      // CP-gen non-push split (single-walk former regions).  The
      // top-level components (collect/overlap/rank/pushloop) tile the
      // cp-gen phase up to loop overhead; the pushloop's own split
      // (push-norm + varnorm + filters + trace + pack) tiles the
      // pushloop the same way.  kbo is a sub-slice of pack.
      u64 sumg = g_atp_cpg_us_collect + g_atp_cpg_us_overlap +
                 g_atp_cpg_us_rank + g_atp_cpg_us_pushloop;
      u64 sump = g_atp_phase_us_push_normalize + g_atp_cpg_us_varnorm +
                 g_atp_cpg_us_filters + g_atp_cpg_us_trace +
                 g_atp_cpg_us_pack;
      printf("   cp-gen detail: overlap-calls=%llu rank-calls=%llu\n"
             "     collect=%.2fs overlap=%.2fs rank=%.2fs pushloop=%.2fs\n"
             "     sum=%.2fs / cp-gen=%.2fs\n"
             "   pushloop detail: push-norm=%.2fs varnorm=%.2fs "
             "filters=%.2fs trace=%.2fs pack=%.2fs (kbo=%.2fs)\n"
             "     sum=%.2fs / pushloop=%.2fs\n",
             (unsigned long long)g_atp_cpg_overlap_calls,
             (unsigned long long)g_atp_cpg_rank_calls,
             g_atp_cpg_us_collect / 1e6, g_atp_cpg_us_overlap / 1e6,
             g_atp_cpg_us_rank / 1e6, g_atp_cpg_us_pushloop / 1e6,
             sumg / 1e6, g_atp_phase_us_cp_gen / 1e6,
             g_atp_phase_us_push_normalize / 1e6,
             g_atp_cpg_us_varnorm / 1e6, g_atp_cpg_us_filters / 1e6,
             g_atp_cpg_us_trace / 1e6, g_atp_cpg_us_pack / 1e6,
             g_atp_cpg_us_kbo / 1e6,
             sump / 1e6, g_atp_cpg_us_pushloop / 1e6);
      if (g_acp_fuse_n_ok + g_acp_fuse_n_fb > 0) {
        printf("   fused pack: ok=%llu fallback=%llu\n",
               (unsigned long long)g_acp_fuse_n_ok,
               (unsigned long long)g_acp_fuse_n_fb);
      }
    }
    if (g_atp_unorient_step_calls > 0) {
      printf("   unorient-step: %llu calls  %llu fires  %llu empty (%.0f%% wasted)  %.2fs total\n",
             (unsigned long long)g_atp_unorient_step_calls,
             (unsigned long long)g_atp_unorient_step_fires,
             (unsigned long long)g_atp_unorient_step_empty,
             100.0 * (double)g_atp_unorient_step_empty /
                     (double)g_atp_unorient_step_calls,
             g_atp_unorient_step_us / 1e6);
    }
    if (g_atp_wmcp_full > 0) {
      printf("   wm-cp-cand: %llu candidates / %llu full-scan partners "
             "(%.1f%% visited, prune %.1fx)\n",
             (unsigned long long)g_atp_wmcp_cand,
             (unsigned long long)g_atp_wmcp_full,
             100.0 * (double)g_atp_wmcp_cand / (double)g_atp_wmcp_full,
             (double)g_atp_wmcp_full / (double)(g_atp_wmcp_cand ? g_atp_wmcp_cand : 1));
      printf("   wm-cp-overlap: %llu zero-yield / %llu nonzero-yield pairs "
             "(%.1f%% wasted overlaps)\n",
             (unsigned long long)g_atp_wmcp_zero,
             (unsigned long long)g_atp_wmcp_nonzero,
             100.0 * (double)g_atp_wmcp_zero /
                     (double)((g_atp_wmcp_zero + g_atp_wmcp_nonzero)
                              ? (g_atp_wmcp_zero + g_atp_wmcp_nonzero) : 1));
    }
    {
      u64 unf_total = g_atp_unf_memo_hits + g_atp_unf_memo_misses;
      if (unf_total > 0) {
        printf("   unf-memo: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
               (unsigned long long)g_atp_unf_memo_hits,
               (unsigned long long)unf_total,
               100.0 * (double)g_atp_unf_memo_hits / (double)unf_total);
      }
      u64 si_total = g_atp_ri_splice_inline_hits + g_atp_ri_splice_inline_misses;
      if (si_total > 0) {
        printf("   splice-inline: %llu hits / %llu attempts (%.0f%% hit-ratio)\n",
               (unsigned long long)g_atp_ri_splice_inline_hits,
               (unsigned long long)si_total,
               100.0 * (double)g_atp_ri_splice_inline_hits / (double)si_total);
      }
      u64 pm_total = g_atp_unf_pos_memo_hits + g_atp_unf_pos_memo_misses;
      if (pm_total > 0) {
        printf("   pos-memo: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
               (unsigned long long)g_atp_unf_pos_memo_hits,
               (unsigned long long)pm_total,
               100.0 * (double)g_atp_unf_pos_memo_hits / (double)pm_total);
      }
      u64 rc_total = g_atp_rhs_cache_hits + g_atp_rhs_cache_misses;
      if (rc_total > 0) {
        printf("   rhs-flat-cache: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
               (unsigned long long)g_atp_rhs_cache_hits,
               (unsigned long long)rc_total,
               100.0 * (double)g_atp_rhs_cache_hits / (double)rc_total);
      }
      u64 nc_total = g_atp_norm_cache_hits + g_atp_norm_cache_misses;
      if (nc_total > 0) {
        printf("   norm-cache: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
               (unsigned long long)g_atp_norm_cache_hits,
               (unsigned long long)nc_total,
               100.0 * (double)g_atp_norm_cache_hits / (double)nc_total);
      }
      u64 jc_total = g_atp_join_cache_hits + g_atp_join_cache_misses;
      if (jc_total > 0) {
        printf("   join-cache: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
               (unsigned long long)g_atp_join_cache_hits,
               (unsigned long long)jc_total,
               100.0 * (double)g_atp_join_cache_hits / (double)jc_total);
      }
    }
  }
  { u32 unor = 0;
    for (u32 i = 0; i < s->n_rules; i++) if (!s->r_orient[i]) unor++;
    printf("   unorientable rules: %u / %u\n", unor, s->n_rules); }
  printf("   steps/sec=%.1f  cps/step=%.1f\n",
         (el > 0.0) ? (double)i / el : 0.0,
         (i > 0) ? (double)max_cps / (double)i : 0.0);
  printf("   gc: passes=%llu  live-after-last=%llu cells\n",
         (unsigned long long)gc_count(),
         (unsigned long long)(HEAP_NEXT - gc_from_start()));
  { u32 mn = 0, mx = 0, bins[12];
    double mean = 0.0;
    u32 qlen = thvm_atp_cp_size_stats(s, &mn, &mx, &mean, bins, 12u, 10u);
    printf("   cp-size: live=%u  min=%u  mean=%.1f  max=%u\n",
           qlen, mn, mean, mx);
    printf("   cp-size histogram (bucket=10 symbols):\n     ");
    for (u32 b = 0; b < 12u; b++) printf("[%u-%u)=%u ", b*10u, (b+1u)*10u, bins[b]);
    printf("\n"); }

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
