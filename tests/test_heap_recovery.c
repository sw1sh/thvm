// test_heap_recovery.c -- verify thvm_heap_exhaust_jmp / thvm_fatal
// longjmp recovery path.  Replaces the historical exit(1) on heap
// exhaust with a longjmp back to a caller-registered setjmp, so a
// LibraryLink entry (thvm_wl_atp_run_proof) can return
// LIBRARY_FUNCTION_ERROR instead of orphaning WolframKernel.
//
// See feedback_wolframscript_oom_risk.md for the failure scenario
// this prevents.

#include "../src/thvm.c"
#include "test.h"
#include <setjmp.h>

int main(void) {
  thvm_init();

  TEST_BEGIN("heap/recovery/exit-path-when-no-hook");
  // With thvm_heap_exhaust_jmp == NULL (default), heap_alloc on
  // exhaust would exit(1).  We can't test that path here without
  // killing the test process; the legacy behaviour is just verified
  // by reading the source (heap_alloc only exits when the hook is
  // NULL).  Sentinel: confirm the hook starts NULL.
  CHECK_EQ(thvm_heap_exhaust_jmp, NULL);
  CHECK_EQ(thvm_heap_exhausted, 0);

  TEST_BEGIN("heap/recovery/longjmp-fires-on-exhaust");
  // Install the recovery target, then attempt an allocation larger
  // than the entire heap.  heap_alloc should detect at+size > cap
  // and longjmp back here.
  jmp_buf jb;
  thvm_heap_exhaust_jmp = &jb;
  thvm_heap_exhausted   = 0;
  int landed_from_longjmp = 0;
  if (setjmp(jb) != 0) {
    landed_from_longjmp = 1;
  } else {
    // First setjmp pass: trigger heap exhaust.  Asking for
    // 2 * thvm_heap_cells() guarantees at + size > cap regardless
    // of current HEAP_NEXT, even if other allocs ran before.
    (void)heap_alloc(2 * thvm_heap_cells());
    // If we reach here without longjmp, the recovery failed --
    // exit(1) would have been called in the buggy version.
  }
  CHECK_EQ(landed_from_longjmp, 1);
  CHECK_EQ(thvm_heap_exhausted, 1);
  thvm_heap_exhaust_jmp = NULL;

  TEST_BEGIN("heap/recovery/thvm_fatal-respects-hook");
  // thvm_fatal is the unified OOM recovery helper used by acp_pack,
  // dt index pools, mnf rule cache, etc.  Same longjmp path as
  // heap_alloc.  Verify it longjmps when the hook is set.
  thvm_heap_exhaust_jmp = &jb;
  thvm_heap_exhausted   = 0;
  int landed_from_fatal = 0;
  if (setjmp(jb) != 0) {
    landed_from_fatal = 1;
  } else {
    thvm_fatal("synthetic test OOM -- expect longjmp");
  }
  CHECK_EQ(landed_from_fatal, 1);
  CHECK_EQ(thvm_heap_exhausted, 1);
  thvm_heap_exhaust_jmp = NULL;

  thvm_free();
  TEST_REPORT();
}
