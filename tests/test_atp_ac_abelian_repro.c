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

// Mirror the WL paclet's encoding of AbelianGroupAxioms exactly:
//   x ⊗ y                  -> L_OP (binary, AC)
//   OverTilde[1] = identity -> L_TILDE applied to L_ONE
//   OverBar[x]   = inverse  -> L_BAR (unary)
// The identity element is a COMPOUND term (a unary function applied
// to a constant), not a bare arity-0 constant.  The earlier mode=1/2
// runs used `e = k(L_E)` -- semantically equivalent but structurally
// different.
#define L_OP     1u
#define L_TILDE  2u   // OverTilde (unary)
#define L_ONE    3u   // integer 1 (constant, arity 0)
#define L_BAR    4u   // OverBar (unary)
#define L_A      5u
#define L_B      6u
#define L_C      7u

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
  // Identity element as a COMPOUND term: OverTilde[1].
  Term e = un(L_TILDE, k(L_ONE));
  Term a = k(L_A), b = k(L_B), cc = k(L_C);

  // Weights all 1.  Precedence MIRRORS the WL paclet's default
  // identity precedence (prec[i] = i+1).
  static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
  static const u32 P[8] = { 0u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
  KboConfig kbo = { .weights = W, .precedence = P,
                    .n_labels = 8, .var_weight = 1u };

  if (mode == 1) {
    thvm_atp_set_ac_mask(1ull << L_OP);
  } else {
    thvm_atp_set_ac_mask(0ull);
  }

  AtpState *s = thvm_atp_init(&kbo, 8192);
  // Comm + assoc + identity + inverse for L_OP.  Identity element is
  // OverTilde[1], inverse is OverBar[x], to mirror the WL encoding.
  Term ax_lhs[4], ax_rhs[4];
  ax_lhs[0] = bin(L_OP, x, y);              ax_rhs[0] = bin(L_OP, y, x);
  ax_lhs[1] = bin(L_OP, bin(L_OP, x, y), z);
  ax_rhs[1] = bin(L_OP, x, bin(L_OP, y, z));
  // Try LEFT-identity / LEFT-inverse (matches test_atp_ac_bench's
  // abelian-group-inv test which proves under AC):
  ax_lhs[2] = bin(L_OP, e, x);              ax_rhs[2] = x;
  ax_lhs[3] = bin(L_OP, un(L_BAR, x), x);   ax_rhs[3] = e;
  for (u32 i = 0; i < 4; i++) {
    thvm_atp_add_equation(s, ax_lhs[i], ax_rhs[i]);
  }
  if (mode == 2) {
    thvm_atp_auto_ac(ax_lhs, ax_rhs, 4u);
  }

  // Goal: ((a ⊗ b) ⊗ c) ⊗ OverBar[a ⊗ b] = c.
  Term lhs = bin(L_OP,
                 bin(L_OP, bin(L_OP, a, b), cc),
                 un(L_BAR, bin(L_OP, a, b)));
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
  if (getenv("DUMP_RULES") != NULL) {
    for (u32 r = 0; r < s->n_rules; r++) {
      Term l = s->lhs[r], rh = s->rhs[r];
      fprintf(stderr, "  rule[%u] dead=%u\n", r, s->r_dead[r]);
      fprintf(stderr, "    lhs: tag=%u ext=%u",
              term_tag(l), term_ext(l));
      if (term_tag(l) == TAG_CTR) {
        u32 n = term_ctr_n(l);
        fprintf(stderr, " arity=%u", n);
        for (u32 c = 0; c < n && c < 4; c++) {
          Term ch = term_ctr_at(l, c);
          fprintf(stderr, "  c%u(tag=%u,ext=%u)", c, term_tag(ch), term_ext(ch));
        }
      }
      fprintf(stderr, "\n    rhs: tag=%u ext=%u",
              term_tag(rh), term_ext(rh));
      if (term_tag(rh) == TAG_CTR) {
        u32 n = term_ctr_n(rh);
        fprintf(stderr, " arity=%u", n);
        for (u32 c = 0; c < n && c < 4; c++) {
          Term ch = term_ctr_at(rh, c);
          fprintf(stderr, "  c%u(tag=%u,ext=%u)", c, term_tag(ch), term_ext(ch));
        }
      }
      fprintf(stderr, "\n");
    }
  }

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

  if (getenv("MODE1_ONLY") == NULL) {
    TEST_BEGIN("abelian-no-ac-proves");
    run_once(0, "no-ac (control)", &st, &iters, &n_rules);
    CHECK(st == ATP_PROVED);
  }

  TEST_BEGIN("abelian-ac-set-before-init-proves");
  run_once(1, "ac before init", &st, &iters, &n_rules);
  CHECK(st == ATP_PROVED);

  if (getenv("MODE1_ONLY") != NULL) goto done;

  TEST_BEGIN("abelian-ac-auto-after-add-proves");
  run_once(2, "auto_ac after add", &st, &iters, &n_rules);
  // Mirrors the WL paclet's call order; if this reproduces the
  // Saturated/Steps=0 hang seen via wolframscript, the bug is in
  // engine state set up between add and goal.
  CHECK(st == ATP_PROVED);

done:
  thvm_free();
  TEST_REPORT();
}
