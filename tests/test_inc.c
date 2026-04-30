// test_inc.c - TAG_INC priority wrapper + thvm_collapse_ordered.
//
// INC is a WNF atom with one heap cell holding the body.  The
// `thvm_collapse_ordered` enumerator walks the SUP-tree as usual,
// counts INC wrappers on the path to each leaf, and sorts results
// by INC-depth ascending (lower = higher priority = emitted first).

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("inc/whnf-stays-as-inc");
  {
    // wnf(INC(NUM(7))) returns INC at the head; INC is opaque to wnf.
    Term t = term_new_inc(build_num(7));
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_INC);
  }

  TEST_BEGIN("inc/collapse-ordered-no-inc-keeps-dfs-order");
  {
    // Without any INCs, ordered collapse equals the regular DFS order.
    Term t = build_sup(7, build_num(11), build_num(22));
    Term out[4] = {0};
    u64  n = thvm_collapse_ordered(t, out, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_val(out[0]), 11);
    CHECK_EQ(term_val(out[1]), 22);
  }

  TEST_BEGIN("inc/collapse-ordered-promotes-low-priority");
  {
    // Left branch wrapped twice (cost 2), right branch unwrapped (cost 0).
    // Ordered collapse emits the right branch first.
    Term left  = term_new_inc(term_new_inc(build_num(11)));
    Term right = build_num(22);
    Term t     = build_sup(7, left, right);
    Term out[4] = {0};
    u64  n = thvm_collapse_ordered(t, out, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_val(out[0]), 22);  // priority 0
    CHECK_EQ(term_val(out[1]), 11);  // priority 2
  }

  TEST_BEGIN("inc/collapse-ordered-three-levels");
  {
    // SUP(SUP(NUM(1) cost 0, INC NUM(2) cost 1), INC INC NUM(3) cost 2)
    // Expected order: 1 (cost 0), 2 (cost 1), 3 (cost 2)
    Term n1 = build_num(1);
    Term n2 = term_new_inc(build_num(2));
    Term n3 = term_new_inc(term_new_inc(build_num(3)));
    Term s12 = build_sup(7, n1, n2);
    Term t   = build_sup(7, s12, n3);
    Term out[4] = {0};
    u64  n = thvm_collapse_ordered(t, out, 4);
    CHECK_EQ(n, 3);
    CHECK_EQ(term_val(out[0]), 1);
    CHECK_EQ(term_val(out[1]), 2);
    CHECK_EQ(term_val(out[2]), 3);
  }

  TEST_BEGIN("inc/cap-truncates-after-sort");
  {
    Term n1 = term_new_inc(term_new_inc(build_num(11)));   // cost 2
    Term n2 = build_num(22);                                // cost 0
    Term n3 = term_new_inc(build_num(33));                  // cost 1
    Term s12 = build_sup(7, n1, n2);
    Term t   = build_sup(7, s12, n3);
    Term out[2] = {0};
    u64  n = thvm_collapse_ordered(t, out, 2);
    // The walk produces 3 leaves, but the buffer only holds 2.  The
    // implementation collects up to `cap` leaves in DFS order, then
    // sorts only those.  Since DFS visits 11 then 22 then 33, with
    // cap=2 we collect 11 (cost 2) and 22 (cost 0) and emit them
    // sorted: 22 (cost 0) then 11 (cost 2).
    CHECK_EQ(n, 2);
    CHECK_EQ(term_val(out[0]), 22);
    CHECK_EQ(term_val(out[1]), 11);
  }

  thvm_free();
  TEST_REPORT();
}
