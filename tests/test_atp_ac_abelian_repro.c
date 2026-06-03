// test_atp_ac_abelian_repro.c -- C-level reproducer of the WL paclet's
// AbelianGroupAxioms/ImpliesAbelianMcCuneAxioms regression under
// THVM_ATP_AUTO_AC=1.
//
// Default (no AC): proves in 27 steps / ~0.03s.
// AUTO_AC=1: QUEUE_EMPTY in <0.01s without proving.
//
// Encodes the 4 abelian-group axioms + the McCune-Abelian conjunct in
// C, runs the saturator twice (mask=0 / auto_ac), reports rule count +
// status.  Bisects which AC-aware code path drops the inference the
// syntactic path finds.

#include "../src/thvm.c"
#include "test.h"

#define L_OP    1u   // x*y     (binary, comm+assoc -> AC)
#define L_E     2u   // identity (const)
#define L_INV   3u   // inverse (unary)
#define L_A     4u
#define L_B     5u
#define L_C     6u

static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0u); }
static Term bin(u32 lab, Term x, Term y) {
  Term kids[2] = { x, y };
  return term_new_ctr(lab, kids, 2u);
}
static Term un(u32 lab, Term x) {
  return term_new_ctr(lab, &x, 1u);
}
static Term v(u32 id) { return term_new_fvr(id); }

static const char *status_name(AtpStatus st) {
  switch (st) {
    case ATP_RUNNING:     return "RUNNING";
    case ATP_PROVED:      return "PROVED";
    case ATP_REFUTED:     return "REFUTED";
    case ATP_TIMEOUT:     return "TIMEOUT";
    case ATP_QUEUE_EMPTY: return "QUEUE_EMPTY";
    case ATP_ABORTED:     return "ABORTED";
    default:              return "?";
  }
}

static void run_once(u64 ac_mask, const char *label,
                     AtpStatus *st_out, u32 *iters_out, u32 *n_rules_out) {
  Term x = v(0), y = v(1), z = v(2);
  Term e = k(L_E);
  Term a = k(L_A), b = k(L_B), cc = k(L_C);

  // Weights all 1.  Precedence MIRRORS the WL paclet's default
  // identity precedence (prec[i] = i+1), so the regression at the
  // WL bridge is reproduced byte-for-byte at the C level: under
  // this layout op is the LOWEST-precedence symbol and constants
  // / unary functions all rank above it.
  //   L_OP=1  -> prec 2 (lowest non-zero)
  //   L_E=2   -> prec 3
  //   L_INV=3 -> prec 4
  //   L_A=4   -> prec 5
  //   L_B=5   -> prec 6
  //   L_C=6   -> prec 7
  static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
  static const u32 P[8] = { 0u, 2u, 3u, 4u, 5u, 6u, 7u, 0u };
  KboConfig kbo = { .weights = W, .precedence = P,
                    .n_labels = 8, .var_weight = 1u };

  thvm_atp_set_ac_mask(ac_mask);

  AtpState *s = thvm_atp_init(&kbo, 8192);
  // Comm + assoc + identity + inverse for L_OP.
  thvm_atp_add_equation(s, bin(L_OP, x, y), bin(L_OP, y, x));
  thvm_atp_add_equation(s, bin(L_OP, bin(L_OP, x, y), z),
                            bin(L_OP, x, bin(L_OP, y, z)));
  thvm_atp_add_equation(s, bin(L_OP, x, e), x);
  thvm_atp_add_equation(s, bin(L_OP, x, un(L_INV, x)), e);

  // Goal: op(op(op(a, b), c), inv(op(a, b))) = c.
  Term lhs = bin(L_OP,
                 bin(L_OP, bin(L_OP, a, b), cc),
                 un(L_INV, bin(L_OP, a, b)));
  thvm_atp_set_goal(s, lhs, cc);

  AtpStatus st = ATP_RUNNING;
  u32 iters = 0;
  for (u32 i = 0; i < 8192u; i++) {
    st = thvm_atp_step(s);
    iters++;
    if (st != ATP_RUNNING) break;
  }
  printf("  %-18s mask=%llx  %s  iters=%u  n_rules=%u\n",
         label, (unsigned long long)ac_mask,
         status_name(st), iters, s->n_rules);

  *st_out = st;
  *iters_out = iters;
  *n_rules_out = s->n_rules;

  thvm_atp_free(s);
  thvm_atp_set_ac_mask(0ull);
}

int main(void) {
  thvm_init();

  AtpStatus syn_st = ATP_RUNNING, ac_st = ATP_RUNNING;
  u32 syn_iters = 0, ac_iters = 0;
  u32 syn_rules = 0, ac_rules = 0;

  printf("== AbelianGroup/ImpliesAbelianMcCune AC reproducer ==\n");

  TEST_BEGIN("abelian-syntactic-proves");
  run_once(0ull, "syntactic", &syn_st, &syn_iters, &syn_rules);
  CHECK(syn_st == ATP_PROVED);

  TEST_BEGIN("abelian-ac-proves");
  run_once(1ull << L_OP, "ac-on", &ac_st, &ac_iters, &ac_rules);
  // AC should NOT regress vs syntactic; failing this is the bug.
  CHECK(ac_st == ATP_PROVED);

  thvm_free();
  TEST_REPORT();
}
