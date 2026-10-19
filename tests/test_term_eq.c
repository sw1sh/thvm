// test_term_eq.c -- structural equality (TTermSame) reads exactly a node's own
// cells: a one-cell DUP or BRI compares its one cell, not the cell after it.
#include "../src/thvm.c"
#include "test.h"

static Term num(u32 v) {
  return term_new(0, TAG_NUM, DT_INT32, v);
}

// A one-cell node holding `body`, followed by an unrelated cell `next`.
static Term one_cell(u8 tag, Term body, Term next) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, body);
  heap_set(loc + 1, next);
  return term_new(0, tag, 5, loc);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("term-eq/one-cell-nodes-compare-one-cell");
  {
    CHECK(term_eq_struct(one_cell(TAG_DUP, num(7), num(1)), one_cell(TAG_DUP, num(7), num(2))));
    CHECK(term_eq_struct(one_cell(TAG_BRI, num(7), num(1)), one_cell(TAG_BRI, num(7), num(2))));
    CHECK(!term_eq_struct(one_cell(TAG_DUP, num(7), num(1)), one_cell(TAG_DUP, num(8), num(1))));
  }

  thvm_free();
  TEST_REPORT();
}
