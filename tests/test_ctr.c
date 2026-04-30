// test_ctr.c - TAG_CTR labelled constructor: construct + read-back.
//
// k0a deliverable: a passive aggregate Term (HVM4 CTR) that holds N
// children at heap [NUM(arity), c_0, ..., c_{n-1}].  Consumers (the
// k0c multi-target interact_grad + k0d WL TGradMany bridge) read
// children via term_ctr_at.  CTR has no IC interaction rules yet
// (no DUP-CTR / ERA-CTR); it lives passively in the heap until a
// consumer reads it.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("ctr/tag-define");
  CHECK_EQ(TAG_CTR, 20);

  TEST_BEGIN("ctr/empty-ctr");
  Term empty = term_new_ctr(0, NULL, 0);
  CHECK_EQ(term_tag(empty), TAG_CTR);
  CHECK_EQ(term_ext(empty), 0u);
  CHECK_EQ(term_ctr_n(empty),     0u);
  CHECK_EQ(term_ctr_at(empty, 0), 0u);   // out-of-range -> 0

  TEST_BEGIN("ctr/three-children-roundtrip");
  Term a = term_new(0, TAG_NUM, DT_FP32, 0x40000000);   // 2.0f bits
  Term b = term_new(0, TAG_NUM, DT_FP32, 0x40400000);   // 3.0f bits
  Term c = term_new(0, TAG_NUM, DT_FP32, 0x40800000);   // 4.0f bits
  Term children[3] = {a, b, c};
  Term tup = term_new_ctr(0, children, 3);
  CHECK_EQ(term_tag(tup), TAG_CTR);
  CHECK_EQ(term_ext(tup), 0u);
  CHECK_EQ(term_ctr_n(tup),     3u);
  CHECK_EQ(term_ctr_at(tup, 0), a);
  CHECK_EQ(term_ctr_at(tup, 1), b);
  CHECK_EQ(term_ctr_at(tup, 2), c);
  CHECK_EQ(term_ctr_at(tup, 3), 0u);     // out-of-range
  CHECK_EQ(term_ctr_at(tup, 99), 0u);

  TEST_BEGIN("ctr/labelled-ctr-preserves-ext");
  // Label rides in the EXT field.  Anonymous tuples use 0; named
  // sums (Pair, Either, etc.) use a small integer id.  Future
  // DUP-CTR / MAT-CTR rules dispatch on this label.
  Term labelled = term_new_ctr(7, children, 2);
  CHECK_EQ(term_tag(labelled), TAG_CTR);
  CHECK_EQ(term_ext(labelled), 7u);
  CHECK_EQ(term_ctr_n(labelled),     2u);
  CHECK_EQ(term_ctr_at(labelled, 0), a);
  CHECK_EQ(term_ctr_at(labelled, 1), b);

  TEST_BEGIN("ctr/non-ctr-input-returns-zero");
  // Defensive: passing a non-CTR term returns 0/0 instead of
  // dereferencing garbage.
  Term not_a_ctr = term_new(0, TAG_NUM, DT_FP32, 42);
  CHECK_EQ(term_ctr_n(not_a_ctr),     0u);
  CHECK_EQ(term_ctr_at(not_a_ctr, 0), 0u);

  Term raw_uop = term_new(0, TAG_UOP, UOP_NEG, 0);
  CHECK_EQ(term_ctr_n(raw_uop),     0u);
  CHECK_EQ(term_ctr_at(raw_uop, 0), 0u);

  thvm_free();
  TEST_REPORT();
}
