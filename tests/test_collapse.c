// test_collapse.c - thvm_collapse enumerates the SUP-tree of a term.
//
// Walks the head SUP via WNF.  TAG_SUP -> recurse into both halves;
// TAG_ERA -> drop the branch; otherwise -> emit the term.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("collapse/single-leaf-no-sup");
  {
    Term out[4] = {0};
    Term t = build_num(42);
    u64 n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 1);
    CHECK_EQ(term_tag(out[0]), TAG_NUM);
    CHECK_EQ(term_val(out[0]), 42);
  }

  TEST_BEGIN("collapse/single-sup-two-leaves");
  {
    Term out[4] = {0};
    Term sup = build_sup(7, build_num(2), build_num(3));
    u64 n = thvm_collapse(sup, out, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_tag(out[0]), TAG_NUM);
    CHECK_EQ(term_val(out[0]), 2);
    CHECK_EQ(term_tag(out[1]), TAG_NUM);
    CHECK_EQ(term_val(out[1]), 3);
  }

  TEST_BEGIN("collapse/nested-sup-three-leaves");
  {
    // &7{ NUM(1), &7{ NUM(2), NUM(3) } }
    Term out[4] = {0};
    Term inner = build_sup(7, build_num(2), build_num(3));
    Term outer = build_sup(7, build_num(1), inner);
    u64 n = thvm_collapse(outer, out, 4);
    CHECK_EQ(n, 3);
    CHECK_EQ(term_val(out[0]), 1);
    CHECK_EQ(term_val(out[1]), 2);
    CHECK_EQ(term_val(out[2]), 3);
  }

  TEST_BEGIN("collapse/era-branch-dropped");
  {
    // &7{ ERA, NUM(5) }  -- failed left branch disappears.
    Term out[4] = {0};
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term sup = build_sup(7, era, build_num(5));
    u64 n = thvm_collapse(sup, out, 4);
    CHECK_EQ(n, 1);
    CHECK_EQ(term_val(out[0]), 5);
  }

  TEST_BEGIN("collapse/cap-truncates");
  {
    // 4 leaves but cap=2 -- only first two emitted.
    Term out[4] = {0};
    Term s00 = build_sup(7, build_num(10), build_num(11));
    Term s01 = build_sup(7, build_num(12), build_num(13));
    Term s   = build_sup(7, s00, s01);
    u64 n = thvm_collapse(s, out, 2);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_val(out[0]), 10);
    CHECK_EQ(term_val(out[1]), 11);
  }

  thvm_free();
  TEST_REPORT();
}
