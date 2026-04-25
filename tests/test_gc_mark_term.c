// test_gc_mark_term.c - gc2 of the tracing-GC arc.
//
// Builds small synthetic Term graphs and verifies gc_mark_term
// visits every reachable buf and skips orphans.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32(u32 n) {
  Shape s = {0}; s.ndim = 1; s.dims[0] = n;
  return tensor_alloc(CURRENT_BACKEND, s, DT_F32);
}

int main(void) {
  thvm_init();

  u8 *visited = (u8 *)calloc(HEAP_CAP, 1);
  CHECK(visited != NULL);

  TEST_BEGIN("gc-mark/leaf-tag-ten-marks-its-buf");
  u32 tA = alloc_f32(4);
  cpu_buf_clear_preserved(0);
  gc_mark_term(term_new(0, TAG_TEN, DT_F32, tA), visited);
  CHECK_EQ(CPU_BUFS[TENS[tA].buf_id].preserved, 1);

  TEST_BEGIN("gc-mark/uop-add-marks-both-children");
  // TUOpAdd[ten_a, ten_b] with ten_c held only as a separate
  // TenDesc; mark from the UOP must set preserved on a and b
  // but NOT on c.
  thvm_free();
  thvm_init();
  memset(visited, 0, HEAP_CAP);
  u32 a = alloc_f32(4);
  u32 b = alloc_f32(4);
  u32 c = alloc_f32(4);
  Term ta = term_new(0, TAG_TEN, DT_F32, a);
  Term tb = term_new(0, TAG_TEN, DT_F32, b);
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, ta);
  heap_set(loc + 1, tb);
  Term add = term_new(0, TAG_UOP, UOP_ADD, loc);

  cpu_buf_clear_preserved(0);
  gc_mark_term(add, visited);
  CHECK_EQ(CPU_BUFS[TENS[a].buf_id].preserved, 1);
  CHECK_EQ(CPU_BUFS[TENS[b].buf_id].preserved, 1);
  CHECK_EQ(CPU_BUFS[TENS[c].buf_id].preserved, 0);

  TEST_BEGIN("gc-mark/zero-term-no-op");
  thvm_free();
  thvm_init();
  memset(visited, 0, HEAP_CAP);
  cpu_buf_clear_preserved(0);
  gc_mark_term(0, visited);   // must not crash
  // No tids exist; no-op is success.

  TEST_BEGIN("gc-mark/leaf-non-ten-tags-no-effect");
  // TAG_NUM, TAG_VAR, TAG_ERA shouldn't crash and shouldn't
  // mark anything.
  u32 d = alloc_f32(4);
  cpu_buf_clear_preserved(0);
  gc_mark_term(term_new(0, TAG_NUM, DT_F32, 42),  visited);
  gc_mark_term(term_new(0, TAG_VAR, 0, 0),        visited);
  gc_mark_term(term_new(0, TAG_ERA, 0, 0),        visited);
  CHECK_EQ(CPU_BUFS[TENS[d].buf_id].preserved, 0);

  TEST_BEGIN("gc-mark/cycle-via-visited-bitmap");
  // App[lam, app2] where app2 points back to the same app
  // location -- a synthetic cycle.  Without the visited
  // bitmap this would infinite-loop; with it, we visit each
  // cell once.
  thvm_free();
  thvm_init();
  memset(visited, 0, HEAP_CAP);
  u64 cyc_loc = heap_alloc(2);
  heap_set(cyc_loc + 0, term_new(0, TAG_ERA, 0, 0));
  heap_set(cyc_loc + 1, term_new(0, TAG_APP, 0, cyc_loc));   // self-loop
  Term cyc = term_new(0, TAG_APP, 0, cyc_loc);
  gc_mark_term(cyc, visited);   // returns; doesn't loop forever

  TEST_BEGIN("gc-mark/follows-tag-ref-into-defs");
  // Register a def whose body holds a TAG_TEN; mark from a
  // TAG_REF for that name should reach the buf.
  thvm_free();
  thvm_init();
  memset(visited, 0, HEAP_CAP);
  u32 e = alloc_f32(4);
  DEFS[3] = term_new(0, TAG_TEN, DT_F32, e);
  cpu_buf_clear_preserved(0);
  gc_mark_term(term_new(0, TAG_REF, 0, 3), visited);
  CHECK_EQ(CPU_BUFS[TENS[e].buf_id].preserved, 1);
  DEFS[3] = 0;

  free(visited);
  thvm_free();
  TEST_REPORT();
}
