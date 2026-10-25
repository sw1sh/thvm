// test_atp_deep_cp.c - the engine's critical-pair push and a few
// saturation steps on a deep critical pair (CP), end to end.
//
// G0.1 of docs/ATP/enigma_portfolio.md recorded SIGSEGV rows on
// SYN986-10.005 / .006: a CP thousands of levels deep overflowed the C
// stack in the push-time treatment (atp_cp_trivially_joinable,
// ft_to_term) and in the ground-join index flatten (atp_ri_flatten).
// test_ft_deep and test_atp_deep pin each walk on its own; this test
// drives the path that crashed, atp_push_cps_traced, with the knobs of
// the crashing lanes: tgt-sel (the target-symbol selection weight) and
// bare-gj (ground joining and connectedness).  It runs on an 8 MB stack,
// FindProof's deployment `ulimit -s 8192`, whatever the machine's own
// limit is.  Nothing here is compared with a reference: the test passes
// when the pushes and steps return and the counters say the CP went
// where it should.

#include "../src/thvm.c"

#include "test.h"
#include "deep_stack.h"

#define DEPLOY_STACK_BYTES ((size_t)8u << 20)

enum { L_E = 1, L_F = 2, L_G = 3, L_B = 4 };

static u32       g_cp_weights[32];
static u32       g_cp_prec[32];
static KboConfig g_cp_cfg;

static Term cst(u32 l) {
  return term_new_ctr(l, NULL, 0u);
}

// F^depth(bottom) with F(t) = f(t, b): no rule matches anywhere but at
// the bottom, so a normalization is one pass over the spine.
static Term spine(u32 depth, Term bottom) {
  Term t = bottom;
  for (u32 k = 0; k < depth; k++) {
    Term kids[2] = { t, cst(L_B) };
    t = term_new_ctr(L_F, kids, 2u);
  }
  return t;
}

// R = { f(x, e) -> x, g(g(x)) -> x } over the test signature.
static AtpState *cp_state(u8 bare_gj, u8 tgt) {
  for (u32 i = 0; i < 32u; i++) {
    g_cp_weights[i] = 1u;
    g_cp_prec[i]    = i;
  }
  g_cp_cfg.weights    = g_cp_weights;
  g_cp_cfg.precedence = g_cp_prec;
  g_cp_cfg.n_labels   = 32u;
  g_cp_cfg.var_weight = 1u;
  AtpState *s = thvm_atp_init(&g_cp_cfg, 8u);
  Term x = term_new_fvr(0u);
  Term fk[2] = { x, cst(L_E) };
  (void)atp_push_rule(s, term_new_ctr(L_F, fk, 2u), x);
  Term gx = term_new_ctr(L_G, &x, 1u);
  (void)atp_push_rule(s, term_new_ctr(L_G, &gx, 1u), x);
  s->use_ground_join   = bare_gj;
  s->use_connectedness = bare_gj;
  if (tgt) {
    s->tgt_sym_mask[0] |= 1ull << L_F;
  }
  return s;
}

typedef struct {
  u32 depth;
  u8  bare_gj;
  u8  tgt;
} DeepCpCase;

// One deep CP, F^d(x0) = F^d(g(x0)) with peak F^d(g(g(x0))): the sides
// differ only at the bottom, so it is neither joinable nor
// ground-joinable and is queued.  The same pair with equal sides is
// dropped by one of the push-time gates.  Then a bounded run selects
// among the queued CPs.
static void deep_cp_case(void *p) {
  const DeepCpCase *c = (const DeepCpCase *)p;
  AtpState *s = cp_state(c->bare_gj, c->tgt);
  Term x0 = term_new_fvr(0u);
  Term g1 = term_new_ctr(L_G, &x0, 1u);
  Term g2 = term_new_ctr(L_G, &g1, 1u);
  CriticalPair cp;
  memset(&cp, 0, sizeof cp);
  cp.lhs   = spine(c->depth, x0);
  cp.rhs   = spine(c->depth, g1);
  cp.peak  = spine(c->depth, g2);
  cp.combo = 0xffu;
  CHECK_EQ(atp_push_cps_traced(s, &cp, 1u, 0u, 1u, 0u, 1u), 1u);
  CHECK_EQ(s->n_cps_dropped_joinable, 0u);

  CriticalPair jp = cp;
  jp.rhs = spine(c->depth, x0);
  CHECK_EQ(atp_push_cps_traced(s, &jp, 1u, 0u, 1u, 0u, 1u), 0u);

  AtpStatus st = thvm_atp_run(s);
  CHECK(st != ATP_ABORTED);
  thvm_atp_free(s);
}

int main(void) {
  // Read once by the engine, at first use: set before anything runs.
  setenv("THVM_ATP_TGT_SEL", "1", 1);
  thvm_init();
  // Each case starts on the heap the first one did: the test runs no
  // collector, and eight deep runs in a row exhaust the from-space.
  u64 hc0 = thvm_atp_heap_checkpoint();
  static const u32 depths[2] = { 20000u, DEEP_LEVELS };
  for (u32 d = 0; d < 2u; d++) {
    for (u32 k = 0; k < 4u; k++) {
      DeepCpCase c = { depths[d], (u8)(k & 1u), (u8)(k >> 1) };
      TEST_BEGIN(d == 0u ? "atp_deep_cp/push-20k" : "atp_deep_cp/push-100k");
      thvm_atp_heap_reset(hc0);
      deep_stack_run(DEPLOY_STACK_BYTES, deep_cp_case, &c);
      if (getenv("THVM_DEEP_CP_HEAP") != NULL) {
        fprintf(stderr, "deep_cp case %u/%u: heap %llu cells\n", d, k,
                (unsigned long long)(HEAP_NEXT - hc0));
      }
    }
  }
  thvm_free();
  TEST_REPORT();
}
