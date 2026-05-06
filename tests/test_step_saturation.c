// test_step_saturation.c -- regression for the silent-corruption bug
// in the step-session use-list hash table.
//
// Pre-fix: a distinct-leaf balanced ADD tree past ~16k distinct Term
// values saturated the fixed-cap (16384 buckets) hash, returned a
// valid bucket index 0 from the saturated insert, and corrupted the
// table.  heap_replace lookups for missing entries hit the empty-
// bucket sentinel and stopped walking, so reduction stalled at the
// first level and the result was wrong.
//
// Post-fix: the table is sized to ~4x HEAP_NEXT at attach, saturation
// returns STEP_USE_HEAD_FULL, and heap_replace falls back to the
// linear scan when an entry is missing.  The reduction completes
// correctly.

#include "../src/thvm.c"
#include "test.h"

// Build a balanced ADD tree of 2^depth distinct integer leaves.
static Term build_distinct_add_tree(u32 depth) {
  u32 n = 1u << depth;
  Term *level = (Term *)malloc(n * sizeof(Term));
  for (u32 i = 0; i < n; i++) {
    level[i] = term_new(0, TAG_NUM, DT_INT32, i + 1);
  }
  while (n > 1) {
    u32 m = n / 2;
    for (u32 i = 0; i < m; i++) {
      u64 loc = heap_alloc(2);
      heap_set(loc + 0, level[2*i]);
      heap_set(loc + 1, level[2*i + 1]);
      level[i] = term_new(0, TAG_OP2, OP_ADD, loc);
    }
    n = m;
  }
  Term root = level[0];
  free(level);
  return root;
}

static int test_distinct_tree_depth_14(void) {
  TEST_BEGIN("nf reduces depth-14 distinct ADD tree to sum 1..16384");
  thvm_init();

  Term root = build_distinct_add_tree(14);
  Term result = nf(root);

  CHECK_EQ(term_tag(result), TAG_NUM);
  // Sum 1..16384 = 16384 * 16385 / 2 = 134225920.
  CHECK_EQ(term_val(result), 134225920u);

  // All 16383 redexes must have fired (8192 + 4096 + ... + 1).
  const WnfPoolStats *s = wnf_pool_last_stats();
  CHECK_EQ(s->total_fires, 16383u);

  thvm_free();
  return 0;
}

static int test_distinct_tree_depth_12(void) {
  TEST_BEGIN("nf reduces depth-12 distinct ADD tree to sum 1..4096");
  thvm_init();

  Term root = build_distinct_add_tree(12);
  Term result = nf(root);

  CHECK_EQ(term_tag(result), TAG_NUM);
  // Sum 1..4096 = 4096 * 4097 / 2 = 8390656.
  CHECK_EQ(term_val(result), 8390656u);

  thvm_free();
  return 0;
}

int main(void) {
  test_distinct_tree_depth_12();
  test_distinct_tree_depth_14();
  TEST_REPORT();
}
