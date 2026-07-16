// test_atp_session_reuse.c -- multi-run WM-preset engine sessions in ONE
// process.
//
// The WL bridge (and any long-lived host) runs many TFindProof engine
// sessions per process: thvm_atp_init .. thvm_atp_free, repeated.  The
// per-state flatterm arena (src/atp/ft_alloc.c) is malloc'd per session,
// so a later session's arena can land at the SAME address as a freed
// one -- any file-scope cache keyed by arena address, span pointer, or
// r_revision watermark silently aliases across sessions and frees or
// serves spans that belong to a DEAD arena (observed: `ft_alloc:
// free_span underflow` exit(1) on WolframAxioms Commutativity ->
// DoubleNegation; SIGBUS on AbelianGroup -> Sheffer).  The CLI never
// sees this (one problem per process); this harness is the in-process
// twin of the WL repro.
//
// Runs the WM-preset bench (the exact TU the CLI wraps) on a fixed
// problem list IN ONE PROCESS and asserts every run completes with the
// single-run status.  Extra .pr paths on argv override the built-in
// list (status not asserted for overrides).

#define main atp_bench_main
#include "test_atp_wolfram_bench.c"
#undef main

#include "test.h"

typedef struct {
  const char *path;
  int         rc;      // atp_bench_main exit: 0 = PROVED
} SessionCase;

int main(int argc, char **argv) {
  // The WL Method->"Waldmeister" twin: the bench TU applies the full
  // preset off this env (overwrite=0 keeps explicit user knobs).
  setenv("THVM_ATP_WALDMEISTER", "1", 0);

  SessionCase cases[16];
  u32 n = 0;
  if (argc > 1) {
    for (int i = 1; i < argc && n < 16; i++) {
      cases[n].path = argv[i];
      cases[n].rc   = -1;   // don't assert overrides
      n++;
    }
  } else {
    // Both orders of the deterministic WL underflow pair, then the
    // SIGBUS pair's problems -- each proves standalone in seconds.
    cases[n++] = (SessionCase){
      "tools/baselines/wm_pr/WolframAxioms__Commutativity.pr", 0};
    cases[n++] = (SessionCase){
      "tools/baselines/wm_pr/WolframAxioms__DoubleNegation.pr", 0};
    cases[n++] = (SessionCase){
      "tools/baselines/wm_pr/WolframAxioms__Commutativity.pr", 0};
    cases[n++] = (SessionCase){
      "tools/baselines/wm_pr/AbelianGroupAxioms__InverseOfInverse.pr", 0};
    cases[n++] = (SessionCase){
      "tools/baselines/wm_pr/ShefferAxioms__OrAssociativity.pr", 0};
  }

  for (u32 i = 0; i < n; i++) {
    TEST_BEGIN(cases[i].path);
    char *bargv[5];
    bargv[0] = (char *)"test_atp_session_reuse";
    bargv[1] = (char *)cases[i].path;
    bargv[2] = (char *)"500000";
    bargv[3] = (char *)"60";
    bargv[4] = NULL;
    int rc = atp_bench_main(4, bargv);
    fprintf(stderr, "%% session %u/%u: %s rc=%d\n",
            i + 1u, n, cases[i].path, rc);
    if (cases[i].rc >= 0) CHECK_EQ(rc, cases[i].rc);
  }

  TEST_REPORT();
}
