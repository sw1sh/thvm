// test_wald.c - Waldmeister .pr parser pieces.
//
// 6.3a: data model only -- WaldSpec init/free + default state.
// Later stages (6.3b lexer, 6.3c section drivers, 6.3d term parser,
// 6.3e equations, 6.3f top-level driver, 6.3g end-to-end) extend
// this file.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("wald/init-and-free");
  {
    WaldSpec *s = wald_init();
    CHECK(s != NULL);
    CHECK_EQ((int)s->mode_proof,    1);          // defaults to PROOF
    CHECK_EQ(s->n_symbols,          0u);
    CHECK_EQ(s->n_vars,             0u);
    CHECK_EQ(s->n_eqns,             0u);
    CHECK_EQ(s->next_label,         1u);         // skips the anonymous-tuple label
    CHECK_EQ(s->goal_lhs,           0u);
    CHECK_EQ(s->goal_rhs,           0u);
    CHECK_EQ((int)s->name[0],       0);          // empty name string
    wald_free(s);
  }

  TEST_BEGIN("wald/free-null-is-safe");
  {
    wald_free(NULL);   // no crash, no-op
  }

  TEST_BEGIN("wald/caps-are-defined");
  {
    // Sanity that the cap macros are sized roughly right -- catches
    // accidental zero-init regressions in thvm.h.
    CHECK(WALD_MAX_SYMBOLS >= 4u);
    CHECK(WALD_MAX_VARS    >= 3u);
    CHECK(WALD_MAX_EQNS    >= 3u);
    CHECK(WALD_NAME_LEN    >= 4u);
  }

  thvm_free();
  TEST_REPORT();
}
