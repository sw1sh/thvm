// test_heap_rooted_preserve.c - hrp1 of the heap-rooted preserve
// arc.  Smoke-tests the standalone heap walk.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // Layout three TenDescs.  Only `referenced` and `also_ref`
  // get TAG_TEN cells written into HEAP[]; `orphan` does not.
  // After mark_heap_rooted_preserve(), only the two referenced
  // bufs should carry preserved=1.
  Shape s = {0}; s.ndim = 1; s.dims[0] = 4;
  u32 referenced = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
  u32 also_ref   = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
  u32 orphan     = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
  CHECK(referenced > 0 && also_ref > 0 && orphan > 0);
  u32 b_ref     = TENS[referenced].buf_id;
  u32 b_also    = TENS[also_ref  ].buf_id;
  u32 b_orphan  = TENS[orphan    ].buf_id;

  // Scrub any stray preserved bits from prior runs / init.
  cpu_buf_clear_preserved(0);

  // Place TAG_TEN cells in the heap directly.  In real flow
  // these come from term_new / heap_set during materialize +
  // UOP construction; for the unit test we write them
  // manually so we can isolate the heap-walk behavior.
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, term_new(0, TAG_TEN, DT_F32, referenced));
  heap_set(loc + 1, term_new(0, TAG_TEN, DT_F32, also_ref));

  TEST_BEGIN("heap-rooted/walk-marks-cells-it-finds");
  // Pre: nothing preserved.
  CHECK_EQ(CPU_BUFS[b_ref   ].preserved, 0);
  CHECK_EQ(CPU_BUFS[b_also  ].preserved, 0);
  CHECK_EQ(CPU_BUFS[b_orphan].preserved, 0);

  mark_heap_rooted_preserve();

  CHECK_EQ(CPU_BUFS[b_ref   ].preserved, 1);
  CHECK_EQ(CPU_BUFS[b_also  ].preserved, 1);
  // The orphan was never written into the heap as a TAG_TEN.
  CHECK_EQ(CPU_BUFS[b_orphan].preserved, 0);

  TEST_BEGIN("heap-rooted/idempotent");
  // Running it again leaves preserved bits unchanged.
  mark_heap_rooted_preserve();
  CHECK_EQ(CPU_BUFS[b_ref   ].preserved, 1);
  CHECK_EQ(CPU_BUFS[b_also  ].preserved, 1);
  CHECK_EQ(CPU_BUFS[b_orphan].preserved, 0);

  TEST_BEGIN("heap-rooted/clear-preserved-zeros-flags");
  // The preserved bits clear via cpu_buf_clear_preserved.
  cpu_buf_clear_preserved(0);
  CHECK_EQ(CPU_BUFS[b_ref   ].preserved, 0);
  CHECK_EQ(CPU_BUFS[b_also  ].preserved, 0);

  TEST_BEGIN("heap-rooted/finds-tag-ten-via-uop-children");
  // Build a UOP whose first child cell IS a TAG_TEN.  The
  // linear scan walks every HEAP cell, so it visits the UOP's
  // child slot directly and marks `referenced` again.
  cpu_buf_clear_preserved(0);
  u64 uloc = heap_alloc(2);
  heap_set(uloc + 0, term_new(0, TAG_TEN, DT_F32, referenced));
  heap_set(uloc + 1, term_new(0, TAG_TEN, DT_F32, also_ref));
  Term uopT = term_new(0, TAG_UOP, UOP_ADD, uloc);
  // Stash the uop term itself somewhere in the heap.
  u64 root = heap_alloc(1);
  heap_set(root, uopT);

  mark_heap_rooted_preserve();
  CHECK_EQ(CPU_BUFS[b_ref ].preserved, 1);
  CHECK_EQ(CPU_BUFS[b_also].preserved, 1);

  TEST_BEGIN("heap-rooted/skips-non-ten-tags");
  // TAG_NUM / TAG_VAR / TAG_ERA cells must not crash and
  // must not flip any preserved bit by accident.
  cpu_buf_clear_preserved(0);
  u64 misc = heap_alloc(3);
  heap_set(misc + 0, term_new(0, TAG_NUM, DT_F32, 42));
  heap_set(misc + 1, term_new(0, TAG_VAR, 0, 0));
  heap_set(misc + 2, term_new(0, TAG_ERA, 0, 0));
  mark_heap_rooted_preserve();
  // The earlier TAG_TEN cells we wrote are still in heap; they
  // get re-marked.  The orphan stays unmarked.
  CHECK_EQ(CPU_BUFS[b_orphan].preserved, 0);

  thvm_free();
  TEST_REPORT();
}
