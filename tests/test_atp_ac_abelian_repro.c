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

static void run_once(int mode, const char *label,
                     AtpStatus *st_out, u32 *iters_out, u32 *n_rules_out) {
  // mode 0: no AC (mask=0).
  // mode 1: ac_mask set BEFORE init + add (the original C repro).
  // mode 2: ac_mask set via auto_ac AFTER add, BEFORE goal (mirrors the
  //         WL paclet's THVM_ATP_AUTO_AC=1 order).
  Term x = v(0), y = v(1), z = v(2);
  Term e = k(L_E);
  Term a = k(L_A), b = k(L_B), cc = k(L_C);

  // Weights all 1.  Precedence MIRRORS the WL paclet's default
  // identity precedence (prec[i] = i+1), so the regression at the
  // WL bridge is reproduced byte-for-byte at the C level.
  static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
  static const u32 P[8] = { 0u, 2u, 3u, 4u, 5u, 6u, 7u, 0u };
  KboConfig kbo = { .weights = W, .precedence = P,
                    .n_labels = 8, .var_weight = 1u };

  if (mode == 1) {
    thvm_atp_set_ac_mask(1ull << L_OP);
  } else {
    thvm_atp_set_ac_mask(0ull);
  }

  AtpState *s = thvm_atp_init(&kbo, 8192);
  // Comm + assoc + identity + inverse for L_OP.
  Term ax_lhs[4], ax_rhs[4];
  ax_lhs[0] = bin(L_OP, x, y);              ax_rhs[0] = bin(L_OP, y, x);
  ax_lhs[1] = bin(L_OP, bin(L_OP, x, y), z);
  ax_rhs[1] = bin(L_OP, x, bin(L_OP, y, z));
  ax_lhs[2] = bin(L_OP, x, e);              ax_rhs[2] = x;
  ax_lhs[3] = bin(L_OP, x, un(L_INV, x));   ax_rhs[3] = e;
  for (u32 i = 0; i < 4; i++) {
    thvm_atp_add_equation(s, ax_lhs[i], ax_rhs[i]);
  }
  if (mode == 2) {
    thvm_atp_auto_ac(ax_lhs, ax_rhs, 4u);
  }

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
  printf("  %-22s mode=%d  mask=%llx  %s  iters=%u  n_rules=%u\n",
         label, mode,
         (unsigned long long)thvm_atp_get_ac_mask(),
         status_name(st), iters, s->n_rules);

  *st_out = st;
  *iters_out = iters;
  *n_rules_out = s->n_rules;

  thvm_atp_free(s);
  thvm_atp_set_ac_mask(0ull);
}

int main(void) {
  thvm_init();

  AtpStatus st = ATP_RUNNING;
  u32 iters = 0, n_rules = 0;

  printf("== AbelianGroup/ImpliesAbelianMcCune AC reproducer ==\n");

  TEST_BEGIN("abelian-no-ac-proves");
  run_once(0, "no-ac (control)", &st, &iters, &n_rules);
  CHECK(st == ATP_PROVED);

  TEST_BEGIN("abelian-ac-set-before-init-proves");
  run_once(1, "ac before init", &st, &iters, &n_rules);
  CHECK(st == ATP_PROVED);

  TEST_BEGIN("abelian-ac-auto-after-add-proves");
  run_once(2, "auto_ac after add", &st, &iters, &n_rules);
  // Mirrors the WL paclet's call order; if this reproduces the
  // Saturated/Steps=0 hang seen via wolframscript, the bug is in
  // engine state set up between add and goal.
  CHECK(st == ATP_PROVED);

  thvm_free();
  TEST_REPORT();
}
