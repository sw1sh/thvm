// test_atp_wpo.c -- ATP saturation driven by WPO.

#include "../src/thvm.c"
#include "test.h"

#define L_F  1u
#define L_A  2u
#define L_B  3u
#define L_C  4u

static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0u); }
static Term bin(u32 lab, Term x, Term y) {
  Term kids[2] = { x, y };
  return term_new_ctr(lab, kids, 2u);
}
static Term v(u32 id) { return term_new_fvr(id); }

int main(void) {
  thvm_init();

  // Trivial: prove f(a, b) = c from axiom f(a, b) = c under WPO.
  TEST_BEGIN("atp/wpo-trivial");
  {
    static const u32 W[8]  = { 0u, 1u, 1u, 1u, 1u, 0u, 0u, 0u };
    static const u32 P[8]  = { 0u, 4u, 1u, 2u, 3u, 0u, 0u, 0u };
    static const WpoStatus St[8] = {
      WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX,
      WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX,
    };
    KboConfig kbo = { .weights = W, .precedence = P, .n_labels = 8, .var_weight = 1u };
    WpoConfig wpo = { .weights = W, .precedence = P, .status = St, .n_labels = 8, .var_weight = 1u };

    AtpState *s = thvm_atp_init(&kbo, 1024);
    thvm_atp_set_wpo(s, &wpo);

    thvm_atp_add_equation(s, bin(L_F, k(L_A), k(L_B)), k(L_C));
    thvm_atp_set_goal(s, bin(L_F, k(L_A), k(L_B)), k(L_C));

    AtpStatus st = ATP_RUNNING;
    for (u32 i = 0; i < 64u; i++) {
      st = thvm_atp_step(s);
      if (st != ATP_RUNNING) break;
    }
    CHECK(st == ATP_PROVED);
    thvm_atp_free(s);
  }

  // Drive saturation past comm + assoc under WPO (all-LEX status,
  // weights = 1).  WPO with weights = 1 and all-LEX is equivalent to
  // LPO on weight ties.  Sanity check that the engine reaches a
  // terminal status.
  TEST_BEGIN("atp/wpo-syntactic-comm-monoid");
  {
    Term x = v(0), y = v(1), z = v(2);
    Term a = k(L_A), b = k(L_B), c = k(L_C);

    static const u32 W[8]  = { 0u, 1u, 1u, 1u, 1u, 0u, 0u, 0u };
    static const u32 P[8]  = { 0u, 1u, 4u, 3u, 2u, 0u, 0u, 0u };
    static const WpoStatus St[8] = {
      WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX,
      WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX,
    };
    KboConfig kbo = { .weights = W, .precedence = P, .n_labels = 8, .var_weight = 1u };
    WpoConfig wpo = { .weights = W, .precedence = P, .status = St, .n_labels = 8, .var_weight = 1u };

    AtpState *s = thvm_atp_init(&kbo, 4096);
    thvm_atp_set_wpo(s, &wpo);

    thvm_atp_add_equation(s, bin(L_F, x, y), bin(L_F, y, x));
    thvm_atp_add_equation(s, bin(L_F, bin(L_F, x, y), z),
                              bin(L_F, x, bin(L_F, y, z)));
    thvm_atp_set_goal(s, bin(L_F, bin(L_F, a, b), c),
                          bin(L_F, c, bin(L_F, b, a)));

    AtpStatus st = ATP_RUNNING;
    for (u32 i = 0; i < 4096u; i++) {
      st = thvm_atp_step(s);
      if (st != ATP_RUNNING) break;
    }
    CHECK(st != ATP_RUNNING);
    thvm_atp_free(s);
  }

  // WPO with status(f) = MUL on the symbol -- self-test that the AC
  // path under MUL-status doesn't fire orientations that break.
  TEST_BEGIN("atp/wpo-mul-status");
  {
    static const u32 W[8]  = { 0u, 1u, 1u, 1u, 1u, 0u, 0u, 0u };
    static const u32 P[8]  = { 0u, 1u, 4u, 3u, 2u, 0u, 0u, 0u };
    static const WpoStatus St[8] = {
      WPO_STATUS_LEX, WPO_STATUS_MUL, WPO_STATUS_LEX, WPO_STATUS_LEX,
      WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX,
    };
    KboConfig kbo = { .weights = W, .precedence = P, .n_labels = 8, .var_weight = 1u };
    WpoConfig wpo = { .weights = W, .precedence = P, .status = St, .n_labels = 8, .var_weight = 1u };

    AtpState *s = thvm_atp_init(&kbo, 1024);
    thvm_atp_set_wpo(s, &wpo);
    // Trivial goal that must prove under any orientation.
    thvm_atp_add_equation(s, bin(L_F, k(L_A), k(L_B)), k(L_C));
    thvm_atp_set_goal(s, bin(L_F, k(L_A), k(L_B)), k(L_C));
    AtpStatus st = ATP_RUNNING;
    for (u32 i = 0; i < 64u; i++) {
      st = thvm_atp_step(s);
      if (st != ATP_RUNNING) break;
    }
    CHECK(st == ATP_PROVED);
    thvm_atp_free(s);
  }

  thvm_free();
  TEST_REPORT();
}
