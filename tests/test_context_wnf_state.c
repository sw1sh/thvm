// test_context_wnf_state.c -- regression: switching contexts must
// keep CURRENT_WNF_STATE pointing at the active context's wnf_state.
//
// Pre-fix, thvm_context_select swapped CURRENT_CTX without touching
// CURRENT_WNF_STATE -- so wnf() called after the switch still wrote
// to the OLD context's stack, eventually crashing context.wlt with
// SIGABRT once the foreign heap got corrupted.

#include "../src/thvm.c"
#include "test.h"

static int test_select_updates_wnf_state(void) {
  TEST_BEGIN("thvm_context_select swaps CURRENT_WNF_STATE in lockstep");
  thvm_init();

  // Default ctx state.
  WnfThreadState *s0 = CURRENT_WNF_STATE;
  CHECK(s0 == &CONTEXTS[0]->wnf_state);

  u32 alt = thvm_context_create(NULL);
  CHECK(alt > 0);

  // Selecting alt must update the WNF state pointer.
  thvm_context_select(alt);
  CHECK(CURRENT_WNF_STATE == &CONTEXTS[alt]->wnf_state);
  CHECK(CURRENT_CTX == CONTEXTS[alt]);

  // Selecting back must restore.
  thvm_context_select(0);
  CHECK(CURRENT_WNF_STATE == &CONTEXTS[0]->wnf_state);
  CHECK(CURRENT_CTX == CONTEXTS[0]);

  thvm_context_destroy(alt);
  thvm_free();
  return 0;
}

static int test_create_restores_caller_state(void) {
  TEST_BEGIN("thvm_context_create restores caller's WNF state on return");
  thvm_init();
  WnfThreadState *before = CURRENT_WNF_STATE;
  u32 alt = thvm_context_create(NULL);
  CHECK(alt > 0);
  // create() temporarily switches CURRENT_CTX to alt for init, then
  // restores.  Both pointers must be back at the caller's home.
  CHECK(CURRENT_WNF_STATE == before);
  CHECK(CURRENT_CTX == CONTEXTS[0]);
  thvm_context_destroy(alt);
  thvm_free();
  return 0;
}

static int test_destroy_falls_back_to_default(void) {
  TEST_BEGIN("thvm_context_destroy on the active ctx falls back to slot 0");
  thvm_init();
  u32 alt = thvm_context_create(NULL);
  thvm_context_select(alt);
  CHECK(CURRENT_WNF_STATE == &CONTEXTS[alt]->wnf_state);
  thvm_context_destroy(alt);
  // Active was alt; destroy should leave us on slot 0.
  CHECK(CURRENT_CTX == CONTEXTS[0]);
  CHECK(CURRENT_WNF_STATE == &CONTEXTS[0]->wnf_state);
  thvm_free();
  return 0;
}

int main(void) {
  test_select_updates_wnf_state();
  test_create_restores_caller_state();
  test_destroy_falls_back_to_default();
  TEST_REPORT();
}
