// test_term.c — round-trip pack/unpack of the 64-bit Term encoding.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  TEST_BEGIN("term/zero");
  Term z = term_new(0, 0, 0, 0);
  CHECK_EQ(z, 0);
  CHECK_EQ(term_tag(z), 0);
  CHECK_EQ(term_ext(z), 0);
  CHECK_EQ(term_val(z), 0);
  CHECK_EQ(term_sub_get(z), 0);

  TEST_BEGIN("term/all-fields-roundtrip");
  Term t = term_new(1, TAG_DUP, 0x12345, 0x123456789ULL);
  CHECK_EQ(term_sub_get(t), 1);
  CHECK_EQ(term_tag(t),     TAG_DUP);
  CHECK_EQ(term_ext(t),     0x12345);
  CHECK_EQ(term_val(t),     0x123456789ULL);

  TEST_BEGIN("term/mask-overflow-truncates");
  // EXT is 18 bits; passing 0x40001 should keep only the low 18 bits (1).
  Term o = term_new(0, TAG_SUP, 0x40001, 0);
  CHECK_EQ(term_ext(o), 1);

  TEST_BEGIN("term/sub-flag-toggle");
  Term s0 = term_new(0, TAG_LAM, 7, 99);
  Term s1 = term_sub_set(s0, 1);
  Term s2 = term_sub_set(s1, 0);
  CHECK_EQ(term_sub_get(s0), 0);
  CHECK_EQ(term_sub_get(s1), 1);
  CHECK_EQ(term_sub_get(s2), 0);
  // Toggling SUB must not disturb the other fields.
  CHECK_EQ(term_tag(s1), TAG_LAM);
  CHECK_EQ(term_ext(s1), 7);
  CHECK_EQ(term_val(s1), 99);
  CHECK_EQ(s0, s2);

  TEST_BEGIN("term/each-tag-distinct");
  for (u8 a = 0; a < TAG_COUNT; a++) {
    for (u8 b = 0; b < TAG_COUNT; b++) {
      if (a == b) continue;
      CHECK(term_new(0, a, 0, 0) != term_new(0, b, 0, 0));
    }
  }

  TEST_REPORT();
}
